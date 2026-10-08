/*
    resourced implementation.
*/
#include "resourced.hpp"

// <windows.h> is included here and not in the header: it defines `small`, `near` and `far` as
// macros, and a header that pulled it in would put those into every translation unit that uses this
// module. Its own names do not escape this file.
#if SCL2_RESOURCED_WINDOWS
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <spanstream>
#include <stdexcept>
#include <system_error>
#include <unordered_set>

namespace scl2 {

namespace {

constexpr char k_pack_magic[8] = { 'S', 'C', 'L', '2', 'R', 'E', 'S', '\0' };

// A longer name than this is not something this library wrote, so the file is treated as broken
// instead of being trusted into a large allocation.
constexpr std::uint32_t k_max_name_length = 4096;

// ── Little-endian, over a stream ─────────────────────────────────────────
//
// The pack is a file format, so its integers are written out byte by byte rather than by
// memcpy'ing native integers: a pack stays readable on a machine of the other endianness.

void put_bytes(std::ostream& out, const void* data, std::size_t size) {
    if (size) out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    if (!out) throw std::runtime_error("resourced: writing the pack failed");
}

void put_u32(std::ostream& out, std::uint32_t value) {
    const std::uint8_t bytes[4] = {
        static_cast<std::uint8_t>(value & 0xFF),
        static_cast<std::uint8_t>((value >> 8) & 0xFF),
        static_cast<std::uint8_t>((value >> 16) & 0xFF),
        static_cast<std::uint8_t>((value >> 24) & 0xFF),
    };
    put_bytes(out, bytes, 4);
}

void put_u64(std::ostream& out, std::uint64_t value) {
    std::uint8_t bytes[8];
    for (int i = 0; i < 8; ++i) bytes[i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF);
    put_bytes(out, bytes, 8);
}

void get_bytes(std::istream& in, void* data, std::size_t size) {
    if (!size) return;
    in.read(static_cast<char*>(data), static_cast<std::streamsize>(size));
    if (static_cast<std::size_t>(in.gcount()) != size)
        throw std::runtime_error("resourced: the pack ends early");
}

std::uint32_t get_u32(std::istream& in) {
    std::uint8_t bytes[4];
    get_bytes(in, bytes, 4);
    return static_cast<std::uint32_t>(bytes[0])
         | (static_cast<std::uint32_t>(bytes[1]) << 8)
         | (static_cast<std::uint32_t>(bytes[2]) << 16)
         | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::uint64_t get_u64(std::istream& in) {
    std::uint8_t bytes[8];
    get_bytes(in, bytes, 8);
    std::uint64_t value = 0;
    for (int i = 7; i >= 0; --i) value = (value << 8) | bytes[i];
    return value;
}

// ── Little-endian, over a blob ───────────────────────────────────────────

std::uint8_t at(const std::byte* p) { return std::to_integer<std::uint8_t>(p[0]); }

std::uint32_t blob_u32(const std::byte* p) {
    return static_cast<std::uint32_t>(at(p))
         | (static_cast<std::uint32_t>(at(p + 1)) << 8)
         | (static_cast<std::uint32_t>(at(p + 2)) << 16)
         | (static_cast<std::uint32_t>(at(p + 3)) << 24);
}

std::uint64_t blob_u64(const std::byte* p) {
    std::uint64_t value = 0;
    for (int i = 7; i >= 0; --i) value = (value << 8) | at(p + i);
    return value;
}

// ── Directory sources ────────────────────────────────────────────────────

// A name asked of a directory source has to stay inside it. Without this, "..\\..\\x" would read
// any file on the machine through a resource name, which is not a resource lookup any more.
bool join_within(const std::filesystem::path& root, std::string_view name,
                 std::filesystem::path& out) {
    if (name.empty()) return false;

    const std::filesystem::path relative{ std::string(name) };
    if (relative.is_absolute()) return false;
    if (relative.has_root_name() || relative.has_root_directory()) return false;

    for (const auto& part : relative) {
        const std::string text = part.string();
        if (text.empty() || text == "." || text == "..") return false;
    }

    out = root / relative;
    return true;
}

std::vector<scl2::string> scan_directory(const std::filesystem::path& root, bool recursive) {
    std::vector<scl2::string> names;

    std::error_code ec;
    const auto options = std::filesystem::directory_options::skip_permission_denied;

    auto add = [&](const std::filesystem::directory_entry& entry) {
        if (!entry.is_regular_file(ec) || ec) return;
        // Names use forward slashes, so a pack written on one platform reads the same on another.
        names.push_back(entry.path().lexically_relative(root).generic_string());
    };

    if (recursive) {
        std::filesystem::recursive_directory_iterator it{ root, options, ec };
        const std::filesystem::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) add(*it);
    } else {
        std::filesystem::directory_iterator it{ root, options, ec };
        const std::filesystem::directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) add(*it);
    }

    return names;
}

// ── Reading a file by range, and finding a name without walking the catalog ──

// Open, seek, read, close — one call, one handle. Opening per call is what makes a source built on
// this thread-safe without a lock: there is no cursor to share.
//
// Fewer bytes than asked for come back when the file ends early, which is a signal the caller
// uses; nothing here treats a short read as an error.
std::optional<scl2::bytearray> read_file_range(const std::filesystem::path& path,
                                               std::uint64_t offset, std::size_t length)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;

    if (offset > 0) {
        in.seekg(static_cast<std::streamoff>(offset));
        if (!in) return std::nullopt;
    }

    scl2::bytearray data(length);
    if (length) {
        in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(length));
        const std::streamsize got = in.gcount();
        if (got < 0) return std::nullopt;
        data.resize(static_cast<std::size_t>(got));
    }
    return data;
}

// The catalog is in pack order; this is the name order, so a lookup is a binary search rather than
// a walk. It holds positions and not names, so it neither owns nor points at any storage — which is
// what lets it survive the container it lives in being moved.
std::vector<std::uint32_t> make_name_index(const std::vector<ResourceInfo>& catalog)
{
    std::vector<std::uint32_t> by_name(catalog.size());
    for (std::uint32_t i = 0; i < by_name.size(); ++i) by_name[i] = i;

    // Stable, so among equal names the one earliest in the pack is the one found.
    std::stable_sort(by_name.begin(), by_name.end(),
                     [&catalog](std::uint32_t a, std::uint32_t b) {
                         return std::string_view(catalog[a].name) < std::string_view(catalog[b].name);
                     });
    return by_name;
}

const ResourceInfo* find_in(const std::vector<ResourceInfo>& catalog,
                            const std::vector<std::uint32_t>& by_name, std::string_view name)
{
    const auto it = std::lower_bound(by_name.begin(), by_name.end(), name,
                                     [&catalog](std::uint32_t position, std::string_view wanted) {
                                         return std::string_view(catalog[position].name) < wanted;
                                     });
    if (it == by_name.end()) return nullptr;

    const ResourceInfo& entry = catalog[*it];
    return std::string_view(entry.name) == name ? &entry : nullptr;
}

// Parse a directory out of the bytes that follow the 16 byte header.
//
// Nothing when those bytes end before the directory does — the caller's signal to read more. A
// directory that is malformed rather than short throws.
std::optional<std::vector<ResourceInfo>> parse_directory(std::span<const std::byte> buffer,
                                                        std::uint32_t count)
{
    std::vector<ResourceInfo> catalog;
    std::size_t cursor = 0;

    for (std::uint32_t i = 0; i < count; ++i) {
        if (cursor + 4 > buffer.size()) return std::nullopt;
        const std::uint32_t name_length = blob_u32(buffer.data() + cursor);
        cursor += 4;

        if (name_length > k_max_name_length)
            throw std::runtime_error("resourced: resource name is too long");
        if (cursor + name_length + 8 > buffer.size()) return std::nullopt;

        ResourceInfo entry;
        entry.name.assign(reinterpret_cast<const char*>(buffer.data() + cursor), name_length);
        cursor += name_length;

        entry.size = blob_u64(buffer.data() + cursor);
        cursor += 8;
        entry.type = ResourceType::Embedded;

        catalog.push_back(std::move(entry));
    }

    return catalog;
}

} // namespace


// ═══════════════════════════════════════════════════════════════════════════
//  ResourcePack
// ═══════════════════════════════════════════════════════════════════════════

std::vector<scl2::string> ResourcePack::names() const {
    std::vector<scl2::string> out;
    out.reserve(entries_.size());
    for (const auto& entry : entries_) out.push_back(entry.first);
    return out;
}

std::optional<scl2::bytearray_view> ResourcePack::view(std::string_view name) const {
    const auto it = entries_.find(scl2::string{ name });
    if (it == entries_.end()) return std::nullopt;
    return scl2::bytearray_view(it->second.data(), it->second.size());
}

std::optional<scl2::bytearray> ResourcePack::get(std::string_view name) const {
    const auto it = entries_.find(scl2::string{ name });
    if (it == entries_.end()) return std::nullopt;
    return it->second;
}

std::optional<ResourceInfo> ResourcePack::info(std::string_view name) const {
    const auto it = entries_.find(scl2::string{ name });
    if (it == entries_.end()) return std::nullopt;

    std::uint64_t directory = 0;
    for (const auto& entry : entries_) directory += 4 + entry.first.size() + 8;

    std::uint64_t offset = 8 + 4 + 4 + directory;
    for (const auto& entry : entries_) {
        if (std::string_view(entry.first) == name) break;
        offset += entry.second.size();
    }

    ResourceInfo out;
    out.name = scl2::string{ name };
    out.type = ResourceType::Embedded;
    out.offset = offset;
    out.size = it->second.size();
    return out;
}

bool ResourcePack::add(std::string_view name, const scl2::bytearray& data) {
    if (name.empty())
        throw std::invalid_argument("resourced: a resource name cannot be empty");
    if (name.size() > k_max_name_length)
        throw std::invalid_argument("resourced: the resource name is too long");
    return entries_.insert(scl2::string{ name }, data).second;
}

bool ResourcePack::replace(std::string_view name, const scl2::bytearray& data) {
    if (name.empty())
        throw std::invalid_argument("resourced: a resource name cannot be empty");
    if (name.size() > k_max_name_length)
        throw std::invalid_argument("resourced: the resource name is too long");
    // insert_or_assign keeps the position of an existing entry, so replacing does not move it.
    return !entries_.insert_or_assign(scl2::string{ name }, data).second;
}

bool ResourcePack::erase(std::string_view name) { return entries_.erase(scl2::string{ name }); }

scl2::bytearray ResourcePack::dump() const {
    std::size_t total = 8 + 4 + 4;
    for (const auto& entry : entries_) total += 4 + entry.first.size() + 8 + entry.second.size();

    scl2::bytearray out(total);
    std::ospanstream stream(std::span<char>(reinterpret_cast<char*>(out.data()), total));
    dump_to(stream);
    return out;
}

void ResourcePack::dump_to(std::ostream& out) const {
    if (entries_.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("resourced: too many resources for one pack");

    put_bytes(out, k_pack_magic, sizeof(k_pack_magic));
    put_u32(out, format_version);
    put_u32(out, static_cast<std::uint32_t>(entries_.size()));

    for (const auto& entry : entries_) {
        put_u32(out, static_cast<std::uint32_t>(entry.first.size()));
        put_bytes(out, entry.first.data(), entry.first.size());
        put_u64(out, entry.second.size());
    }
    for (const auto& entry : entries_) {
        put_bytes(out, entry.second.data(), entry.second.size());
    }
}

void ResourcePack::load(std::istream& in) {
    char magic[sizeof(k_pack_magic)];
    get_bytes(in, magic, sizeof(magic));
    if (std::memcmp(magic, k_pack_magic, sizeof(magic)) != 0)
        throw std::runtime_error("resourced: not a resource pack");

    const std::uint32_t version = get_u32(in);
    if (version != format_version)
        throw std::runtime_error("resourced: unsupported resource pack version");

    const std::uint32_t count = get_u32(in);

    // The directory first, so the payload behind it can be read in one pass. Nothing is written
    // into this pack until the whole thing has been read, so a broken pack leaves it as it was.
    struct entry_header {
        std::string name;
        std::uint64_t size;
    };

    std::vector<entry_header> headers;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t name_length = get_u32(in);
        if (name_length > k_max_name_length)
            throw std::runtime_error("resourced: resource name is too long");

        std::string name(name_length, '\0');
        if (name_length) get_bytes(in, name.data(), name_length);

        headers.push_back({ std::move(name), get_u64(in) });
    }

    constexpr std::uint64_t largest_holdable =
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) / 2;

    table_type parsed;
    for (auto& header : headers) {
        if (header.size > largest_holdable)
            throw std::runtime_error("resourced: a resource is too large to hold in memory");

        scl2::bytearray data(static_cast<std::size_t>(header.size));
        if (header.size) get_bytes(in, data.data(), static_cast<std::size_t>(header.size));

        parsed.insert(scl2::string(header.name), std::move(data));
    }

    entries_ = std::move(parsed);
}

void ResourcePack::load(const scl2::bytearray& data) {
    const std::size_t start = data.tellr();
    if (start > data.size())
        throw std::runtime_error("resourced: the read cursor is past the end of the bytearray");

    std::ispanstream in(std::span<const char>(
        reinterpret_cast<const char*>(data.data() + start), data.size() - start));
    load(in);

    // Leave the caller's cursor just past the pack, the way the library's other readers do.
    const std::streampos consumed = in.tellg();
    if (consumed > 0) data.seekr(start + static_cast<std::size_t>(consumed));
}


// ═══════════════════════════════════════════════════════════════════════════
//  ResourceView
// ═══════════════════════════════════════════════════════════════════════════

std::optional<ResourceView> ResourceView::parse(std::span<const std::byte> blob) noexcept {
    try {
        if (blob.size() < 16) return std::nullopt;
        if (std::memcmp(blob.data(), k_pack_magic, sizeof(k_pack_magic)) != 0) return std::nullopt;

        const std::uint32_t version = blob_u32(blob.data() + 8);
        if (version != ResourcePack::format_version) return std::nullopt;
        const std::uint32_t count = blob_u32(blob.data() + 12);

        std::vector<ResourceInfo> catalog;
        catalog.reserve(std::min<std::uint32_t>(count, 1024));

        std::uint64_t directory = 0;
        std::size_t cursor = 16;
        for (std::uint32_t i = 0; i < count; ++i) {
            if (cursor + 4 > blob.size()) return std::nullopt;
            const std::uint32_t name_length = blob_u32(blob.data() + cursor);
            cursor += 4;

            if (name_length > k_max_name_length) return std::nullopt;
            if (cursor + name_length + 8 > blob.size()) return std::nullopt;

            ResourceInfo entry;
            entry.name.assign(reinterpret_cast<const char*>(blob.data() + cursor), name_length);
            cursor += name_length;

            entry.size = blob_u64(blob.data() + cursor);
            cursor += 8;
            entry.type = ResourceType::Embedded;

            catalog.push_back(std::move(entry));
            directory += 4 + name_length + 8;
        }

        // Offsets are not stored, so they are worked out here from the directory size.
        std::uint64_t offset = 16 + directory;
        for (auto& entry : catalog) {
            entry.offset = offset;
            offset += entry.size;
        }
        if (offset > blob.size()) return std::nullopt;

        ResourceView view;
        view.blob_ = blob;
        view.by_name_ = make_name_index(catalog);
        view.catalog_ = std::move(catalog);
        return view;
    } catch (...) {
        // parse() promises not to throw, and the only things that can throw here are allocations.
        return std::nullopt;
    }
}

bool ResourceView::contains(std::string_view name) const {
    return find_in(catalog_, by_name_, name) != nullptr;
}

std::vector<scl2::string> ResourceView::names() const {
    std::vector<scl2::string> out;
    out.reserve(catalog_.size());
    for (const auto& entry : catalog_) out.push_back(entry.name);
    return out;
}

std::optional<scl2::bytearray_view> ResourceView::view(std::string_view name) const {
    const ResourceInfo* entry = find_in(catalog_, by_name_, name);
    if (!entry) return std::nullopt;
    return scl2::bytearray_view(blob_.data() + entry->offset, static_cast<std::size_t>(entry->size));
}

std::optional<scl2::bytearray> ResourceView::get(std::string_view name) const {
    const auto found = view(name);
    if (!found) return std::nullopt;
    return scl2::bytearray(found->data(), found->size());
}

std::optional<ResourceInfo> ResourceView::info(std::string_view name) const {
    const ResourceInfo* entry = find_in(catalog_, by_name_, name);
    if (!entry) return std::nullopt;
    return *entry;
}


// ═══════════════════════════════════════════════════════════════════════════
//  ResourceManager
// ═══════════════════════════════════════════════════════════════════════════

bool ResourceManager::holds(const source_body& body, std::string_view name) {
    if (const auto* pack = std::get_if<ResourcePack>(&body)) return pack->contains(name);
    if (const auto* view = std::get_if<ResourceView>(&body)) return view->contains(name);
    if (const auto* packed = std::get_if<PackedFileSource>(&body))
        return find_in(packed->catalog, packed->by_name, name) != nullptr;

    const auto& directory = std::get<DirectorySource>(body);
    std::filesystem::path path;
    if (!join_within(directory.root, name, path)) return false;

    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec) && !ec;
}

std::vector<scl2::string> ResourceManager::list(const source_body& body) {
    if (const auto* pack = std::get_if<ResourcePack>(&body)) return pack->names();
    if (const auto* view = std::get_if<ResourceView>(&body)) return view->names();
    if (const auto* packed = std::get_if<PackedFileSource>(&body)) {
        std::vector<scl2::string> names;
        names.reserve(packed->catalog.size());
        for (const auto& entry : packed->catalog) names.push_back(entry.name);
        return names;
    }

    const auto& directory = std::get<DirectorySource>(body);
    return scan_directory(directory.root, directory.recursive);
}

std::optional<scl2::bytearray> ResourceManager::read_from(const source_body& body,
                                                          std::string_view name) {
    if (const auto* pack = std::get_if<ResourcePack>(&body)) return pack->get(name);
    if (const auto* view = std::get_if<ResourceView>(&body)) return view->get(name);

    if (const auto* packed = std::get_if<PackedFileSource>(&body)) {
        const ResourceInfo* entry = find_in(packed->catalog, packed->by_name, name);
        if (!entry) return std::nullopt;
        if (entry->size == 0) return scl2::bytearray{};
        // Read that one range out of the file; the rest of the pack is never touched.
        return read_file_range(packed->file, entry->offset, static_cast<std::size_t>(entry->size));
    }

    const auto& directory = std::get<DirectorySource>(body);
    std::filesystem::path path;
    if (!join_within(directory.root, name, path)) return std::nullopt;

    // The size comes from the file system, so the read asks for exactly that many bytes rather
    // than for "everything" — read_file_range sizes its buffer up front.
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) return std::nullopt;
    return read_file_range(path, 0, static_cast<std::size_t>(size));
}

std::optional<ResourceManager::source_body> ResourceManager::open_packed_file(
    const std::filesystem::path& file)
{
    // The header first: it says how many entries there are, and nothing else is read until that is
    // known.
    const auto header = read_file_range(file, 0, 16);
    if (!header || header->size() < 16) return std::nullopt;
    if (std::memcmp(header->data(), k_pack_magic, sizeof(k_pack_magic)) != 0) return std::nullopt;
    if (blob_u32(header->data() + 8) != ResourcePack::format_version) return std::nullopt;

    const std::uint32_t count = blob_u32(header->data() + 12);

    // The directory's length is not stored, so read a chunk and grow it until the whole directory
    // fits. A short read means the file ended inside the directory, which is a broken pack.
    constexpr std::size_t largest_chunk = 64u * 1024u * 1024u;
    std::size_t chunk = 64u * 1024u;
    std::optional<std::vector<ResourceInfo>> catalog;
    for (;;) {
        const auto buffer = read_file_range(file, 16, chunk);
        if (!buffer) return std::nullopt;

        catalog = parse_directory(*buffer, count);
        if (catalog) break;

        if (buffer->size() < chunk) return std::nullopt;
        if (chunk >= largest_chunk) return std::nullopt;
        chunk *= 4;
    }

    PackedFileSource packed;
    packed.file = file;
    packed.catalog = std::move(*catalog);
    packed.by_name = make_name_index(packed.catalog);

    // Offsets in the file: the payload starts where the directory ends.
    std::uint64_t directory = 0;
    for (const auto& entry : packed.catalog) directory += 4 + entry.name.size() + 8;

    std::uint64_t offset = 16 + directory;
    for (auto& entry : packed.catalog) {
        entry.offset = offset;
        offset += entry.size;
    }

    return source_body{ std::move(packed) };
}

const ResourceManager::source_type* ResourceManager::find_source(std::string_view name) const {
    // From the last mounted backwards: the newest mount is the one that gets to answer.
    for (auto it = sources_.rbegin(); it != sources_.rend(); ++it) {
        if (holds(it->body, name)) return &*it;
    }
    return nullptr;
}

ResourceManager::ResourceManager() {
    SINGLE_INSTANCE_ONCREATE
}

ResourceManager::~ResourceManager() {
    SINGLE_INSTANCE_ONDESTROY
}

SINGLE_INSTANCE_IMPL(ResourceManager)

bool ResourceManager::_mount(std::string_view source, ResourcePack pack) {
    for (const auto& mounted : sources_) {
        if (std::string_view(mounted.name) == source) return false;
    }
    sources_.push_back({ scl2::string{ source }, ResourceType::Embedded, std::move(pack) });
    return true;
}

bool ResourceManager::_mount(std::string_view source, ResourceView view) {
    for (const auto& mounted : sources_) {
        if (std::string_view(mounted.name) == source) return false;
    }
    sources_.push_back({ scl2::string{ source }, ResourceType::Embedded, std::move(view) });
    return true;
}

bool ResourceManager::_mount_directory(std::string_view source,
                                      const std::filesystem::path& directory, bool recursive) {
    for (const auto& mounted : sources_) {
        if (std::string_view(mounted.name) == source) return false;
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec) || ec) return false;

    DirectorySource body;
    body.root = std::filesystem::absolute(directory, ec);
    if (ec) body.root = directory;   // an absolute path could not be formed; keep what we were given
    body.recursive = recursive;

    sources_.push_back({ scl2::string{ source }, ResourceType::ExternalFile, std::move(body) });
    return true;
}

bool ResourceManager::_mount_pack_file(std::string_view source,
                                      const std::filesystem::path& file) {
    for (const auto& mounted : sources_) {
        if (std::string_view(mounted.name) == source) return false;
    }

    auto body = open_packed_file(file);
    if (!body) return false;

    sources_.push_back({ scl2::string{ source }, ResourceType::ExternalPacked, std::move(*body) });
    return true;
}

bool ResourceManager::_unmount(std::string_view source) {
    const auto it = std::find_if(sources_.begin(), sources_.end(),
                                 [source](const source_type& mounted) {
                                     return std::string_view(mounted.name) == source;
                                 });
    if (it == sources_.end()) return false;
    sources_.erase(it);
    return true;
}

void ResourceManager::_clear() { sources_.clear(); }

bool ResourceManager::_contains(std::string_view name) const {
    return find_source(name) != nullptr;
}

std::optional<scl2::bytearray_view> ResourceManager::_view(std::string_view name) const {
    const source_type* source = find_source(name);
    if (!source) return std::nullopt;

    if (const auto* pack = std::get_if<ResourcePack>(&source->body)) return pack->view(name);
    if (const auto* view = std::get_if<ResourceView>(&source->body)) return view->view(name);

    // A loose file is not in memory, so there is nothing here to hand out a view of.
    return std::nullopt;
}

std::optional<scl2::bytearray> ResourceManager::_get(std::string_view name) const {
    const source_type* source = find_source(name);
    if (!source) return std::nullopt;
    return read_from(source->body, name);
}

std::optional<ResourceInfo> ResourceManager::_info(std::string_view name) const {
    const source_type* source = find_source(name);
    if (!source) return std::nullopt;

    std::optional<ResourceInfo> found;
    if (const auto* pack = std::get_if<ResourcePack>(&source->body)) {
        found = pack->info(name);
    } else if (const auto* view = std::get_if<ResourceView>(&source->body)) {
        found = view->info(name);
    } else if (const auto* packed = std::get_if<PackedFileSource>(&source->body)) {
        const ResourceInfo* entry = find_in(packed->catalog, packed->by_name, name);
        if (!entry) return std::nullopt;
        found = *entry;
    } else {
        const auto& directory = std::get<DirectorySource>(source->body);
        std::filesystem::path path;
        if (!join_within(directory.root, name, path)) return std::nullopt;

        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec) return std::nullopt;

        ResourceInfo entry;
        entry.name = scl2::string{ name };
        entry.offset = 0;
        entry.size = size;
        found = std::move(entry);
    }
    if (!found) return std::nullopt;

    found->source = source->name;
    found->type = source->type;
    return found;
}

std::vector<scl2::string> ResourceManager::_names() const {
    std::vector<scl2::string> out;
    std::unordered_set<std::string> seen;

    // Walking backwards means a name is reported by the source that would answer for it.
    for (auto it = sources_.rbegin(); it != sources_.rend(); ++it) {
        for (auto& name : list(it->body)) {
            if (seen.insert(name).second) out.push_back(std::move(name));
        }
    }
    return out;
}

std::vector<scl2::string> ResourceManager::_sources() const {
    std::vector<scl2::string> out;
    out.reserve(sources_.size());
    for (const auto& source : sources_) out.push_back(source.name);
    return out;
}


// ═══════════════════════════════════════════════════════════════
//  The default registry
// ═══════════════════════════════════════════════════════════════

namespace res {

namespace {
// The forwarders below throw std::logic_error when no instance exists, and so does this. It is here
// as well because it can say what to construct, which a message naming only the call cannot.
void require_registry() {
    if (!ResourceManager::hasInstance()) {
        throw std::logic_error(
            "resourced: scl2::res needs a ResourceManager. Construct one in main() first, so it "
            "lives as long as the program does.");
    }
}
} // namespace

ResourceManager& manager() {
    require_registry();
    return *ResourceManager::instance();
}

bool has_manager() noexcept { return ResourceManager::hasInstance(); }

bool mount_builtin(std::span<const std::byte> pack, std::string_view source) {
    require_registry();
    const auto parsed = ResourceView::parse(pack);
    if (!parsed) return false;
    return ResourceManager::mount(source, *parsed);
}

bool mount(std::string_view source, ResourcePack pack) {
    require_registry();
    return ResourceManager::mount(source, std::move(pack));
}

bool mount(std::string_view source, ResourceView view) {
    require_registry();
    return ResourceManager::mount(source, view);
}

bool mount_directory(std::string_view source, const std::filesystem::path& directory,
                     bool recursive) {
    require_registry();
    return ResourceManager::mount_directory(source, directory, recursive);
}

bool mount_pack_file(std::string_view source, const std::filesystem::path& file) {
    require_registry();
    return ResourceManager::mount_pack_file(source, file);
}

bool unmount(std::string_view source) {
    require_registry();
    return ResourceManager::unmount(source);
}

bool contains(std::string_view name) {
    require_registry();
    return ResourceManager::contains(name);
}

std::optional<scl2::bytearray> get(std::string_view name) {
    require_registry();
    return ResourceManager::get(name);
}

std::optional<scl2::bytearray_view> view(std::string_view name) {
    require_registry();
    return ResourceManager::view(name);
}

std::optional<ResourceInfo> info(std::string_view name) {
    require_registry();
    return ResourceManager::info(name);
}

std::vector<scl2::string> names() {
    require_registry();
    return ResourceManager::names();
}

std::vector<scl2::string> sources() {
    require_registry();
    return ResourceManager::sources();
}

std::optional<std::string> text(std::string_view name) {
    require_registry();
    const auto bytes = ResourceManager::get(name);
    if (!bytes) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size());
}

} // namespace res


// ═══════════════════════════════════════════════════════════════════════════
//  Bytes into platform objects
// ═══════════════════════════════════════════════════════════════════════════

#if SCL2_RESOURCED_WINDOWS

namespace {

// An .ico or .cur begins with an ICONDIR: reserved (must be 0), then the type. Distinguishing it
// from a bare image block is what lets one entry point accept either.
constexpr std::uint16_t k_cursor_type = 2;

// The Windows calls take a mutable pointer even though they only read it, so the const has to come
// off somewhere — once here, rather than at every call site.
BYTE* as_mutable(const std::byte* data) noexcept {
    return const_cast<BYTE*>(reinterpret_cast<const BYTE*>(data));
}

bool is_icondir(const std::byte* data, std::size_t size) noexcept {
    if (size < 6) return false;
    const std::uint16_t reserved = static_cast<std::uint16_t>(at(data) | (at(data + 1) << 8));
    if (reserved != 0) return false;
    const std::uint16_t type = static_cast<std::uint16_t>(at(data + 2) | (at(data + 3) << 8));
    return type == 1 || type == k_cursor_type;
}

// Pick the frame of a whole .ico / .cur that fits the size asked for. The Windows search returns
// the offset of that frame inside the directory, which is not a resource id when the directory
// came from a file: that is the long-standing way this pair of functions is used.
struct frame {
    const std::byte* data = nullptr;
    std::size_t size = 0;
};

std::optional<frame> pick_frame(const std::byte* base, std::size_t total, bool icon, int cx, int cy) {
    const int id = LookupIconIdFromDirectoryEx(as_mutable(base), icon ? TRUE : FALSE, cx, cy,
                                              LR_DEFAULTCOLOR);
    if (id <= 0) return std::nullopt;

    const auto offset = static_cast<std::size_t>(id);
    if (offset >= total) return std::nullopt;

    // Read the directory entry whose image offset is that, to learn the frame's own length. It is
    // usually the last one, but nothing about the format says it has to be.
    std::size_t length = total - offset;
    if (total >= 6) {
        const std::uint16_t count = static_cast<std::uint16_t>(at(base + 4) | (at(base + 5) << 8));
        for (std::uint16_t i = 0; i < count; ++i) {
            const std::size_t entry = 6 + static_cast<std::size_t>(i) * 16;
            if (entry + 16 > total) break;
            if (blob_u32(base + entry + 12) == offset) {
                length = blob_u32(base + entry + 8);
                break;
            }
        }
    }
    if (length == 0 || offset + length > total) length = total - offset;

    return frame{ base + offset, length };
}

} // namespace

void* to_hicon(const scl2::bytearray_view& image, int cx, int cy) {
    if (image.empty()) return nullptr;

    const auto* base = reinterpret_cast<const std::byte*>(image.data());

    if (is_icondir(base, image.size())) {
        const auto chosen = pick_frame(base, image.size(), /*icon*/ true, cx, cy);
        if (!chosen) return nullptr;
        return CreateIconFromResourceEx(as_mutable(chosen->data),
                                        static_cast<DWORD>(chosen->size),
                                        TRUE, 0x00030000, cx, cy, LR_DEFAULTCOLOR);
    }

    return CreateIconFromResourceEx(as_mutable(base), static_cast<DWORD>(image.size()),
                                    TRUE, 0x00030000, cx, cy, LR_DEFAULTCOLOR);
}

bool set_window_icon(void* hwnd, void* icon) {
    if (!hwnd || !icon) return false;

    const auto window = static_cast<HWND>(hwnd);
    const auto handle = static_cast<HICON>(icon);

    SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(handle));
    SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(handle));
    return true;
}

#endif // SCL2_RESOURCED_WINDOWS


} // namespace scl2
