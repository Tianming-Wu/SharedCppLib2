#include "settings.hpp"

#include "base64.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

namespace scl2 {

namespace {

// ── keys ────────────────────────────────────────────────────────────────

bool is_digits(const std::string& text)
{
    return !text.empty() && std::all_of(text.begin(), text.end(),
        [](unsigned char c) { return std::isdigit(c) != 0; });
}

bool parse_index(const std::string& text, std::size_t& out)
{
    if (!is_digits(text)) return false;
    out = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc() && result.ptr == text.data() + text.size();
}

/// "/a/b" and "a/b" both give {"a", "b"}. Empty segments are skipped, so "a//b" is "a/b".
std::vector<std::string> split_key(const std::string& key)
{
    std::vector<std::string> segments;
    std::string current;
    for (const char c : key) {
        if (c != '/') {
            current += c;
            continue;
        }
        if (!current.empty()) {
            segments.push_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) segments.push_back(std::move(current));
    return segments;
}

std::string join_key(const std::vector<std::string>& segments, std::size_t from)
{
    std::string text;
    for (std::size_t i = from; i < segments.size(); ++i) {
        if (!text.empty()) text += '/';
        text += segments[i];
    }
    return text;
}

std::vector<std::string> key_segments(const std::string& key)
{
    std::vector<std::string> segments = split_key(key);
    if (segments.empty()) throw std::invalid_argument("settings: an empty key names nothing");
    return segments;
}

settings_provider provider_from_extension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (extension == ".toml") return settings_provider::toml;
    if (extension == ".ini") return settings_provider::ini;
    if (extension == ".xkeydb" || extension == ".xkdb" || extension == ".db") return settings_provider::xkeydb;
    return settings_provider::json;
}

// ── text and numbers ────────────────────────────────────────────────────

std::string format_double(double value)
{
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (result.ec != std::errc()) return std::to_string(value);
    return std::string(buffer, result.ptr);
}

std::string_view trim(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) text.remove_suffix(1);
    return text;
}

bool iequals(std::string_view a, std::string_view b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto left = std::tolower(static_cast<unsigned char>(a[i]));
        const auto right = std::tolower(static_cast<unsigned char>(b[i]));
        if (left != right) return false;
    }
    return true;
}

bool parse_int64(std::string_view text, std::int64_t& out)
{
    text = trim(text);
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc() && result.ptr == text.data() + text.size();
}

bool parse_double(std::string_view text, double& out)
{
    text = trim(text);
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc() && result.ptr == text.data() + text.size();
}

// ── json2 ───────────────────────────────────────────────────────────────

template<typename Value>
Value* json_find_in(Value& root, const std::vector<std::string>& segments, std::size_t from)
{
    Value* current = &root;
    for (std::size_t i = from; i < segments.size(); ++i) {
        const std::string& segment = segments[i];
        if (current->is_object()) {
            current = current->find(segment);
        } else if (current->is_array()) {
            std::size_t index = 0;
            if (!parse_index(segment, index) || index >= current->size()) return nullptr;
            current = &(*current)[index];
        } else {
            return nullptr;
        }
        if (current == nullptr) return nullptr;
    }
    return current;
}

const json2::value* json_lookup(const json2::value& root, const std::vector<std::string>& segments)
{
    return json_find_in(root, segments, 0);
}

json2::value* json_find(json2::value& root, const std::vector<std::string>& segments)
{
    return json_find_in(root, segments, 0);
}

/// The value the segments name, making every level on the way: an object for a named
/// segment, an array for an all-digit one.
json2::value& json_descend(json2::value& node, const std::vector<std::string>& segments,
                           std::size_t from)
{
    if (from == segments.size()) return node;

    const std::string& segment = segments[from];
    std::size_t index = 0;
    if (parse_index(segment, index)) {
        if (node.is_null()) node.set_array();
        if (!node.is_array()) {
            throw std::runtime_error("settings: '" + join_key(segments, 0)
                                     + "' runs through a value that is not an array");
        }
        while (node.size() <= index) node.push_back(json2::value());
        return json_descend(node[index], segments, from + 1);
    }

    if (node.is_null()) node.set_object();
    if (!node.is_object()) {
        throw std::runtime_error("settings: '" + join_key(segments, 0)
                                 + "' runs through a value that is not an object");
    }
    return json_descend(node[segment], segments, from + 1);
}

bool json_erase(json2::value& root, const std::vector<std::string>& segments)
{
    const std::vector<std::string> parents(segments.begin(), segments.end() - 1);
    json2::value* parent = json_find(root, parents);
    if (parent == nullptr) return false;

    const std::string& last = segments.back();
    if (parent->is_object()) return parent->erase_key(last);

    std::size_t index = 0;
    if (!parse_index(last, index)) return false;
    return parent->erase(index);
}

// ── toml ────────────────────────────────────────────────────────────────

template<typename Node>
Node* toml_drill(Node* current, const std::vector<std::string>& segments, std::size_t from)
{
    for (std::size_t i = from; i < segments.size(); ++i) {
        const std::string& segment = segments[i];
        if (current->is_table()) {
            if (!current->has_key(segment)) return nullptr;
            current = &(*current)[segment];
        } else if (current->is_array()) {
            std::size_t index = 0;
            if (!parse_index(segment, index) || index >= current->array_size()) return nullptr;
            current = &(*current)[index];
        } else {
            return nullptr;
        }
    }
    return current;
}

template<typename Table>
std::conditional_t<std::is_const_v<Table>, const toml_value*, toml_value*>
toml_lookup_in(Table& root, const std::vector<std::string>& segments)
{
    const auto first = root.find(segments.front());
    if (first == root.end()) return nullptr;
    return toml_drill(&first->second, segments, 1);
}

const toml_value* toml_lookup(const toml& document, const std::vector<std::string>& segments)
{
    return toml_lookup_in(document.table(), segments);
}

toml_value* toml_find(toml& document, const std::vector<std::string>& segments)
{
    return toml_lookup_in(document.table(), segments);
}

/// The value the segments name, making every table on the way.
toml_value& toml_descend(toml_value& node, const std::vector<std::string>& segments,
                         std::size_t from)
{
    if (from == segments.size()) return node;

    const std::string& segment = segments[from];
    std::size_t index = 0;
    if (parse_index(segment, index)) {
        if (!node.is_array()) node = toml_value(toml_array{});
        while (node.array_size() <= index) node.push_back(toml_value());
        return toml_descend(node[index], segments, from + 1);
    }

    if (!node.is_table()) node = toml_value(toml_table{});
    if (!node.has_key(segment)) node[segment] = toml_value(toml_table{});
    return toml_descend(node[segment], segments, from + 1);
}

toml_value& toml_ensure(toml& document, const std::vector<std::string>& segments)
{
    toml_table& root = document.table();
    return toml_descend(root[segments.front()], segments, 1);
}

bool toml_erase(toml& document, const std::vector<std::string>& segments)
{
    if (segments.size() == 1) return document.erase(segments.front());

    const std::vector<std::string> parents(segments.begin(), segments.end() - 1);
    toml_value* parent = toml_find(document, parents);
    if (parent == nullptr) return false;

    const std::string& last = segments.back();
    if (parent->is_table()) return parent->erase(last);

    std::size_t index = 0;
    if (!parse_index(last, index)) return false;
    return parent->erase(index);
}

// ── ini ─────────────────────────────────────────────────────────────────

struct ini_slot {
    std::string section;
    std::string key;
};

ini_slot ini_slot_of(const std::vector<std::string>& segments)
{
    if (segments.size() == 1) return { "General", segments.front() };
    return { segments.front(), join_key(segments, 1) };
}

// INI holds text only, so a value is written the way it reads back best.
std::string ini_text_of(const settings_key& value)
{
    if (value.is_bool()) return value.as_bool() ? "true" : "false";
    if (value.is_int()) return std::to_string(value.as_int64());
    if (value.is_float()) return format_double(value.as_double());
    if (value.is_string()) return value.as_string();
    if (value.is_bytes()) return "base64:" + base64::encode(value.as_bytearray());
    throw std::runtime_error("settings: a null value has no INI text");
}

settings_key ini_key_of(const std::string& text)
{
    if (text == "true") return settings_key(true);
    if (text == "false") return settings_key(false);

    if (std::int64_t integer = 0; parse_int64(text, integer)) return settings_key(integer);
    if (double floating = 0; parse_double(text, floating)) return settings_key(floating);
    return settings_key(text);
}

// ── the file ────────────────────────────────────────────────────────────

void ensure_parent_directory(const std::filesystem::path& path)
{
    const std::filesystem::path parent = path.parent_path();
    if (parent.empty()) return;

    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
        throw std::runtime_error("settings: cannot create " + parent.string() + ": " + error.message());
    }
}

/// A missing file, an empty one and a file holding a stray byte order mark all start as an
/// empty object, so that a settings file created by hand works before anything is written.
/// @param has_text Set to whether the file held anything, which decides how it is written back.
json2::document load_json(const std::filesystem::path& path, bool exists, bool& has_text)
{
    json2::document document;
    has_text = false;
    if (exists) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) throw std::runtime_error("settings: cannot read " + path.string());
        const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        has_text = !trim(text).empty();
        if (has_text) document = json2::document::parse(text);
    }
    if (document.root().is_null()) document.root().set_object();
    return document;
}

} // namespace

template <typename T>
T &settings::getp()
{
    return std::get<T>(provider_instance); 
}

template <typename T>
const T &settings::getp() const
{
    return std::get<T>(provider_instance);
}

settings::settings(const std::filesystem::path& path, settings_provider provider)
    : settings(path, settings_options{ .provider = provider })
{
}

settings::settings(const std::filesystem::path& path, const settings_options& options)
    : provider_id(options.provider == settings_provider::automatic
                      ? provider_from_extension(path)
                      : options.provider)
    , file_path(path)
{
    const bool exists = std::filesystem::exists(path);

    switch (provider_id) {
    case settings_provider::json: {
        bool has_text = false;
        json2::document document = load_json(path, exists, has_text);
        json_needs_layout = !has_text;
        provider_instance.emplace<json2::document>(std::move(document));
        break;
    }

    case settings_provider::toml:
        if (exists) provider_instance.emplace<toml>(toml::fromFile(path));
        else provider_instance.emplace<toml>();
        break;

    case settings_provider::ini:
        provider_instance.emplace<::ini>();
        if (exists && !getp<::ini>().loadFromFile(path)) {
            throw std::runtime_error("settings: cannot read " + path.string());
        }
        break;

    case settings_provider::xkeydb: {
        auto database = std::make_unique<xkeydb::database>(path);
        if (!database->exists()) {
            if (database->status() != xkeydb::file_status::missing) {
                throw std::runtime_error(std::string("settings: cannot open ") + path.string() + ": "
                                         + xkeydb::database::describe(database->status()));
            }
            ensure_parent_directory(path);
            database->initialize({ .compress = options.compress, .integrity = options.integrity });
            if (!options.secret.empty()) database->lock(options.secret);
        } else if (!database->valid()) {
            throw std::runtime_error("settings: " + path.string() + " is not a usable xkeydb file: "
                                     + xkeydb::database::describe(database->status()));
        } else if (database->needsSecret()) {
            if (options.secret.empty()) {
                throw std::runtime_error("settings: " + path.string() + " needs a secret");
            }
            database->unlock(options.secret);
        }
        database->open();
        provider_instance.emplace<std::unique_ptr<xkeydb::database>>(std::move(database));
        break;
    }

    default:
        throw std::logic_error("settings: unknown provider");
    }
}

settings::~settings()
{
    if (!auto_save_flag || file_path.empty()) return;
    try {
        save();
    } catch (...) {
        // A destructor has nowhere to report a failed write; call save() by hand to see it.
    }
}

settings::settings(settings&& other) noexcept
    : provider_id(other.provider_id)
    , file_path(std::move(other.file_path))
    , auto_save_flag(other.auto_save_flag)
    , instant_save_flag(other.instant_save_flag)
    , json_needs_layout(other.json_needs_layout)
    , provider_instance(std::move(other.provider_instance))
{
    other.file_path.clear();
    other.auto_save_flag = false;
}

settings& settings::operator=(settings&& other) noexcept
{
    if (this == &other) return *this;

    provider_id = other.provider_id;
    file_path = std::move(other.file_path);
    auto_save_flag = other.auto_save_flag;
    instant_save_flag = other.instant_save_flag;
    json_needs_layout = other.json_needs_layout;
    provider_instance = std::move(other.provider_instance);

    other.file_path.clear();
    other.auto_save_flag = false;
    return *this;
}

void settings::save()
{
    if (file_path.empty()) return;
    ensure_parent_directory(file_path);

    switch (provider_id) {
    case settings_provider::json:
        if (json_needs_layout) {
            // Nothing was read, so there is no layout to keep: write the standard one.
            std::ofstream stream(file_path, std::ios::binary | std::ios::trunc);
            if (!stream) throw std::runtime_error("settings: cannot write " + file_path.string());
            stream << getp<json2::document>().to_string();
        } else {
            getp<json2::document>().to_file(file_path);
        }
        break;
    case settings_provider::toml:
        getp<toml>().toFile(file_path);
        break;
    case settings_provider::ini:
        if (!getp<::ini>().saveToFile(file_path)) {
            throw std::runtime_error("settings: cannot write " + file_path.string());
        }
        break;
    case settings_provider::xkeydb:
        getp<std::unique_ptr<xkeydb::database>>()->save();
        break;
    default:
        throw std::logic_error("settings: unknown provider");
    }
}

void settings::reload()
{
    const bool exists = std::filesystem::exists(file_path);

    switch (provider_id) {
    case settings_provider::json: {
        bool has_text = false;
        json2::document document = load_json(file_path, exists, has_text);
        json_needs_layout = !has_text;
        provider_instance.emplace<json2::document>(std::move(document));
        break;
    }
    case settings_provider::toml:
        provider_instance.emplace<toml>(exists ? toml::fromFile(file_path) : toml());
        break;
    case settings_provider::ini:
        provider_instance.emplace<::ini>();
        if (exists && !getp<::ini>().loadFromFile(file_path)) {
            throw std::runtime_error("settings: cannot read " + file_path.string());
        }
        break;
    case settings_provider::xkeydb: {
        xkeydb::database& database = *getp<std::unique_ptr<xkeydb::database>>();
        if (database.isOpen()) database.discard();
        else database.open();
        break;
    }
    default:
        break;
    }
}

void settings::clear()
{
    switch (provider_id) {
    case settings_provider::json:
        getp<json2::document>().root().set_object();
        break;
    case settings_provider::toml:
        provider_instance.emplace<toml>();
        break;
    case settings_provider::ini:
        provider_instance.emplace<::ini>();
        break;
    case settings_provider::xkeydb:
        getp<std::unique_ptr<xkeydb::database>>()->clear();
        break;
    default:
        break;
    }

    if (instant_save_flag) save();
}

void settings::set(const std::string& key, const settings_key& value)
{
    set_value(key, value);
}

void settings::set(const std::string& key, settings_key&& value)
{
    set_value(key, value);
}

void settings::set_default(const std::string& key, const settings_key& value)
{
    if (!contains(key)) set_value(key, value);
}

void settings::set_value(const std::string& key, const settings_key& value)
{
    const std::vector<std::string> segments = key_segments(key);

    switch (provider_id) {
    case settings_provider::json: {
        json2::value& root = getp<json2::document>().root();
        if (value.is_null()) {
            json_erase(root, segments);
            break;
        }
        json2::value& slot = json_descend(root, segments, 0);
        json2::value incoming = value.to_json();
        if (slot != incoming) slot = std::move(incoming);
        break;
    }
    case settings_provider::toml: {
        if (value.is_null()) {
            toml_erase(getp<toml>(), segments);
            break;
        }
        toml_value& slot = toml_ensure(getp<toml>(), segments);
        toml_value incoming = value.to_toml();
        if (!(slot == incoming)) slot = std::move(incoming);
        break;
    }
    case settings_provider::ini: {
        ::ini& instance = getp<::ini>();
        const ini_slot slot = ini_slot_of(segments);
        if (value.is_null()) instance.removeKey(slot.section, slot.key);
        else instance.setValue(slot.section, slot.key, ini_text_of(value));
        break;
    }
    case settings_provider::xkeydb: {
        xkeydb::database& database = *getp<std::unique_ptr<xkeydb::database>>();
        const std::string flat = join_key(segments, 0);
        if (value.is_null()) {
            database.eraseKey(flat);
            break;
        }
        const scl2::variant incoming = value.to_variant();
        scl2::variant existing;
        if (database.get(flat, existing) && existing == incoming) break;

        const xkeydb::xkeydb_error error = database.setValue(flat, incoming);
        if (error != xkeydb::xkeydb_error::None) {
            throw std::runtime_error(std::string("settings: cannot write ") + flat + ": "
                                     + xkeydb::database::describe(error));
        }
        break;
    }
    default:
        throw std::logic_error("settings: unknown provider");
    }

    if (instant_save_flag) save();
}

settings_key settings::get(const std::string& key) const
{
    const std::vector<std::string> segments = split_key(key);
    if (segments.empty()) return settings_key();

    switch (provider_id) {
    case settings_provider::json: {
        const json2::value* slot = json_lookup(getp<json2::document>().root(), segments);
        return slot ? settings_key::from_json(*slot) : settings_key();
    }
    case settings_provider::toml: {
        const toml_value* slot = toml_lookup(getp<toml>(), segments);
        return slot ? settings_key::from_toml(*slot) : settings_key();
    }
    case settings_provider::ini: {
        const ::ini& instance = getp<::ini>();
        const ini_slot slot = ini_slot_of(segments);
        if (!instance.hasKey(slot.section, slot.key)) return settings_key();
        return ini_key_of(instance.getValue(slot.section, slot.key));
    }
    case settings_provider::xkeydb: {
        scl2::variant stored;
        const auto& database = *getp<std::unique_ptr<xkeydb::database>>();
        if (!database.get(join_key(segments, 0), stored)) return settings_key();
        return settings_key::from_variant(stored);
    }
    default:
        return settings_key();
    }
}

settings_key settings::get(const std::string& key, const settings_key& default_value) const
{
    const settings_key value = get(key);
    return value.is_null() ? default_value : value;
}

bool settings::contains(const std::string& key) const
{
    const std::vector<std::string> segments = split_key(key);
    if (segments.empty()) return false;

    switch (provider_id) {
    case settings_provider::json:
        return json_lookup(getp<json2::document>().root(), segments) != nullptr;
    case settings_provider::toml:
        return toml_lookup(getp<toml>(), segments) != nullptr;
    case settings_provider::ini: {
        const ini_slot slot = ini_slot_of(segments);
        return getp<::ini>().hasKey(slot.section, slot.key);
    }
    case settings_provider::xkeydb:
        return getp<std::unique_ptr<xkeydb::database>>()->hasKey(join_key(segments, 0));
    default:
        return false;
    }
}

bool settings::remove(const std::string& key)
{
    const std::vector<std::string> segments = split_key(key);
    if (segments.empty()) return false;

    bool removed = false;
    switch (provider_id) {
    case settings_provider::json:
        removed = json_erase(getp<json2::document>().root(), segments);
        break;
    case settings_provider::toml:
        removed = toml_erase(getp<toml>(), segments);
        break;
    case settings_provider::ini: {
        ::ini& instance = getp<::ini>();
        const ini_slot slot = ini_slot_of(segments);
        if (instance.hasKey(slot.section, slot.key)) {
            instance.removeKey(slot.section, slot.key);
            removed = true;
        }
        break;
    }
    case settings_provider::xkeydb: {
        xkeydb::database& database = *getp<std::unique_ptr<xkeydb::database>>();
        const std::string flat = join_key(segments, 0);
        removed = database.hasKey(flat);
        database.eraseKey(flat);
        break;
    }
    default:
        break;
    }

    if (removed && instant_save_flag) save();
    return removed;
}

const char* settings_key::type_name() const noexcept
{
    switch (value_.index()) {
    case 0: return "null";
    case 1: return "bool";
    case 2: return "integer";
    case 3: return "floating";
    case 4: return "string";
    case 5: return "bytes";
    default: return "unknown";
    }
}

std::nullptr_t settings_key::as_null() const
{
    if (!is_null()) {
        throw std::runtime_error("settings: the value is a " + std::string(type_name()) + ", not null");
    }
    return nullptr;
}

bool settings_key::as_bool() const
{
    if (const auto* v = std::get_if<bool>(&value_)) return *v;
    if (const auto* v = std::get_if<std::int64_t>(&value_)) return *v != 0;
    if (const auto* v = std::get_if<double>(&value_)) return *v != 0.0;
    if (const auto* v = std::get_if<std::string>(&value_)) {
        const std::string_view text = trim(*v);
        if (iequals(text, "true") || text == "1") return true;
        if (iequals(text, "false") || text == "0") return false;
    }
    throw std::runtime_error("settings: a " + std::string(type_name()) + " value is not a bool");
}

std::int64_t settings_key::as_int64() const
{
    if (const auto* v = std::get_if<std::int64_t>(&value_)) return *v;
    if (const auto* v = std::get_if<bool>(&value_)) return *v ? 1 : 0;
    if (const auto* v = std::get_if<double>(&value_)) {
        if (*v < -9.2233720368547758e18 || *v > 9.2233720368547758e18) {
            throw std::runtime_error("settings: the value does not fit in an integer");
        }
        return static_cast<std::int64_t>(*v);
    }
    if (const auto* v = std::get_if<std::string>(&value_)) {
        if (std::int64_t integer = 0; parse_int64(*v, integer)) return integer;
        if (double floating = 0; parse_double(*v, floating)) return static_cast<std::int64_t>(floating);
    }
    throw std::runtime_error("settings: a " + std::string(type_name()) + " value is not an integer");
}

std::uint64_t settings_key::as_uint64() const
{
    const std::int64_t value = as_int64();
    if (value < 0) {
        throw std::runtime_error("settings: the value is negative, so it is not an unsigned integer");
    }
    return static_cast<std::uint64_t>(value);
}

double settings_key::as_double() const
{
    if (const auto* v = std::get_if<double>(&value_)) return *v;
    if (const auto* v = std::get_if<std::int64_t>(&value_)) return static_cast<double>(*v);
    if (const auto* v = std::get_if<bool>(&value_)) return *v ? 1.0 : 0.0;
    if (const auto* v = std::get_if<std::string>(&value_)) {
        if (double floating = 0; parse_double(*v, floating)) return floating;
    }
    throw std::runtime_error("settings: a " + std::string(type_name()) + " value is not a number");
}

std::string settings_key::as_string() const
{
    if (const auto* v = std::get_if<std::string>(&value_)) return *v;
    if (const auto* v = std::get_if<std::int64_t>(&value_)) return std::to_string(*v);
    if (const auto* v = std::get_if<double>(&value_)) return format_double(*v);
    if (const auto* v = std::get_if<bool>(&value_)) return *v ? "true" : "false";
    if (const auto* v = std::get_if<scl2::bytearray>(&value_)) return "base64:" + base64::encode(*v);
    throw std::runtime_error("settings: a null value is not a string");
}

scl2::bytearray settings_key::as_bytearray() const
{
    if (const auto* v = std::get_if<scl2::bytearray>(&value_)) return *v;
    if (const auto* v = std::get_if<std::string>(&value_)) {
        if (v->rfind("base64:", 0) == 0) return base64::decode(v->substr(7));
        return scl2::bytearray(*v);
    }
    throw std::runtime_error("settings: a " + std::string(type_name()) + " value is not bytes");
}

json2::value settings_key::to_json() const
{
    return std::visit([](const auto& v) -> json2::value {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return json2::value(nullptr);
        } else if constexpr (std::is_same_v<T, scl2::bytearray>) {
            return json2::value("base64:" + base64::encode(v));
        } else {
            return json2::value(v);
        }
    }, value_);
}

toml_value settings_key::to_toml() const
{
    return std::visit([](const auto& v) -> toml_value {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            throw std::runtime_error("settings: TOML has no null value");
        } else if constexpr (std::is_same_v<T, scl2::bytearray>) {
            return toml_value("base64:" + base64::encode(v));
        } else {
            return toml_value(v);
        }
    }, value_);
}

scl2::variant settings_key::to_variant() const
{
    return std::visit([](const auto& v) -> scl2::variant {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return scl2::variant(nullptr);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return scl2::variant(scl2::string(v));
        } else {
            return scl2::variant(v);
        }
    }, value_);
}

settings_key settings_key::from_json(const json2::value& value)
{
    if (value.is_null()) return settings_key(nullptr);
    if (value.is_bool()) return settings_key(value.as_bool());
    if (value.is_int()) return settings_key(value.as_int());
    if (value.is_double()) return settings_key(value.as_double());
    if (value.is_string()) return settings_key(value.as_string());
    return settings_key(); // an array or an object is not a settings value
}

settings_key settings_key::from_toml(const toml_value& value)
{
    switch (value.type()) {
    case toml_value_type::boolean: return settings_key(value.as_bool());
    case toml_value_type::integer: return settings_key(value.as_int());
    case toml_value_type::floating: return settings_key(value.as_double());
    case toml_value_type::string: return settings_key(value.as_string());
    case toml_value_type::datetime: return settings_key(value.as_datetime());
    default: return settings_key(); // an array or a table is not a settings value
    }
}

settings_key settings_key::from_variant(const scl2::variant& value)
{
    if (value.is_null()) return settings_key(nullptr);
    if (value.is_bool()) return settings_key(value.as_bool());
    if (value.is_int()) return settings_key(value.as_int());
    if (value.is_double()) return settings_key(value.as_double());
    if (value.is_string()) return settings_key(std::string(value.as_string()));
    if (value.is_bytearray()) return settings_key(value.as_bytearray());
    return settings_key(); // a vector or a map is not a settings value
}


std::unique_ptr<settings>& globalsettings::slot()
{
    // Destroyed on the way out of the program, which is what writes the file when reset()
    // was not called.
    static std::unique_ptr<settings> instance;
    return instance;
}

void globalsettings::init(const std::filesystem::path& path, settings_provider provider)
{
    init(path, settings_options{ .provider = provider });
}

void globalsettings::init(const std::filesystem::path& path, const settings_options& options)
{
    std::unique_ptr<settings>& instance = slot();
    if (instance) throw std::logic_error("globalsettings: already initialized");
    instance = std::make_unique<settings>(path, options);
}

void globalsettings::reset()
{
    slot().reset();
}

bool globalsettings::initialized() noexcept
{
    return slot() != nullptr;
}

settings& globalsettings::instance_ref()
{
    settings* instance = slot().get();
    if (instance == nullptr) throw std::logic_error("globalsettings: not initialized");
    return *instance;
}

} // namespace scl2