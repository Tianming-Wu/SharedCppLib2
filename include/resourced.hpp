/*
    resourced — where a program's resources live, and how they are found.

    A "resource" is a named blob of bytes that the program needs but did not compute: an icon, a
    language pack, a shader, a template, a payload for an installer. This module answers one
    question — given a name, where do the bytes come from — and leaves what they mean to the caller.

    Sources are layered. A manager holds a list of mounted sources, each with a name of its own, and
    a lookup walks them until one has the resource:

        scl2::ResourceManager res;
        res.mount("builtin", builtin_pack);          // shipped inside the binary
        res.mount_pack_file("lang", "zh_CN.pack");   // an optional pack next to the exe
        res.mount_directory("override", "res/");     // a loose file, for development

        if (auto ico = res.view("app.ico"))
            scl2::set_window_icon(hwnd, scl2::to_hicon(*ico));

    Later mounts are searched first, so a directory mounted last overrides everything the program
    shipped with — that is the arrangement a language pack or a development override wants.

    Four kinds of source, matching ResourceType:
      - an owned ResourcePack, or a non-owning ResourceView over a blob that is already in memory
        (an embedded resource, or a payload appended to the executable) — ResourceType::Embedded
      - a directory of ordinary files, read on demand                        — ExternalFile
      - a ResourcePack stored in a file                                      — ExternalPacked

    ResourceType::PlatformEmbedded and ResourceType::SystemResource are reserved for a source that
    asks the platform itself (a Windows resource section, a macOS bundle, a system font). Nothing
    implements them yet, so nothing here produces them.

    ResourcePack is a container: a directory of names and lengths, followed by the data, in the same
    order. It is deliberately not compressed and not encrypted — the Encryption API takes a
    bytearray and this is a bytearray, so a caller who wants either can have it without this module
    knowing about it.

    ResourceView parses the same format without copying: it keeps the span it was given and hands
    out a bytearray_view for each entry. That is what makes a blob in read-only memory usable as it
    stands, which is the state an embedded resource is in.

    On Windows the module also turns bytes into platform objects: to_hicon() builds a real icon
    straight from memory, so a window can be given an icon without an .rc file. The icon Explorer
    shows for the executable itself is a different thing and is not covered: it lives in the PE
    resource section and only a resource compiler can set it.

    Cursors are deliberately not here: CreateIconFromResourceEx() builds icons only, and a cursor
    needs CreateIconIndirect() fed two bitmaps split apart by hand. That is a separate piece of
    work, not a flag on this one.
*/

#pragma once

#include "orderedmap.hpp"
#include "bytearray.hpp"
#include "string.hpp"
#include "singleinst.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <istream>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// No <windows.h> here, on purpose: it defines `small`, `near` and `far` as macros, and pulling it
// into a header would put those into every translation unit that uses this module. The Windows
// calls that need real handles take and return them as plain pointers, which an HWND or an HICON
// converts to and from without a cast.
#if defined(_WIN32) || defined(_WIN64)
    #define SCL2_RESOURCED_WINDOWS 1
#else
    #define SCL2_RESOURCED_WINDOWS 0
#endif


namespace scl2 {

/// @brief Where the bytes of a resource actually come from.
/// @note The two reserved values are not produced by anything here yet. They exist so a caller can
///       test for them once a platform source is added.
enum class ResourceType {
    Null,             // Not known, or not looked up yet.
    Embedded,         // Held in memory: an owned pack, or a view over a blob in the binary.
    PlatformEmbedded, // Reserved — a platform resource section, a macOS bundle.
    ExternalFile,     // A loose file on disk, read on demand.
    ExternalPacked,   // A pack file on disk, holding many resources.
    SystemResource    // Reserved — something the platform provides (a system font, a stock icon).
};

/// @brief What is known about one resource, without reading it.
/// @note Names arrive as a std::string_view, so a literal, a std::string, a scl2::string or a
///       std::string_view all pass without a conversion at the call site.
struct ResourceInfo {
    scl2::string name;                       // the name it answers to
    scl2::string source;                     // which mounted source provides it
    ResourceType type = ResourceType::Null;  // how that source keeps its bytes
    std::uint64_t offset = 0;                // where the data starts inside its container
    std::uint64_t size = 0;                  // how long it is
};

/// @brief A container of named resources, owned.
///
/// Layout, little-endian, version 1:
///
///     magic    8 bytes   "SCL2RES\0"
///     version  u32       1
///     count    u32       number of resources
///     directory          count entries, in payload order:
///                            name_len u32, name bytes (no terminator), size u64
///     payload            the data of every entry, in directory order
///
/// Offsets are not stored: an entry starts where the previous one ended, so writing takes one pass
/// and the directory stays small.
class ResourcePack {
public:
    static constexpr std::uint32_t format_version = 1;

    ResourcePack() = default;

    // ── Looking at it ────────────────────────────────────────────────
    bool empty() const noexcept { return entries_.empty(); }
    std::size_t size() const noexcept { return entries_.size(); }

    /// @brief Whether a name is in the pack.
    bool contains(std::string_view name) const { return entries_.contains(scl2::string{ name }); }

    /// @brief Every name, in pack order.
    std::vector<scl2::string> names() const;

    /// @brief The bytes of one resource, without copying — they live in this pack.
    /// @return Nothing when there is no such name.
    std::optional<scl2::bytearray_view> view(std::string_view name) const;

    /// @brief The bytes of one resource, copied out.
    std::optional<scl2::bytearray> get(std::string_view name) const;

    std::optional<ResourceInfo> info(std::string_view name) const;

    // ── Building it ──────────────────────────────────────────────────
    /// @brief Add a resource at the end.
    /// @return false when the name is already taken — use replace() to overwrite.
    bool add(std::string_view name, const scl2::bytearray& data);

    /// @brief Add a resource, or overwrite one of the same name without moving it.
    /// @return true when an existing entry was replaced.
    bool replace(std::string_view name, const scl2::bytearray& data);

    bool erase(std::string_view name);

    void clear() { entries_.clear(); }

    // ── Serializing it ───────────────────────────────────────────────
    scl2::bytearray dump() const;

    void dump_to(std::ostream& out) const;

    // ── Parsing it ───────────────────────────────────────────────────
    /// @brief Read a pack from a bytearray, starting at its read cursor — the habit the library's
    ///        other readers follow (the cursor is mutable, so this works on a const bytearray).
    /// @throw std::runtime_error when the data is not a pack, or is truncated.
    void load(const scl2::bytearray& data);

    /// @brief Read a pack from a stream, which may be a file.
    /// @throw std::runtime_error when the data is not a pack, or is truncated.
    void load(std::istream& in);

private:
    using table_type = scl2::ordered_map<scl2::string, scl2::bytearray>;
    table_type entries_;
};

/// @brief A pack seen inside memory that someone else owns: parses the directory, copies nothing.
///
/// This is the reader for a blob that is already in the binary — a generated array, a payload
/// appended to the executable, a mapped file. The bytes handed out point into the blob, so the blob
/// must outlive the view and must not move. Like bytearray_view, the class refuses to be built from
/// a temporary.
class ResourceView {
public:
    ResourceView() = default;

    /// @brief Parse a pack out of a blob, without copying it.
    /// @return Nothing when the blob is not a pack, or its directory runs past the end.
    static std::optional<ResourceView> parse(std::span<const std::byte> blob) noexcept;

    bool empty() const noexcept { return catalog_.empty(); }
    std::size_t size() const noexcept { return catalog_.size(); }
    bool contains(std::string_view name) const;

    /// @brief Every name, in pack order.
    std::vector<scl2::string> names() const;

    /// @brief The bytes of one resource — a view into the blob this was parsed from.
    std::optional<scl2::bytearray_view> view(std::string_view name) const;

    /// @brief The bytes of one resource, copied out.
    std::optional<scl2::bytearray> get(std::string_view name) const;

    std::optional<ResourceInfo> info(std::string_view name) const;

    /// @brief The blob this view was parsed from.
    std::span<const std::byte> blob() const noexcept { return blob_; }

private:
    std::span<const std::byte> blob_{};
    std::vector<ResourceInfo> catalog_;    // in pack order
    std::vector<std::uint32_t> by_name_;   // positions in catalog_, ordered by name
};

/// @brief A list of mounted sources, searched from the last mounted to the first.
///
/// A manager registers itself while it lives, and the static calls below forward to it. That is what
/// lets the bytes be asked for from anywhere without passing a manager around. Construct one in
/// `main` and let it live as long as the program does.
class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager();

    // One registry per program: a manager registers itself while it lives, and the calls below
    // forward to it. A copy would be a second manager that nothing forwards to, so copying is not
    // allowed.
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    SINGLE_INSTANCE(ResourceManager)

    // ── Mounting ─────────────────────────────────────────────────────
    /// @brief Mount a pack the manager owns.
    /// @return false when a source of that name is already mounted.
    si_static_access(mount, _mount)

    /// @brief Mount a directory of ordinary files. A resource name is the path inside it with
    ///        forward slashes, so from a directory holding res/, the file res/a/b.txt is asked for
    ///        as "a/b.txt".
    /// @return false when the directory does not exist, or the source name is taken.
    si_static_access(mount_directory, _mount_directory)

    /// @brief Mount a pack stored in a file. Only the directory is read while mounting; an entry
    ///        is read from the file when it is asked for.
    /// @return false when the file cannot be read as a pack, or the source name is taken.
    si_static_access(mount_pack_file, _mount_pack_file)

    /// @brief Remove a mounted source, and everything it was answering for.
    si_static_access(unmount, _unmount)

    /// @brief Remove every mounted source.
    si_static_access(clear, _clear)

    // ── Asking ───────────────────────────────────────────────────────
    /// @brief Whether any mounted source answers to the name.
    si_static_access(contains, _contains)

    /// @brief The bytes of one resource, without copying.
    /// @return Nothing when no source has it, or when the source that has it cannot hand out a view
    ///         (a loose file has to be read first — ask get() for those).
    si_static_access(view, _view)

    /// @brief The bytes of one resource, copied out. Works for every kind of source.
    si_static_access(get, _get)

    /// @brief Where a name resolves, and to what.
    si_static_access(info, _info)

    /// @brief Every name from every source, each reported once, as the search would find them.
    si_static_access(names, _names)

    /// @brief The mounted source names, in mount order.
    si_static_access(sources, _sources)

protected:
    // The per-instance work behind the names above. A caller that holds its own manager and wants
    // to reach it without going through the registry derives from this class.

    bool _mount(std::string_view source, ResourcePack pack);

    /// @param view Has to keep pointing at live, unmoved memory for as long as it stays mounted.
    bool _mount(std::string_view source, ResourceView view);

    bool _mount_directory(std::string_view source, const std::filesystem::path& directory,
                          bool recursive = true);
    bool _mount_pack_file(std::string_view source, const std::filesystem::path& file);
    bool _unmount(std::string_view source);
    void _clear();

    bool _contains(std::string_view name) const;
    std::optional<scl2::bytearray_view> _view(std::string_view name) const;
    std::optional<scl2::bytearray> _get(std::string_view name) const;
    std::optional<ResourceInfo> _info(std::string_view name) const;
    std::vector<scl2::string> _names() const;
    std::vector<scl2::string> _sources() const;

private:
    struct DirectorySource {
        std::filesystem::path root;
        bool recursive = true;
    };

    // A pack left in a file. Only its directory is held; each entry is read from the file on
    // demand, so a pack too large to fit in memory never has to be loaded. `catalog` offsets are
    // absolute file offsets, which is why this cannot be a ResourceView.
    struct PackedFileSource {
        std::filesystem::path file;
        std::vector<ResourceInfo> catalog;    // in pack order, offsets absolute in the file
        std::vector<std::uint32_t> by_name;   // positions in catalog, ordered by name
    };

    // An owned pack and a borrowed view are both "in memory"; a directory and a pack file are kept
    // as the path they came from.
    using source_body = std::variant<ResourcePack, ResourceView, DirectorySource, PackedFileSource>;

    struct source_type {
        scl2::string name;
        ResourceType type = ResourceType::Null;
        source_body body;
    };

    /// @brief The last mounted source holding the name, or nullptr.
    const source_type* find_source(std::string_view name) const;

    static bool holds(const source_body& body, std::string_view name);
    static std::vector<scl2::string> list(const source_body& body);
    static std::optional<scl2::bytearray> read_from(const source_body& body, std::string_view name);

    /// @brief Open a pack file and read its directory, in one call: open, identify, read, and hand
    ///        back the table. Nothing else of the file is touched.
    static std::optional<source_body> open_packed_file(const std::filesystem::path& file);

    std::vector<source_type> sources_;  // front = mounted first = searched last
};

/// @brief The default registry: the manager the program is using, reached by name from anywhere.
///
/// Construct one `ResourceManager` in `main` and this namespace forwards to it, so the bytes can be
/// asked for without passing the manager down every call. A program that needs several independent
/// registries uses `ResourceManager` directly instead.
namespace res {

/// @brief The manager the calls here forward to.
/// @throws std::logic_error when no `ResourceManager` exists — asking before one was constructed is
///         a mistake in the program, not a missing resource, so it does not come back as an empty
///         answer.
ResourceManager& manager();

/// @brief Whether a `ResourceManager` exists.
bool has_manager() noexcept;

/// @brief Parse a pack and mount it, in one call.
/// @param pack The bytes of a pack, for instance what a build step generated.
/// @param source The name to mount it under. The later a source is mounted, the earlier it is
///               searched.
/// @return false when the bytes are not a pack, or the source name is taken.
bool mount_builtin(std::span<const std::byte> pack, std::string_view source = "builtin");

bool mount(std::string_view source, ResourcePack pack);
bool mount(std::string_view source, ResourceView view);
bool mount_directory(std::string_view source, const std::filesystem::path& directory,
                     bool recursive = true);
bool mount_pack_file(std::string_view source, const std::filesystem::path& file);
bool unmount(std::string_view source);

bool contains(std::string_view name);
std::optional<scl2::bytearray> get(std::string_view name);
std::optional<scl2::bytearray_view> view(std::string_view name);
std::optional<ResourceInfo> info(std::string_view name);
std::vector<scl2::string> names();
std::vector<scl2::string> sources();

/// @brief The bytes as text.
/// @note Whether they are text at all is the caller's business; this only copies them into a
///       `std::string`.
std::optional<std::string> text(std::string_view name);

} // namespace res

#if SCL2_RESOURCED_WINDOWS

/// @brief Build a Windows icon out of bytes held in memory.
///
/// No .rc file, no resource section and no file on disk take part: the bytes go straight to
/// CreateIconFromResourceEx(), which is what lets an icon come from somewhere other than the
/// executable's own resource section. Either a complete .ico (an ICONDIR, as it would be in a file)
/// or a single icon image block is accepted, and a multi-size .ico is asked for the size wanted.
///
/// @param image The .ico, or the image bytes.
/// @param cx,cy The size wanted. 0 means "leave it as it is".
/// @return A live HICON, or nullptr on failure. The caller owns it, and passes it to DestroyIcon().
void* to_hicon(const scl2::bytearray_view& image, int cx = 0, int cy = 0);

/// @brief Put an icon on a window, for the title bar and for the taskbar button.
/// @note The icon is not owned here: it has to outlive the windows showing it.
bool set_window_icon(void* hwnd, void* icon);

#endif // SCL2_RESOURCED_WINDOWS


scl2_check_generic_dump_load(ResourcePack)


} // namespace scl2
