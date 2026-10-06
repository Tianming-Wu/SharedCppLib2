/*
    json2 — a lossless-capable JSON module for SharedCppLib2 (experimental, sits next to json)

    Two fidelity levels over the same node type:
      - fidelity::raw      : the source text is kept, so unmodified parts serialize back
                             byte for byte — whitespace, indentation, key order, the way a
                             number was spelled, the way a string was escaped.
      - fidelity::semantic : only the values are kept; serialization is regenerated compact.

    Member order is preserved, and lookup by key stays O(log n) at scale: members live in a
    vector (contiguous, cheap to walk and to parse into) and an index sorted by key is built
    on demand, only when a lookup is asked of an object that is big enough to warrant it.
    Nothing about a lookup reorders the members.

    Design notes:
      - Every array element / object member carries the text in front of it (`lead`: the comma
        and the whitespace), and a container carries only `tail` (what sits between its last
        element, or its opening bracket, and the closing bracket). So serialization is
          "[" + sum(lead + element) + tail + "]"
        which reproduces the source exactly and regenerates only the parts that were touched.
      - A scalar's `raw` is the source token. Empty means "there is none" — it was written by
        hand or parsed in semantic mode — and serialization regenerates it from the value.
      - Duplicate keys: raw mode keeps every one of them, because a faithful round trip has to;
        semantic mode keeps the first and drops the rest, which is what scl2::json does.
      - No dependency outside the standard library, so this is a standalone module that can be
        copied on its own.

    Not implemented yet:
      - path queries / JSON Pointer, reformatting (indentation inference), a pretty printer
        beyond what the source already had, and the bytearray dump/load integration.

    [SCL_STANDALONE_MODULE]
    version: 0.1.0
    cpp_generation: cxx17 - cxx23
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace scl2::json2 {

/// How much of the source text is kept.
enum class fidelity : std::uint8_t {
    semantic = 0, ///< values only; serialization is regenerated (compact)
    raw = 1,      ///< keep the source; unmodified parts come back byte for byte
};

/// The order of these must match the std::variant in `value`.
enum class value_type : std::uint8_t {
    null = 0,
    boolean = 1,
    integer = 2,
    floating = 3,
    string = 4,
    array = 5,
    object = 6,
};

class parsing_error : public std::runtime_error {
public:
    explicit parsing_error(const std::string& message)
        : std::runtime_error("json2 parsing error: " + message) {}
};

/// How a regenerated document is indented. `space4` is what scl2::json writes by default, and
/// the default here too.
enum class indent_style : std::uint8_t {
    none = 0, ///< one element per line, but nothing in front of it
    space2 = 1,
    space4 = 2,
    tab = 3,
};

/// The layout of the regenerated text, and the two switches that have nothing to do with layout
/// but sit next to it, the same way they do in json's exporter.
struct exporter {
    bool isCompact = false;    ///< no line breaks at all, and no space after the ':'
    bool isInline = false;     ///< line breaks become single spaces, so everything is one line
    indent_style indentStyle = indent_style::space4;
    bool escapeNonAscii = false; ///< write every byte above 0x7F as a \uXXXX escape

    static exporter compact_exporter() { exporter e; e.isCompact = true; return e; }
    static exporter inline_exporter() { exporter e; e.isInline = true; return e; }
};

class value;
struct item;
struct member;

/// How many members an object may have before a lookup builds the key-sorted index. Above it
/// a lookup is O(log n); at or below it a linear scan over contiguous members is faster.
///
/// Overridable at compile time, before including this header:
///   SCL2_JSON2_INDEX_THRESHOLD 16        the default
///   SCL2_JSON2_INDEX_THRESHOLD 0         index every object from its first member
///   SCL2_JSON2_INDEX_THRESHOLD SIZE_MAX  no index at all, always linear
#ifndef SCL2_JSON2_INDEX_THRESHOLD
#define SCL2_JSON2_INDEX_THRESHOLD 16
#endif

/// The body of an array: the elements, and the text before the closing bracket.
struct array_body {
    std::vector<item> items; ///< in document order, each with the text in front of it
    std::string tail;        ///< between the last element (or '[') and ']'
};

/// An ordered object. Members keep their document order; lookup uses an index built on demand.
class object_body {
public:
    /// The text between the last member (or '{') and '}'.
    std::string tail;

    std::size_t size() const { return members_.size(); }
    bool empty() const { return members_.empty(); }

    /// The members, in document order. Taking the mutable view drops the lookup index, so
    /// edit through it freely and the next lookup rebuilds.
    std::vector<member>& members() { index_ready_ = false; return members_; }
    const std::vector<member>& members() const { return members_; }

    /// The member with this key, or nullptr when there is none. The first match wins.
    member* find(std::string_view key);
    const member* find(std::string_view key) const;

    /// The value of this key, or nullptr when there is none.
    value* find_value(std::string_view key);
    const value* find_value(std::string_view key) const;

    /// The member with this key, appended at the end when it is not there yet.
    member& operator[](std::string key);

    /// Append without looking for the key (the parser's path in raw mode, where a duplicate
    /// key has to survive).
    member& append(std::string key);

    /// Remove the first member with this key. Returns whether anything was removed.
    bool erase(std::string_view key);

    void clear();

    /// How many members a lookup walks before it builds the index.
    static constexpr std::size_t index_threshold = SCL2_JSON2_INDEX_THRESHOLD;

private:
    /// Build the key-sorted index if it is not current.
    void ensure_index() const;

    std::vector<member> members_;
    mutable std::vector<std::uint32_t> index_; ///< positions into members_, sorted by key
    mutable bool index_ready_ = false;
};

/// A JSON Pointer (RFC 6901): `/a/0/b`. Segments are separated by `/`; inside one, `~1` is a `/`
/// and `~0` is a `~`. The empty pointer names the whole document.
///
/// A pointer is parsed once into its segments and can then be applied as often as wanted, to any
/// value.
class pointer {
public:
    pointer() = default;

    /// Explicit on purpose: `at_path("/a/0")` takes the text directly, so an implicit
    /// conversion from it would only make those calls ambiguous.
    explicit pointer(std::string_view text) { assign(text); }

    /// Parse the text.
    /// @throws std::invalid_argument when it is not a pointer (it has to start with '/').
    void assign(std::string_view text);

    const std::vector<std::string>& segments() const { return segments_; }
    bool empty() const { return segments_.empty(); }

    /// The pointer text, with the escapes put back.
    std::string str() const;
    std::string to_string() const { return str(); }

private:
    std::vector<std::string> segments_;
};

/// One JSON value: null, bool, number, string, array or object.
class value {
public:
    value() = default; ///< null
    value(std::nullptr_t) {}
    value(bool b) : data_(b) {}

    /// Any integral or floating type, cast to int64_t or double. One template rather than one
    /// per kind: two of them would have the same signature.
    template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>>
    value(T v) {
        if constexpr (std::is_floating_point_v<T>) data_ = static_cast<double>(v);
        else                                      data_ = static_cast<std::int64_t>(v);
    }

    value(std::string s) : data_(std::move(s)) {}
    value(std::string_view s) : data_(std::string(s)) {}
    value(const char* s) : data_(std::string(s)) {}

    static value make_array();
    static value make_object();

    // ── what it is ────────────────────────────────────────────────────
    value_type type() const noexcept { return static_cast<value_type>(data_.index()); }
    bool is_null() const noexcept { return type() == value_type::null; }
    bool is_bool() const noexcept { return type() == value_type::boolean; }
    bool is_int() const noexcept { return type() == value_type::integer; }
    bool is_double() const noexcept { return type() == value_type::floating; }
    bool is_number() const noexcept { return is_int() || is_double(); }
    bool is_string() const noexcept { return type() == value_type::string; }
    bool is_array() const noexcept { return type() == value_type::array; }
    bool is_object() const noexcept { return type() == value_type::object; }

    // ── reading scalars (throw when the value is another type) ────────
    bool as_bool() const;
    std::int64_t as_int() const;
    double as_double() const;
    const std::string& as_string() const;

    // ── arrays ────────────────────────────────────────────────────────
    std::size_t size() const;
    bool empty() const;
    value& operator[](std::size_t index);
    const value& operator[](std::size_t index) const;

    /// Append to an array. A null value becomes an empty array first; anything else throws.
    /// `lead` is what gets written in front of it, so a document built by hand serializes as
    /// `[1, 2, 3]` — pass an empty string for a compact `[1,2,3]`.
    void push_back(value v, std::string lead = ", ");

    /// Remove an element. Returns whether anything was removed.
    bool erase(std::size_t index);

    array_body&       as_array_body();
    const array_body& as_array_body() const;

    // ── objects ───────────────────────────────────────────────────────
    bool has_key(std::string_view key) const;

    /// The value under this key, or nullptr when there is none.
    value* find(std::string_view key);
    const value* find(std::string_view key) const;

    /// The value under this key, appended as null when it is not there yet. A null value
    /// becomes an empty object first; anything else throws.
    value& operator[](std::string key);

    /// Remove the member with this key. Returns whether anything was removed.
    bool erase_key(std::string_view key);

    /// The same, under the name `json` uses for it.
    bool remove_member(std::string_view key) { return erase_key(key); }
    bool erase(std::string_view key) { return erase_key(key); }

    object_body&       as_object_body();
    const object_body& as_object_body() const;

    /// The members, in document order.
    std::vector<member>&       members() { return as_object_body().members(); }
    const std::vector<member>& members() const { return as_object_body().members(); }

    /// The elements of an array, in document order (each with the text in front of it).
    std::vector<item>&       items() { return as_array_body().items; }
    const std::vector<item>& items() const { return as_array_body().items; }

    // ── arrays and objects, the `json`-shaped way in ──────────────────
    /// The element at this index, bounds checked.
    /// @throws std::out_of_range when it is not an array or the index is past the end.
    const value& at(std::size_t index) const { return (*this)[index]; }

    /// The value under this key.
    /// @throws std::out_of_range when it is not an object or the key is not there.
    const value& at(std::string_view key) const;

    const value& front() const;
    const value& back() const;

    /// Remove the last element of an array. Silently does nothing when it is empty, like
    /// std::vector::pop_back().
    void pop_back();

    /// Turn this into an empty array / object, whatever it was.
    void clear_as_array();
    void clear_as_object();

    /// Whether this is an array / object and empty.
    bool empty_as_array() const { return is_array() && as_array_body().items.empty(); }
    bool empty_as_object() const { return is_object() && as_object_body().empty(); }

    /// The length of a string. 0 for anything else.
    std::size_t length() const { return is_string() ? std::get<std::string>(data_).size() : 0; }

    // ── JSON Pointer ──────────────────────────────────────────────────
    /// The value the pointer names.
    /// @throws std::out_of_range when it names nothing, or the path runs through a scalar.
    value& at_path(const pointer& p);
    const value& at_path(const pointer& p) const;
    value& at_path(std::string_view pointer_text) { return at_path(pointer(pointer_text)); }
    const value& at_path(std::string_view pointer_text) const { return at_path(pointer(pointer_text)); }

    /// The value the pointer names, or nullptr when it names nothing.
    value* find_path(const pointer& p);
    const value* find_path(const pointer& p) const;
    value* find_path(std::string_view pointer_text) { return find_path(pointer(pointer_text)); }
    const value* find_path(std::string_view pointer_text) const { return find_path(pointer(pointer_text)); }

    /// Whether the pointer names something here.
    bool contains_path(std::string_view pointer_text) const {
        return find_path(pointer_text) != nullptr;
    }

    // ── writing ───────────────────────────────────────────────────────
    void set_null();
    void set_bool(bool b);
    void set_int(std::int64_t i);
    void set_double(double d);
    void set_string(std::string s);
    void set_array();
    void set_object();

    /// Back to null.
    void clear();

    /// The source text of this scalar, when it was parsed in raw mode and has not been
    /// touched. Empty means "no source text": serialization regenerates it.
    const std::string& raw() const { return raw_; }
    void set_raw(std::string text) { raw_ = std::move(text); }

    // ── serializing ───────────────────────────────────────────────────
    /// The value as JSON text. Parts that were never modified come out exactly as they were
    /// written; the rest is regenerated.
    std::string dump() const;
    void dump_to(std::string& out) const;

    /// The value as JSON text, laid out by `fmt`. The source layout and the original spelling
    /// are not used — this is regenerated, which is what a reformat produces. The default
    /// (`exporter()`) is the layout scl2::json writes by default: one member or element per
    /// line, indented with four spaces, `": "` after a key, and empty containers on one line.
    std::string to_string(const exporter& fmt = exporter()) const;

    /// The compact form: `to_string(exporter::compact_exporter())`.
    std::string to_compact_string() const;

    /// Name of the type, for messages.
    const char* type_name() const noexcept;

    // ── comparing ─────────────────────────────────────────────────────
    /// Whether two values hold the same thing. The source text is not part of this: two values
    /// parsed from differently formatted documents are equal when what they hold is equal.
    /// Arrays are compared element by element, objects by key — member order does not matter.
    bool operator==(const value& other) const;
    bool operator!=(const value& other) const { return !(*this == other); }

    /// Nothing but whole values: `bool`, an integral, a floating point, or `std::string`. No
    /// parsing happens, so a string value does not assign to a number and a number does not
    /// assign to a string — use `as_string()` / `as_int()` when that is what is wanted.
    template<typename T>
    void assign_to(T& dest) const {
        if constexpr (std::is_same_v<T, bool>) {
            dest = as_bool();
        } else if constexpr (std::is_integral_v<T>) {
            dest = static_cast<T>(as_int());
        } else if constexpr (std::is_floating_point_v<T>) {
            dest = static_cast<T>(as_double());
        } else if constexpr (std::is_same_v<T, std::string>) {
            dest = as_string();
        } else {
            static_assert(sizeof(T) == 0, "json2: assign_to: unsupported target type");
        }
    }

private:
    /// Write the value out with the layout `fmt` asks for, ignoring the source text.
    void regenerate(std::string& out, const exporter& fmt, std::size_t level) const;

    std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, array_body, object_body> data_;
    std::string raw_;
};

/// One array element: the text in front of it, and the value.
struct item {
    std::string lead; ///< comma and whitespace before this element, as written
    value       val;
};

/// One object member.
struct member {
    std::string lead;      ///< comma and whitespace before the key, as written
    std::string raw_key;   ///< the key token as written, quotes included; empty when absent
    std::string between;   ///< the text between the key and the value (": " and friends)
    std::string key;       ///< the decoded key
    value       val;
    bool        key_dirty = false; ///< set when the key was changed; the token is regenerated
};

/// A parsed document: the root value plus whatever surrounded it.
class document {
public:
    document() = default;

    /// Parse JSON text. `fidelity::raw` keeps the source text so the document can be written
    /// back unchanged.
    /// @throws parsing_error when the text is not valid JSON, with the offset in the message.
    static document parse(std::string text, fidelity f = fidelity::raw);

    /// Read a file and parse it. The bytes are used as they are; no BOM handling beyond
    /// keeping it in the prolog.
    /// @throws std::runtime_error when the file cannot be read.
    static document from_file(const std::filesystem::path& path, fidelity f = fidelity::raw);

    /// Serialize the document.
    std::string serialize() const;

    /// Serialize it regenerated: the source layout and the original spelling are dropped, and
    /// the text is laid out by `fmt` — the default being scl2::json's default layout. This is
    /// what a reformat produces.
    std::string to_string(const exporter& fmt = exporter()) const;

    /// The compact form.
    std::string to_compact_string() const;

    /// The value this pointer names, and whether it names one.
    value&       at_path(std::string_view pointer_text) { return root_.at_path(pointer_text); }
    const value& at_path(std::string_view pointer_text) const { return root_.at_path(pointer_text); }
    value*       find_path(std::string_view pointer_text) { return root_.find_path(pointer_text); }
    const value* find_path(std::string_view pointer_text) const { return root_.find_path(pointer_text); }
    bool contains_path(std::string_view pointer_text) const { return root_.contains_path(pointer_text); }

    /// Serialize into a file. Returns the number of bytes written.
    /// @throws std::runtime_error when the file cannot be written.
    std::size_t to_file(const std::filesystem::path& path) const;

    fidelity mode() const { return mode_; }

    value&       root() { return root_; }
    const value& root() const { return root_; }

    /// The text before and after the root value (whitespace, and a byte order mark).
    const std::string& prolog() const { return prolog_; }
    const std::string& epilog() const { return epilog_; }
    void set_prolog(std::string text) { prolog_ = std::move(text); }
    void set_epilog(std::string text) { epilog_ = std::move(text); }

private:
    fidelity    mode_ = fidelity::raw;
    std::string prolog_;
    std::string epilog_;
    value       root_;
};

} // namespace scl2::json2
