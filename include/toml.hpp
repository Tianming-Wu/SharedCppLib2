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

    Fidelity — what a parse / edit / serialize cycle does with the file it read:
      - fidelity::raw (the default) keeps the source text: comments, blank lines, the order of the
        members, indentation, the spelling of every value and the line endings. Serialization
        writes the source back and regenerates only the values that were replaced, so editing one
        setting leaves the rest of the file — comments included — as it was.
      - fidelity::semantic keeps the values only. Everything is regenerated in a canonical layout,
        with the members still in document order (no comments, no source spelling).

    A table is scl2::ordered_map: TOML keys are unique, so a table is a map, and a map that
    remembers the order of the file is what a round trip needs. That makes this module standalone
    but for `ordered_map`.

    [SCL_STANDALONE_MODULE]
    version: 0.3.0
    cpp_generation: cxx17 - cxx23
    standalone_dependency: orderedmap
*/

#pragma once

#include "orderedmap.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace scl2 {

class toml_value;
class toml;

namespace parser_impl { class parser; }

/// @brief How much of the source text a parse keeps.
/// @see The fidelity section of doc/toml.md.
enum class fidelity {
    raw,       ///< comments, layout, order and the spelling of every value are kept
    semantic,  ///< the values only; serialization regenerates the document
};

/// @brief How a table was written, which is what decides how it is written back.
enum class toml_table_style : uint8_t {
    generated = 0,   ///< built by hand: written as a [header] block
    header,          ///< came from a [a.b] or [[a.b]] header line
    inline_table,    ///< came from { a = 1 }: written on its member's line
    dotted,          ///< exists only as the prefix of a dotted key: written inside its parent
};

typedef std::vector<toml_value> toml_array;
typedef scl2::ordered_map<std::string, toml_value> toml_table;

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
    toml_value(const toml_table& obj);
    toml_value(toml_table&& obj);
    /// The shape std::map used to be: still accepted, so code that builds a table with it keeps
    /// working. The result carries no source text, like any value made by hand.
    toml_value(const std::map<std::string, toml_value>& obj);
    toml_value(std::map<std::string, toml_value>&& obj);

    toml_value(const toml_value&) = default;
    toml_value(toml_value&&) = default;

    /// @brief Take the value of @p other, and regenerate its text.
    /// @note This is what `doc["port"] = 9090` goes through. The member keeps its own lead, key,
    ///       separator and trailing comment — only the value itself is written again.
    toml_value& operator=(const toml_value& other);
    toml_value& operator=(toml_value&& other) noexcept;

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
    const toml_table& as_table() const;
    bool& as_bool();
    int64_t& as_int();
    double& as_double();
    std::string& as_string();
    std::vector<toml_value>& as_array();
    toml_table& as_table();

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

    /// @brief Remove the member @p key (a table) or the element @p index (an array).
    /// @return Whether there was one. An array element takes its separator with it.
    bool erase(const std::string& key);
    bool erase(size_t index);

    // ---- source text (fidelity::raw) ----
    // Everything below is empty on a value that was built by hand, and describes what the file
    // said on one that was parsed. Serialization uses it verbatim when it is there.

    /// @brief Whether this value came from a parsed document.
    bool has_source() const { return sourced_; }

    /// @brief Whether the value was replaced after the parse, so its text is written again.
    /// @note The lead, the key, the separator and the trailing comment of the member survive that:
    ///       a changed value keeps its comment.
    bool dirty() const { return dirty_; }

    /// @brief The value token as it stands in the file, for a scalar (`0x1F`, `'literal'`, `1_000`,
    ///        `1.0`, `1979-05-27T07:32:00Z`). Empty for a container and for a regenerated value.
    const std::string& raw() const { return raw_; }

    /// @brief The whole header line of a table that was written as `[a.b]` / `[[a.b]]`,
    ///        line ending and trailing comment included.
    const std::string& header_text() const { return header_; }

    /// @brief The comment and blank lines in front of this member (for an array element or an
    ///        inline table member: the text in front of it, separator included).
    const std::string& lead() const { return lead_; }

    /// @brief The rest of this member's line after its value, line ending included.
    const std::string& trail() const { return trail_; }

    /// @brief The key as it stands (`a.b`, `"a b"`), empty when it is generated from the map key.
    const std::string& key_text() const { return key_text_; }

    /// @brief The text between the key and the value, `" = "` in most files.
    const std::string& separator() const { return separator_; }

    /// @brief The text between the last element and the closing bracket (arrays, inline tables).
    const std::string& tail() const { return tail_; }

    /// @brief How this table was written; `toml_table_style::generated` on anything else.
    toml_table_style table_style() const { return style_; }

    /// @brief Whether this table is written on its member's line as `{ a = 1 }`.
    bool is_inline_table() const { return style_ == toml_table_style::inline_table; }

    // ---- source text, writable ----

    /// @brief State the value token as written, for a value being built by hand.
    /// @details `v.set_raw("0x1F")` writes `0x1F` and keeps it: the same thing a parse does.
    void set_raw(std::string text);

    /// @brief Replace the lines in front of this member (a comment line ends with a line ending).
    void set_lead(std::string text);

    /// @brief Replace what follows this member's value to the end of its line, line ending included.
    void set_trail(std::string text);

    /// @brief Put a `# comment` after this member's value, in place of the one that is there.
    /// @param text The comment text, without the `#`. Empty removes the comment.
    void set_comment(std::string text);

    /// @brief Write this table as `{ a = 1 }` on its member's line (or as a `[header]` block again).
    void set_inline_table(bool on = true);

    toml_value_type type() const;

    /// table: number of members; array: size; string/datetime: length; else 0.
    size_t size() const;
    bool empty() const;

    bool operator==(const toml_value& other) const;
    bool operator!=(const toml_value& other) const { return !(*this == other); }

    // ---- assign into a user variable ----
    /// @brief Write this value into @p dest, converting to @p dest's type.
    ///        Works with concrete targets (bool / integral / floating /
    ///        std::string / toml_datetime / toml_array / toml_table) and with
    ///        std::variant targets (picks the matching alternative).
    /// @throw std::runtime_error if the value is not assignable to @p dest
    ///        (including implicit-conversion cases the target cannot accept).
    /// @note std::visit / if constexpr are C++17; this module targets cxx17-cxx23
    ///       so no feature guard is needed.
    template<typename T>
    void assign_to(T& dest) const {
        std::visit([&](const auto& src) {
            using SRC = std::decay_t<decltype(src)>;
            constexpr bool string_exact = std::is_same_v<T, std::string> && std::is_same_v<SRC, std::string>;
            constexpr bool bool_exact   = std::is_same_v<T, bool> && std::is_same_v<SRC, bool>;
            if constexpr (string_exact || bool_exact) {
                dest = src;
            } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, bool>) {
                // never allow narrowing like int -> char (string::operator=(char)) or int -> bool
                throw std::runtime_error("toml: assign_to: value type is not assignable to the target");
            } else if constexpr (std::is_same_v<T, std::map<std::string, toml_value>>
                                 && std::is_same_v<SRC, toml_table>) {
                // a table used to be a std::map; keep that destination working
                dest.clear();
                for (const auto& [k, v] : src) dest.emplace(k, v);
            } else if constexpr (std::is_assignable_v<T&, const SRC&>) {
                dest = src;
            } else {
                throw std::runtime_error("toml: assign_to: value type is not assignable to the target");
            }
        }, value);
    }

private:
    friend class parser_impl::parser;

    using variant_type = std::variant<std::string, int64_t, double, bool, toml_datetime,
                                      std::vector<toml_value>, toml_table>;

    variant_type value;

    // Source text (fidelity::raw). Empty on a value that was built by hand.
    std::string raw_;        // the scalar token as written
    std::string header_;     // the whole header line of a table written as [a.b]
    std::string lead_;       // what comes before this slot
    std::string key_text_;   // the key as written
    std::string separator_;  // between the key and the value
    std::string trail_;      // what follows the value on its line
    std::string tail_;       // before the closing bracket of an array / inline table
    toml_table_style style_ = toml_table_style::generated;
    bool sourced_ = false;   // it came from a parsed document
    bool dirty_ = false;     // its value was replaced, so its text is written again
};

/// @brief A TOML document — always a table of key/value pairs.
class toml {
public:
    toml() = default;

    /// @brief Parse a TOML document from a string.
    /// @param str The document text (UTF-8; a leading byte order mark is accepted).
    /// @param f How much of the source text to keep. `fidelity::raw` by default.
    /// @throw std::runtime_error on syntax errors.
    static toml fromString(const std::string& str, fidelity f = fidelity::raw);

    /// @brief Parse a TOML document from a file (UTF-8).
    /// @param path The file.
    /// @param f How much of the source text to keep. `fidelity::raw` by default.
    static toml fromFile(const std::filesystem::path& path, fidelity f = fidelity::raw);

    /// @brief Serialize this document back to TOML text.
    /// @details `fidelity::raw`: the source text comes back, and only the values that were
    ///          replaced, added or removed are written again. `fidelity::semantic`: the whole
    ///          document is regenerated in a canonical layout, members in document order.
    std::string toString() const;

    /// @brief Serialize and write to @p path.
    std::string toFile(const std::filesystem::path& path) const;

    /// @brief The fidelity this document was parsed with.
    fidelity mode() const { return m_fidelity; }

    /// @brief The text after the last member of the document, as it stands in the file.
    ///        Trailing comments and blank lines live here.
    const std::string& epilog() const { return m_epilog; }

    /// @brief Replace that text — the place for a comment at the end of the file.
    void set_epilog(std::string text) { m_epilog = std::move(text); }

    // The document itself is a table:
    bool has_key(const std::string& key) const;
    bool contains(const std::string& key) const { return has_key(key); }
    toml_value& operator[](const std::string& key);
    const toml_value& operator[](const std::string& key) const;
    size_t size() const;
    bool empty() const;

    /// @brief Remove the member @p key, comment included. @return Whether there was one.
    bool erase(const std::string& key);

    toml_table& table();
    const toml_table& table() const;

private:
    friend class parser_impl::parser;

    toml_table m_table;
    std::string m_epilog;                                        // what follows the last member
    fidelity m_fidelity = fidelity::raw;
};

} // namespace scl2
