/*
    YAML (YAML Ain't Markup Language) Module for SharedCppLib2

    [SCL_MODULE_STANDARD_SUPPORT]
    YAML_STANDARD: YAML 1.2 (https://yaml.org/spec/1.2/spec.html)
    FULL_FEATURED: NO

    [SCL_STANDALONE_MODULE]
    version: 0.1.0
*/

#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <sstream>
#include <stdexcept>
#include <type_traits>

#ifdef __cpp_lib_generator
    #include <generator>
#endif

namespace scl2::yaml {

enum class type : uint8_t {
    null = 0,
    boolean = 1,
    integer = 2,
    floating = 3,
    string = 4,
    array = 5,
    object = 6,
    alias = 7,
};


class value;
class document;
class parser;

class yaml_exception : public std::runtime_error {
public:
    yaml_exception(const std::string& message) : std::runtime_error("yaml: " + message) {}
};

struct alias_ref {
    std::string name;
    value* target = nullptr; // resolved after parsing
    bool operator==(const alias_ref& other) const { return name == other.name; }
};

// ---- Feature flags (add new ones as features are implemented) ----
struct features {
    bool comments      = true;   // # line comments
    bool multi_line    = true;   // | and > scalar blocks
    bool anchors       = false;  // &anchor and *alias (deferred — needs second pass)
    bool tags          = false;  // !!type tags
    bool multi_doc     = false;  // --- / ... document separators
};

class value {
public:
    value() = default;

    value(std::nullptr_t);
    value(bool b);
    value(int64_t i);
    value(double d);
    value(const std::string& s);
    value(std::string&& s);
    value(const char* s) : value(std::string(s)) {}
    value(const std::vector<value>& arr);

    // Accept any integral or floating type - cast to int64_t or double
    template<typename T,
             typename = std::enable_if_t<(std::is_integral_v<T> || std::is_floating_point_v<T>)
                                         && !std::is_same_v<T, bool>>>
    value(T val) {
        if constexpr (std::is_floating_point_v<T>)
            _value = static_cast<double>(val);
        else
            _value = static_cast<int64_t>(val);
    }
    value(std::vector<value>&& arr);
    value(const std::map<std::string, value>& obj);
    value(std::map<std::string, value>&& obj);

    bool is_null() const;
    bool is_bool() const;
    bool is_int() const;
    bool is_double() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;
    bool is_alias() const;

    nullptr_t as_null() const;
    bool as_bool() const;
    int64_t as_int() const;
    double as_double() const;
    const std::string& as_string() const;
    const std::vector<value>& as_array() const;
    const std::map<std::string, value>& as_object() const;

    nullptr_t& as_null();
    bool& as_bool();
    int64_t& as_int();
    double& as_double();
    std::string& as_string();
    std::vector<value>& as_array();
    std::map<std::string, value>& as_object();

    const alias_ref& as_alias() const;
    alias_ref& as_alias();

    // for array
#ifdef __cpp_lib_generator
    std::generator<const value&> array_elements() const;
#endif
    value& operator[](size_t index);
    const value& operator[](size_t index) const;
    const value& at(size_t index) const;
    size_t array_size() const;
    void push_back(const value& v);
    void push_back(value&& v);

    // for object
#ifdef __cpp_lib_generator
    std::generator<std::pair<const std::string&, const value&>> object_members() const;
#endif
    bool has_key(const std::string& key) const;
    const value& operator[](const std::string& key) const;
    value& operator[](const std::string& key);
    const value& at(const std::string& key) const;
    size_t object_size() const;
    bool remove_member(const std::string& key);

    // for string
    size_t length() const;

    // universal
    // for array and object, return the size. For string, return length. For other types, return 0.
    size_t size() const;
    void clear(); // clear the value, set it to null

    // pointer

    type type() const;

    bool operator==(const value& other) const;
    bool operator!=(const value& other) const { return !(*this == other); }

private:
    std::variant<
        std::nullptr_t,
        bool,
        int64_t,
        double,
        std::string,
        std::vector<value>,
        std::map<std::string, value>,
        alias_ref
    > _value;
};


class document : public value {
    friend class parser;
public:
    using value::value;
    document() = default;
    explicit document(value&& v) : value(std::move(v)) {}

    static document fromStream(std::istream& input, features feat = {});
    static document fromString(const std::string& input, features feat = {});

private:
    std::map<std::string, value*> _anchors;  // anchor table (deferred)
};


class parser {
public:
    // ---- Entry points ----
    static document fromStream(std::istream& input, features feat = {});
    static document fromString(const std::string& input, features feat = {});

    // Streaming: read next document from a persistent stream.
    // Returns true if a document was parsed, false at EOF.
    bool parseNext(std::istream& input, document& out);

    features feat;  // set before parsing

private:
    // ---- Line buffer ----
    std::string _line;
    size_t      _line_pos = 0;    // cursor in current line
    size_t      _line_num = 0;    // current line number (for errors)
    int         _indent_level= 0; // indent of current line
    int         _indent_unit = 0; // indent unit


    // ---- Indent stack (tracks block scopes) ----
    struct indent_frame {
        int         indent_level; // indent level for this scope
        value       container;    // the array/object being built (null until first entry)
        bool        is_mapping;   // true=mapping, false=sequence
        std::string pending_key;  // current key for addValue (inline values)
        std::string parent_key;   // key in parent frame that this frame fills (pop writes back)
    };
    std::vector<indent_frame> _stack;

    // ---- Anchor table (deferred) ----
    // std::map<std::string, value*> _anchors;

    // ---- Core loop ----
    bool readLine(std::istream& input);
    void parseLine();

    // ---- Dispatching (extend here for new features) ----
    void dispatch_scalar(const std::string& text);
    void dispatch_mapping_key(const std::string& key);
    void dispatch_sequence_entry();

    // ---- Scalar detection ----
    bool isNull(const std::string& s);
    bool isBool(const std::string& s);
    bool isInteger(const std::string& s);
    bool isDouble(const std::string& s);
    std::string processDoubleQuotedEscapes(const std::string& s);

    // ---- Helpers ----
    int  countIndentLevel();
    void pushIndent(int indent, bool is_mapping);
    void popIndent();
    value& currentContainer();
    void addValue(value&& v);
    std::string stripComments(const std::string& line) const;
};




} // namespace scl2::yaml