/*
    TOML module for SharedCppLib2.

    TOML (Tom's Obvious, Minimal Language) is a configuration file format that
    maps naturally to a table of key/value pairs.

    A TOML document is always a table; a value inside it can be:
      - string     (basic "..." and literal '...', both with multi-line forms)
      - integer    (decimal, hex/octal/binary, underscores allowed)
      - floating   (also inf / nan)
      - boolean    (true / false)
      - date-time  (kept verbatim, e.g. 2024-01-01T12:00:00Z)
      - array      [1, 2, 3]
      - table      ([table] headers and inline { a = 1 } tables)

    It supports dotted keys (a.b.c = 1), [[array-of-tables]] headers, and
    comments (# to end of line). There is no null type.

    Usage:
        scl2::toml doc = scl2::toml::fromString(text);       // parse
        std::string v = doc["mods"][0]["modId"].as_string(); // access
        std::string out = doc.toString();                    // serialize back

    [SCL_STANDALONE_MODULE]
    version: 0.1.0
    cpp_generation: cxx17 - cxx23
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace scl2 {

class toml_value;
class toml;

typedef std::vector<toml_value> toml_array;
typedef std::map<std::string, toml_value> toml_table;

/// @brief A TOML date-time value, preserved verbatim as text.
struct toml_datetime {
    std::string text;
    bool operator==(const toml_datetime& other) const { return text == other.text; }
};

enum class toml_value_type : uint8_t {
    string = 0,
    integer = 1,
    floating = 2,
    boolean = 3,
    datetime = 4,
    array = 5,
    table = 6,
};

/// @brief A single TOML value (a member of a table or an element of an array).
class toml_value {
public:
    toml_value();  // an empty string
    toml_value(const std::string& s);
    toml_value(std::string&& s);
    toml_value(const char* s) : toml_value(std::string(s)) {}
    toml_value(int64_t i);
    toml_value(double d);
    toml_value(bool b);
    toml_value(const toml_datetime& dt);
    toml_value(toml_datetime&& dt);
    toml_value(const std::vector<toml_value>& arr);
    toml_value(std::vector<toml_value>&& arr);
    toml_value(const std::map<std::string, toml_value>& obj);
    toml_value(std::map<std::string, toml_value>&& obj);

    /// Accept any integral or floating type - cast to int64_t or double.
    template<typename T,
             typename = std::enable_if_t<(std::is_integral_v<T> || std::is_floating_point_v<T>)
                                         && !std::is_same_v<T, bool>>>
    toml_value(T val) {
        if constexpr (std::is_floating_point_v<T>)
            value = static_cast<double>(val);
        else
            value = static_cast<int64_t>(val);
    }

    bool is_bool() const;
    bool is_int() const;
    bool is_double() const;
    bool is_string() const;
    bool is_datetime() const;
    bool is_array() const;
    bool is_table() const;

    bool as_bool() const;
    int64_t as_int() const;
    double as_double() const;
    const std::string& as_string() const;
    const std::string& as_datetime() const;
    const std::vector<toml_value>& as_array() const;
    const std::map<std::string, toml_value>& as_table() const;
    bool& as_bool();
    int64_t& as_int();
    double& as_double();
    std::string& as_string();
    std::vector<toml_value>& as_array();
    std::map<std::string, toml_value>& as_table();

    // ---- array ----
    toml_value& operator[](size_t index);
    const toml_value& operator[](size_t index) const;
    size_t array_size() const;
    void push_back(const toml_value& v);
    bool empty_as_array() const;

    // ---- table ----
    bool has_key(const std::string& key) const;
    bool contains(const std::string& key) const { return has_key(key); }
    toml_value& operator[](const std::string& key);
    const toml_value& operator[](const std::string& key) const;
    const toml_value& at(const std::string& key) const;
    size_t table_size() const;

    toml_value_type type() const;

    /// table: number of members; array: size; string/datetime: length; else 0.
    size_t size() const;
    bool empty() const;

    bool operator==(const toml_value& other) const;
    bool operator!=(const toml_value& other) const { return !(*this == other); }

private:
    std::variant<std::string, int64_t, double, bool, toml_datetime,
                 std::vector<toml_value>, std::map<std::string, toml_value>>
        value;
};

/// @brief A TOML document — always a table of key/value pairs.
class toml {
public:
    toml() = default;

    /// @brief Parse a TOML document from a string.
    /// @throw std::runtime_error on syntax errors.
    static toml fromString(const std::string& str);

    /// @brief Parse a TOML document from a file (UTF-8).
    static toml fromFile(const std::filesystem::path& path);

    /// @brief Serialize this document back to TOML text.
    std::string toString() const;

    /// @brief Serialize and write to @p path.
    std::string toFile(const std::filesystem::path& path) const;

    // The document itself is a table:
    bool has_key(const std::string& key) const;
    bool contains(const std::string& key) const { return has_key(key); }
    toml_value& operator[](const std::string& key);
    const toml_value& operator[](const std::string& key) const;
    size_t size() const;
    bool empty() const;

    toml_table& table();
    const toml_table& table() const;

private:
    toml_table m_table;
};

} // namespace scl2
