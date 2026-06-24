/*
    Json Module for SharedCppLib2

    Yes, it finally comes, json support for SharedCppLib2.
    It won't be simple, but it will be easy to use.

    You can define SCL2_JSON_ENABLE_EXTENSIONS before including this header,
    to enable some builtin features such as data URI and base64 string support.

    Even if they are enabled, they are still within standard JSON format. It just
    make it easier to use in some cases.

    Also check jbt if you want some even more compact storage of json data.

    [SCL_STANDALONE_MODULE]
    version: 1.3.0
*/

#pragma once

#include <string>
#include <variant>
#include <vector>
#include <map>
#include <optional>
#include <generator>
#include <stdexcept>

#ifdef SCL2_JSON_ENABLE_EXTENSIONS
    #include <cctype> // for std::isxdigit
    #include "bytearray.hpp"
    #include "datauri.hpp"
#endif

namespace scl2 {

// Helper: convert a u8 string literal to std::string.
// On MSVC, use with /utf-8 flag or u8"..." prefix:
//   j["name"] = json_value(json_u8(u8"你好"));
inline std::string json_u8(const char8_t* s) {
    return std::string(reinterpret_cast<const char*>(s));
}

class json_value;
class json;
class json_parser;
class json_exporter;

enum class json_value_type : uint8_t {
    null = 0,
    boolean = 1,
    integer = 2,
    floating = 3,
    string = 4,
    array = 5,
    object = 6,
#ifdef SCL2_JSON_ENABLE_EXTENSIONS
    bytearray = 7,
    data_uri = 8,
#endif
};
// highest 2 bits in the byte are reserved for mark bits.
// so we can only use 6 bytes, for up to 63 types, which is quite enough for any future
// use in json. we won't need more.

// abstract pointer layer.
class json_pointer {
public:
    json_pointer(const std::string& pointer_str);

    json_value& apply(const json_value& root) const;

    std::string to_string() const;

private:
    void split_tokens();
    std::string unescape_segment(std::string segment) const;
    json_value& apply_impl(const json_value& current, size_t depth) const;

    std::string pointer_str;
    std::vector<std::string> tokens;
};

class json_value {
public:
    json_value() = default;

    json_value(std::nullptr_t);
    json_value(bool b);
    json_value(int64_t i);
    json_value(double d);
    json_value(const std::string& s);
    json_value(std::string&& s);
    json_value(const std::vector<json_value>& arr);
    json_value(std::vector<json_value>&& arr);
    json_value(const std::map<std::string, json_value>& obj);
    json_value(std::map<std::string, json_value>&& obj);

    bool is_null() const;
    bool is_bool() const;
    bool is_int() const;
    bool is_double() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;

    nullptr_t as_null() const;
    bool as_bool() const;
    int64_t as_int() const;
    double as_double() const;
    const std::string& as_string() const;
    const std::vector<json_value>& as_array() const;
    const std::map<std::string, json_value>& as_object() const;

    nullptr_t& as_null();
    bool& as_bool();
    int64_t& as_int();
    double& as_double();
    std::string& as_string();
    std::vector<json_value>& as_array();
    std::map<std::string, json_value>& as_object();

#ifdef SCL2_JSON_ENABLE_EXTENSIONS
    json_value(std::bytearray&& ba);
    bool is_bytearray() const;
    const std::bytearray& as_bytearray() const;
    std::bytearray& as_bytearray();

    json_value(const inline_data_uri& data_uri);
    bool is_data_uri() const;
    const inline_data_uri& as_data_uri() const;
    inline_data_uri& as_data_uri();
#endif

    // for array
    std::generator<const json_value&> array_elements() const;
    json_value& operator[](size_t index);
    const json_value& operator[](size_t index) const;
    const json_value& at(size_t index) const;
    size_t array_size() const;

    // for object
    std::generator<std::pair<const std::string&, const json_value&>> object_members() const;
    bool has_key(const std::string& key) const;
    const json_value& operator[](const std::string& key) const;
    json_value& operator[](const std::string& key);
    const json_value& at(const std::string& key) const;
    size_t object_size() const;
    bool remove_member(const std::string& key);

    // for string
    size_t length() const;

    // universal
    // for array and object, return the size. For string, return length. For other types, return 0.
    size_t size() const;
    void clear(); // clear the value, set it to null

    // json pointer
    json_value& at(const json_pointer& pointer);
    const json_value& at(const json_pointer& pointer) const;

    json_value_type type() const;

    bool operator==(const json_value& other) const;
    bool operator!=(const json_value& other) const { return !(*this == other); }

private:
    std::variant<
        std::nullptr_t,
        bool,
        int64_t,
        double,
        std::string,
        std::vector<json_value>,
        std::map<std::string, json_value>
#ifdef SCL2_JSON_ENABLE_EXTENSIONS
        , std::bytearray
        , inline_data_uri
#endif
    > value;
};


class json : public json_value {
public:
    using json_value::json_value; // inherit constructors

    json() = default;
    json(json_value&& v);

    // static factory functions
    static json fromString(const std::string& str);
    static json fromFile(const std::string& filename);

    std::string toString() const;
    // remove any unnecessary whitespace.
    std::string toCompatString() const;

    // This uses the default format. If you want anything else, do it yourself.
    std::string toFile(const std::string& filename) const;

    inline json_value& value() { return *this; }
    inline const json_value& value() const { return *this; }

private:
    // this class has no secrets
};

/*
    This class is automatically called by json static factory,
    and user better not use it directly.
*/
class json_parser {
public:
    void parseFromString(const std::string& str_input);
    json&& getResult();

private:
    // helper functions
    void skipWhitespace();
    char peek() const;

    std::string parseJsonString();
    json_value parseJsonValue();
    
    json_value parseNull();
    json_value parseBool();
    json_value parseObject();
    json_value parseArray();
    json_value parseString();
    json_value parseNumber();

    std::pair<std::string, json_value> getNextObject();

    // assert functions
    void jexpect(char c, const char* error_message) const;
    void jexpect(const char* expected, const char* error_message) const;
    bool jcompare(const char* expected) const;
    void jinbound() const;
    void jinbound(const char* error_message) const;
    bool jisdigit(char c) const;

    std::string json_str;
    size_t pos;
    json result_root;
};


class json_exporter {
public:

    enum class indent_style {
        none, // no indentation, all in one line
        space2, // 2 spaces per indent level
        space4, // 4 spaces per indent level
        tab, // 1 tab per indent level
    };

    json_exporter() = default;
    
    static json_exporter compact_exporter();
    static json_exporter inline_exporter();

    std::string exportToString(const json& j);
    std::string exportToCompatString(const json& j);

    // just let user directly set these flags if needed.
    // We aren't multi-threading anyway.
    bool isCompat = false;
    bool isInline = false;
    bool escapeNonAscii = false;  // escape non-ASCII UTF-8 as \uXXXX for max portability
    indent_style indentStyle = indent_style::space4;

private:

    void exportValue(const json_value& value, size_t indentLevel);
    std::string escapeJsonString(const std::string& str);

    void exportKey(const std::string& key, size_t indentLevel);

    void exportNull(const json_value& value, size_t indentLevel);
    void exportBool(const json_value& value, size_t indentLevel);
    void exportObject(const json_value& value, size_t indentLevel);
    void exportArray(const json_value& value, size_t indentLevel);
    void exportString(const json_value& value, size_t indentLevel);
    void exportNumber(const json_value& value, size_t indentLevel);

    void jindent(size_t indentLevel);
    void jnline();
    std::string jquote(const std::string& str);

    size_t indentLevel = 0;
    std::string result_str;
};





} // namespace scl2