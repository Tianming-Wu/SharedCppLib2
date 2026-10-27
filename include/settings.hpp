/*
    Settings module for SharedCppLib2
    Tianming Wu <https://github.com/Tianming-Wu> 2026.7.5

    One object that holds the settings of an application, bound to a file. A key is the path
    of the value inside that file, and four formats can hold it:

      json    json2, so member order and the spelling of untouched values survive a save.
              A path with no known extension gets this one.
      toml    the same fidelity for TOML.
      ini     flat sections and keys; INI holds text, so a value's type comes from how it
              was written.
      xkeydb  a binary key-value file: typed values, optional encryption, atomic saves, and
              not meant to be edited by hand.

    Usage:
        scl2::settings config("config.toml");
        int width = config.get("window/width", 800);
        config.set("window/width", 1024);
        config.save();

    An enumeration registered with scl2_strenum / scl2_bitstrenum is written by the name its
    table gives it, and read back the same way:

        config.set("mode", AIU::StartStopMode::Automatic);      // the file holds "Automatic"
        auto mode = config.get("mode").as_strenum<AIU::StartStopMode>();
        auto mode = config.get("mode", AIU::StartStopMode::Automatic);   // with a default
*/

#pragma once

#include "basics.hpp"
#include "bytearray.hpp"
#include "api.hpp"
#include "enum.hpp"

#include "json2.hpp"
#include "toml.hpp"
#include "ini.hpp"
#include "xkeydb.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace scl2 {

/// The format a settings file is written in.
enum class settings_provider : uint8_t {
    automatic = 0, ///< decided by the file extension; json when nothing else fits
    json = 1,      ///< json2: member order and untouched values survive a save
    toml = 2,      ///< the same fidelity for TOML
    ini = 3,       ///< flat sections and keys, values as text
    xkeydb = 4,    ///< binary key-value file: typed values, optional encryption
};

/// What a settings file needs beyond its path.
struct settings_options {
    settings_provider provider = settings_provider::automatic;

    /// xkeydb only: unlocks an existing file, and encrypts one that is created.
    scl2::bytearray secret;

    /// xkeydb only: how a file that has to be created is compressed and checked.
    scl2::compression_id compress = compress_algo::none;
    xkeydb::integrity_algo integrity = xkeydb::integrity_algo::crc32;
};


/// One value of a settings file: null, bool, integer, floating point, string or bytes.
/// The type a value was given is the type it keeps; the accessors below convert where the
/// conversion makes sense, which matters most for INI, where everything is text.
class settings_key {
private:
    /// A class that is plain data: its bytes are the value.
    template<typename T>
    static constexpr bool plain_data =
        std::is_class_v<T> && std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>
        && !std::is_same_v<T, std::string> && !std::is_same_v<T, std::string_view>
        && !std::is_same_v<T, scl2::bytearray>;

public:
    using storage = std::variant<
        std::nullptr_t,
        bool, std::int64_t, double,
        std::string,
        scl2::bytearray
    >;

    settings_key() = default;

    settings_key(std::nullptr_t) : value_(nullptr) {}
    settings_key(bool v) : value_(v) {}
    settings_key(std::string v) : value_(std::move(v)) {}
    settings_key(const char* v) : value_(std::string(v)) {}
    settings_key(const scl2::bytearray& v) : value_(v) {}
    settings_key(scl2::bytearray&& v) : value_(std::move(v)) {}

    /// Any integral type, stored as std::int64_t.
    template<typename T>
    requires (std::is_integral_v<T> && !std::is_same_v<T, bool>)
    settings_key(T v) : value_(static_cast<std::int64_t>(v)) {}

    /// Any floating point type, stored as double.
    template<typename T>
    requires (std::is_floating_point_v<T>)
    settings_key(T v) : value_(static_cast<double>(v)) {}

    /// Any enumeration. One registered with scl2_strenum / scl2_bitstrenum is stored by the
    /// name its table gives it, so a file someone may open reads "Automatic" rather than
    /// 3; a value the table has no name for stays the number it is. Everything else is
    /// stored as its underlying value.
    template<typename T>
    requires (std::is_enum_v<T>)
    settings_key(T v) : value_(enum_storage(v)) {}

    /// A class that is plain data: stored as its bytes.
    template<typename T>
    requires (plain_data<T>)
    settings_key(const T& v)
        : value_(scl2::bytearray(reinterpret_cast<const char*>(&v), sizeof(T))) {}

    /// Anything else that can dump itself to bytes and load itself back.
    template<typename T>
    requires (std::is_class_v<T>
              && !(std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>)
              && scl2::has_generic_dump_load<T>)
    settings_key(const T& v) : value_(scl2::generic_dump(v)) {}

    // ── what it holds ────────────────────────────────────────────────

    bool is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(value_); }
    bool is_bool() const noexcept { return std::holds_alternative<bool>(value_); }
    bool is_int() const noexcept { return std::holds_alternative<std::int64_t>(value_); }
    bool is_float() const noexcept { return std::holds_alternative<double>(value_); }
    bool is_string() const noexcept { return std::holds_alternative<std::string>(value_); }
    bool is_bytes() const noexcept { return std::holds_alternative<scl2::bytearray>(value_); }

    const char* type_name() const noexcept;

    // ── reading it ───────────────────────────────────────────────────
    // Each of these converts where it can and throws std::runtime_error where it cannot.

    std::nullptr_t as_null() const;
    bool as_bool() const;
    std::int64_t as_int64() const;
    std::uint64_t as_uint64() const;
    float as_float() const { return static_cast<float>(as_double()); }
    double as_double() const;
    long double as_long_double() const { return static_cast<long double>(as_double()); }
    std::string as_string() const;

    /// The bytes of the value. A string gives its own bytes, or the decoded form when it
    /// starts with "base64:" - which is how the text providers store a bytearray.
    scl2::bytearray as_bytearray() const;

    /// The value as an enumeration taken straight from its number - the short form of
    /// static_cast<T>(as_int64()).
    /// @note An enumeration registered with scl2_strenum / scl2_bitstrenum is read with
    ///       as_strenum / as_bitstrenum or with as<T>() instead: its value may be a name.
    template<typename T>
    requires (std::is_enum_v<T>
              && !scl2::strenum_registered<T>
              && !scl2::bitstrenum_registered<T>)
    T as_enum() const { return static_cast<T>(as_int64()); }

    /// The value as the enumeration its table names it. A name is looked up, and a value
    /// stored as a number - or as text that is one - is taken as that number.
    /// @throws std::runtime_error when the key holds nothing that names a value of @p E.
    template<typename E>
    requires (std::is_enum_v<E> && scl2::strenum_registered<E>)
    E as_strenum() const { return enum_value<E>(); }

    /// The same for an enumeration registered with scl2_bitstrenum: a "FlagA | FlagC"
    /// text, or the number behind it.
    template<typename E>
    requires (std::is_enum_v<E> && scl2::bitstrenum_registered<E>)
    E as_bitstrenum() const { return enum_value<E>(); }

    /// The value as @p T: bool, an integral, a floating point, an enumeration, std::string,
    /// scl2::bytearray, a class that is plain data, or one with generic dump/load.
    template<typename T>
    T as() const {
        if constexpr (std::is_same_v<T, bool>)                  return as_bool();
        else if constexpr (std::is_enum_v<T>)                   return enum_value<T>();
        else if constexpr (std::is_integral_v<T>)               return static_cast<T>(as_int64());
        else if constexpr (std::is_floating_point_v<T>)         return static_cast<T>(as_double());
        else if constexpr (std::is_same_v<T, std::string>)      return as_string();
        else if constexpr (std::is_same_v<T, scl2::bytearray>)  return as_bytearray();
        else if constexpr (plain_data<T>)                       return as_trivial_class<T>();
        else                                                    return as_nontrivial_class<T>();
    }

    template<typename T>
    requires (plain_data<T>)
    T as_trivial_class() const {
        const scl2::bytearray bytes = as_bytearray();
        if (bytes.size() != sizeof(T)) {
            throw std::runtime_error("settings: the stored bytes are not the size of the requested type");
        }
        T object{};
        std::memcpy(&object, bytes.data(), sizeof(T));
        return object;
    }

    template<typename T>
    requires (std::is_class_v<T>
              && !(std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>)
              && scl2::has_generic_dump_load<T>)
    T as_nontrivial_class() const { return scl2::generic_load<T>(as_bytearray()); }

    // ── the shape each provider stores ───────────────────────────────

    json2::value to_json() const;
    toml_value to_toml() const;
    scl2::variant to_variant() const;

    static settings_key from_json(const json2::value& value);
    static settings_key from_toml(const toml_value& value);
    static settings_key from_variant(const scl2::variant& value);

private:
    /// What an enumeration is stored as: the name its table gives it when that name is one,
    /// the number otherwise. A value the table has no name for therefore loses nothing.
    template<typename T>
    requires (std::is_enum_v<T>)
    static storage enum_storage(T value)
    {
        if constexpr (scl2::strenum_registered<T> || scl2::bitstrenum_registered<T>) {
            if (auto text = scl2::to_string(value); text.has_value() && !text->empty()) {
                return storage(std::move(*text));
            }
        }
        return storage(static_cast<std::int64_t>(value));
    }

    /// An enumeration read back: the name it was stored as, else the number it holds. Text
    /// that is neither a name nor a number throws, as every other as_* does.
    template<typename T>
    requires (std::is_enum_v<T>)
    T enum_value() const
    {
        if constexpr (scl2::strenum_registered<T> || scl2::bitstrenum_registered<T>) {
            if (const auto* text = std::get_if<std::string>(&value_)) {
                if (auto parsed = scl2::from_string<T>(*text)) return *parsed;
            }
        }
        return static_cast<T>(as_int64());
    }

    storage value_;
};


/// An application's settings, bound to one file.
///
/// A key is a path: "window/width" is the member `width` of the object `window`. The
/// separator is '/', so a key may not contain one. An all-digit segment addresses an
/// element when the level it names is an array.
class settings {
public:
    settings(const std::filesystem::path& path,
             settings_provider provider = settings_provider::automatic);
    settings(const std::filesystem::path& path, const settings_options& options);
    ~settings();

    settings(settings&& other) noexcept;
    settings& operator=(settings&& other) noexcept;

    settings(const settings&) = delete;
    settings& operator=(const settings&) = delete;

    // ── values ───────────────────────────────────────────────────────

    /// Store @p value under @p key. A null value removes the key.
    void set(const std::string& key, const settings_key& value);
    void set(const std::string& key, settings_key&& value);

    /// An enumeration: a text file keeps the name its table gives it, an xkeydb file keeps
    /// the number - that one is a store for the program, not for a reader.
    template<typename T>
    requires (std::is_enum_v<T>)
    void set(const std::string& key, T value) { set_enum(key, value); }

    /// Store @p value only when the key holds nothing yet.
    void set_default(const std::string& key, const settings_key& value);

    template<typename T>
    requires (std::is_enum_v<T>)
    void set_default(const std::string& key, T value) {
        if (!contains(key)) set_enum(key, value);
    }

    /// The value under @p key, null when there is none. A key that names an object or an
    /// array holds no value, so it answers null as well.
    settings_key get(const std::string& key) const;

    /// The value under @p key, or @p default_value when there is none.
    settings_key get(const std::string& key, const settings_key& default_value) const;

    /// The value under @p key as @p T. @p default_value comes back when the key is missing
    /// or holds something that cannot be read as @p T.
    template<typename T>
    T get(const std::string& key, T default_value = T()) const {
        const settings_key value = get(key);
        if (value.is_null()) return default_value;
        try {
            return value.as<T>();
        } catch (...) {
            return default_value;
        }
    }

    bool contains(const std::string& key) const;
    bool remove(const std::string& key);
    void clear();

    // ── the file ─────────────────────────────────────────────────────

    const std::filesystem::path& path() const noexcept { return file_path; }
    settings_provider provider() const noexcept { return provider_id; }

    /// Write the document back. Creates the parent directory when it is missing.
    void save();

    /// Drop what is in memory and read the file again.
    void reload();

    bool auto_save() const noexcept { return auto_save_flag; }
    void set_auto_save(bool on) noexcept { auto_save_flag = on; }
    bool instant_save() const noexcept { return instant_save_flag; }
    void set_instant_save(bool on) noexcept { instant_save_flag = on; }
    
    void setValue(const std::string& key, const settings_key& value) { set(key, value); }
    void setValue(const std::string& key, settings_key&& value) { set(key, std::move(value)); }

    settings_key getValue(const std::string& key) const { return get(key); }
    settings_key getValue(const std::string& key, const settings_key& default_value) const { return get(key, default_value); }

private:
    template<typename T> T& getp();
    template<typename T> const T& getp() const;

    void set_value(const std::string& key, const settings_key& value);

    template<typename T>
    requires (std::is_enum_v<T>)
    void set_enum(const std::string& key, T value) {
        if (provider_id == settings_provider::xkeydb) {
            set_value(key, settings_key(static_cast<std::int64_t>(value)));
        } else {
            set_value(key, settings_key(value));
        }
    }

private:
    settings_provider provider_id = settings_provider::json;
    std::filesystem::path file_path;

    bool auto_save_flag = true;     // save on destruction
    bool instant_save_flag = false; // save on every change
    bool json_needs_layout = false; // the JSON document has no source text yet, so it is laid out on save

    std::variant<
        json2::document,
        toml,
        ::ini,
        std::unique_ptr<xkeydb::database>
    > provider_instance;
};


// A single settings object for the whole program.
// The singleinst module needs its instance to live on the stack, so this is its own
// small holder instead. The instance is held by a function-local static, so it is
// destroyed — and written out — when the program ends, whether reset() was called or not.
class globalsettings {
public:
    globalsettings() = delete;
    globalsettings(const globalsettings&) = delete;
    globalsettings(globalsettings&&) = delete;

    /// Create the one instance. Refuses when there already is one.
    static void init(const std::filesystem::path& path,
                     settings_provider provider = settings_provider::automatic);
    static void init(const std::filesystem::path& path, const settings_options& options);

    /// Destroy the instance, saving it if its auto-save is on.
    static void reset();

    static bool initialized() noexcept;

    /// The instance. Throws std::logic_error when init() has not run.
    static settings& instance_ref();

private:
    static std::unique_ptr<settings>& slot();
};

} // namespace scl2