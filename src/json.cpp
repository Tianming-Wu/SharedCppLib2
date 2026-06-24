/*
    [SCL_STANDALONE_MODULE]
    version: 1.6.1
*/
#include "json.hpp"

#include <fstream>
#include <sstream>

namespace scl2 {

json_pointer::json_pointer(const std::string& pointer_str)
    : pointer_str(pointer_str)
{
    split_tokens();
}

json_value &json_pointer::apply(const json_value &root) const
{
    if (tokens.empty()) return const_cast<json_value&>(root); // empty pointer points to the whole document
    return apply_impl(root, 0);
}

std::string json_pointer::to_string() const
{
    return pointer_str;
}

void json_pointer::split_tokens()
{
    if (pointer_str.empty()) return; // empty pointer is valid and points to the whole document
    if (pointer_str[0] != '/')
        throw std::runtime_error("json_pointer::split_tokens: pointer must start with '/'");

    size_t pos = 0, lpos = 1;
    while ((pos = pointer_str.find('/', lpos)) != std::string::npos) {
        std::string current_token = pointer_str.substr(lpos, pos - lpos);
        tokens.push_back(unescape_segment(current_token));
        lpos = pos + 1;
    }

    // get the last token
    std::string last_token = pointer_str.substr(lpos);
    tokens.push_back(unescape_segment(last_token));
}

std::string json_pointer::unescape_segment(std::string segment) const
{
    if (segment.empty()) return "";
    size_t pos = 0, lpos = 0;
    while((pos = segment.find("~1", lpos)) != std::string::npos) { segment.replace(pos, 2, "/"); lpos = pos + 1; }
    pos = 0; lpos = 0;
    while((pos = segment.find("~0", lpos)) != std::string::npos) { segment.replace(pos, 2, "~"); lpos = pos + 1; }
    return segment;
}

json_value &json_pointer::apply_impl(const json_value &current, size_t depth) const
{
    const std::string& segment = tokens[depth];

    // Navigate into the current level
    const json_value* next = nullptr;
    if (current.is_array()) {
        size_t index;
        try { index = std::stoul(segment); } catch (const std::exception&) {
            throw std::runtime_error("json_pointer::apply: invalid array index: " + segment);
        }
        if (index >= current.array_size()) {
            throw std::runtime_error("json_pointer::apply: array index out of bounds: " + segment);
        }
        next = &current[index];
    } else if (current.is_object()) {
        if (!current.has_key(segment)) {
            throw std::runtime_error("json_pointer::apply: object key not found: " + segment);
        }
        next = &current[segment];
    } else {
        throw std::runtime_error("json_pointer::apply: cannot apply pointer to scalar value");
    }

    // Last token: return the target
    if (depth == tokens.size() - 1) return const_cast<json_value&>(*next);

    // Intermediate: recurse deeper
    return apply_impl(*next, depth + 1);
}

json_value::json_value(std::nullptr_t) : value(nullptr) {}
json_value::json_value(bool b) : value(b) {}
json_value::json_value(int64_t i) : value(i) {}
json_value::json_value(double d) : value(d) {}
json_value::json_value(const std::string& s) : value(s) {}
json_value::json_value(std::string&& s) : value(std::move(s)) {}
json_value::json_value(const std::vector<json_value>& arr) : value(arr) {}
json_value::json_value(std::vector<json_value>&& arr) : value(std::move(arr)) {}
json_value::json_value(const std::map<std::string, json_value>& obj) : value(obj) {}
json_value::json_value(std::map<std::string, json_value>&& obj) : value(std::move(obj)) {}

bool json_value::is_null() const { return std::holds_alternative<std::nullptr_t>(value); }
bool json_value::is_bool() const { return std::holds_alternative<bool>(value); }
bool json_value::is_int() const { return std::holds_alternative<int64_t>(value); }
bool json_value::is_double() const { return std::holds_alternative<double>(value); }
bool json_value::is_string() const { return std::holds_alternative<std::string>(value); }
bool json_value::is_array() const { return std::holds_alternative<std::vector<json_value>>(value); }
bool json_value::is_object() const { return std::holds_alternative<std::map<std::string, json_value>>(value); }


nullptr_t json_value::as_null() const { return nullptr; }
bool json_value::as_bool() const { return std::get<bool>(value); }
int64_t json_value::as_int() const { return std::get<int64_t>(value); }
double json_value::as_double() const { return std::get<double>(value); }
const std::string& json_value::as_string() const { return std::get<std::string>(value); }
const std::vector<json_value>& json_value::as_array() const { return std::get<std::vector<json_value>>(value); }
const std::map<std::string, json_value>& json_value::as_object() const { return std::get<std::map<std::string, json_value>>(value); }

nullptr_t& json_value::as_null() { return std::get<std::nullptr_t>(value); }
bool& json_value::as_bool() {
    if (is_null()) value = false;
    return std::get<bool>(value);
}
int64_t& json_value::as_int() {
    if (is_null()) value = int64_t{0};
    return std::get<int64_t>(value);
}
double& json_value::as_double() {
    if (is_null()) value = double{0};
    return std::get<double>(value);
}
std::string& json_value::as_string() {
    if (is_null()) value = std::string{};
    return std::get<std::string>(value);
}
std::vector<json_value>& json_value::as_array() {
    if (is_null()) value = std::vector<json_value>();
    return std::get<std::vector<json_value>>(value);
}
std::map<std::string, json_value>& json_value::as_object() {
    if (is_null()) value = std::map<std::string, json_value>();
    return std::get<std::map<std::string, json_value>>(value);
}

#ifdef SCL2_JSON_ENABLE_EXTENSIONS
    json_value::json_value(std::bytearray&& ba) : value(ba) {}
    bool json_value::is_bytearray() const { return std::holds_alternative<std::bytearray>(value); }
    const std::bytearray& json_value::as_bytearray() const { return std::get<std::bytearray>(value); }
    std::bytearray& json_value::as_bytearray() { return std::get<std::bytearray>(value); }

    json_value::json_value(const inline_data_uri &data_uri) : value(data_uri) {}
    bool json_value::is_data_uri() const { return std::holds_alternative<inline_data_uri>(value); }
    const inline_data_uri& json_value::as_data_uri() const { return std::get<inline_data_uri>(value); }
    inline_data_uri& json_value::as_data_uri() { return std::get<inline_data_uri>(value); }
#endif

#ifdef __cpp_lib_generator
std::generator<const json_value &> json_value::array_elements() const
{
    if (!is_array())
        throw std::runtime_error("json_value::array_elements: not an array");
    for (const auto &elem : std::get<std::vector<json_value>>(value))
    {
        co_yield elem;
    }
}
#endif

json_value &json_value::operator[](size_t index)
{
    if (is_null()) value = std::vector<json_value>();
    if (!is_array())
        throw std::runtime_error("json_value::operator[]: not an array");
    auto& arr = std::get<std::vector<json_value>>(value);
    if (index >= arr.size()) arr.resize(index + 1);
    return arr[index];
}

const json_value &json_value::operator[](size_t index) const
{
    if (!is_array())
        throw std::runtime_error("json_value::operator[]: not an array");
    return std::get<std::vector<json_value>>(value)[index];
}

const json_value &json_value::at(size_t index) const
{
    return operator[](index);
}

size_t json_value::array_size() const
{
    if (!is_array())
        throw std::runtime_error("json_value::array_size: not an array");
    return std::get<std::vector<json_value>>(value).size();
}

bool json_value::clear_as_array()
{
    if (!is_array()) return false;
    value = std::vector<json_value>();
    return true;
}

void json_value::push_back(const json_value& v)
{
    as_array().push_back(v);
}
void json_value::push_back(json_value&& v)
{
    as_array().push_back(std::move(v));
}

#ifdef __cpp_lib_generator
std::generator<std::pair<const std::string &, const json_value &>> json_value::object_members() const
{
    if (!is_object())
        throw std::runtime_error("json_value::object_members: not an object");
    for (const auto &pair : std::get<std::map<std::string, json_value>>(value))
    {
        co_yield pair;
    }
}
#endif

bool json_value::has_key(const std::string &key) const
{
    if (!is_object())
        throw std::runtime_error("json_value::has_key: not an object");
    return std::get<std::map<std::string, json_value>>(value).find(key) != std::get<std::map<std::string, json_value>>(value).end();
}

const json_value &json_value::operator[](const std::string &key) const
{
    if (!is_object())
        throw std::runtime_error("json_value::operator[]: not an object");
    return std::get<std::map<std::string, json_value>>(value).at(key);
}

json_value &json_value::operator[](const std::string &key)
{
    if (is_null()) value = std::map<std::string, json_value>();
    if (!is_object())
        throw std::runtime_error("json_value::operator[]: not an object");
    return std::get<std::map<std::string, json_value>>(value)[key];
}

const json_value &json_value::at(const std::string &key) const
{
    return operator[](key);
}

size_t json_value::object_size() const
{
    if (!is_object())
        throw std::runtime_error("json_value::object_size: not an object");
    return std::get<std::map<std::string, json_value>>(value).size();
}

size_t json_value::length() const
{
    if (!is_string())
        throw std::runtime_error("json_value::length: not a string");
    return as_string().size();
}

size_t json_value::size() const
{
    if (is_array())
        return array_size();
    else if (is_object())
        return object_size();
    else if (is_string())
        return length();
    else
        return 0;
}

void json_value::clear()
{
    value = nullptr;
}

json_value &json_value::at(const json_pointer &pointer)
{
    return pointer.apply(*this);
}

const json_value &json_value::at(const json_pointer &pointer) const
{
    return pointer.apply(*this);
}

json_value_type json_value::type() const
{
    if (is_null())
        return json_value_type::null;
    else if (is_bool())
        return json_value_type::boolean;
    else if (is_int())
        return json_value_type::integer;
    else if (is_double())
        return json_value_type::floating;
    else if (is_string())
        return json_value_type::string;
    else if (is_array())
        return json_value_type::array;
    else if (is_object())
        return json_value_type::object;
#ifdef SCL2_JSON_ENABLE_EXTENSIONS
    else if (is_bytearray())
        return json_value_type::bytearray;
    else if (is_data_uri())
        return json_value_type::data_uri;
#endif
    else
        throw std::runtime_error("json_value::type: unknown type");
}

bool json_value::remove_member(const std::string& key)
{
    if (!is_object()) return false;
    return as_object().erase(key) > 0;
}

bool json_value::clear_as_object()
{
    if (!is_object()) return false;
    as_object().clear();
    return true;
}

bool json_value::operator==(const json_value& other) const
{
    if (value.index() != other.value.index()) return false;
    return std::visit([&other](const auto& a) -> bool {
        using T = std::decay_t<decltype(a)>;
        return a == std::get<T>(other.value);
    }, value);
}

json::json(json_value &&v)
    : json_value(std::move(v))
{
}

json json::fromString(const std::string &str)
{
    json_parser parser;
    parser.parseFromString(str);
    return parser.getResult();
}

json json::fromFile(const std::string &filename)
{
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Failed to open JSON file: " + filename);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return fromString(buffer.str());
}

std::string json::toString() const
{
    json_exporter exporter;
    return exporter.exportToString(*this);
}

std::string json::toCompatString() const
{
    json_exporter exporter;
    exporter.isCompat = true;
    return exporter.exportToString(*this);
}

std::string json::toFile(const std::string &filename) const
{
    std::ofstream file(filename);
    if (!file) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }
    file << toString();
    return filename;
}

void json_parser::parseFromString(const std::string &str_input)
{
    result_root.clear();
    pos = 0;
    json_str = str_input;

    if (json_str.empty())
    {
        // A json file should have at least {}, so empty string is not valid.
        // User is responsible to check if the input string is empty, and define default
        // behavior based on that. We should not do it here.
        throw std::runtime_error("Input JSON string is empty");
    }

    result_root = parseJsonValue();
}

json &&json_parser::getResult()
{
    return std::move(result_root);
}

void json_parser::skipWhitespace()
{
    while (pos < json_str.size()) {
        char c = json_str[pos];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++pos;
            continue;
        }
#ifdef SCL2_JSON_ENABLE_COMMENTS
        // Line comment: skip until newline or EOF
        if (c == '/' && pos + 1 < json_str.size() && json_str[pos + 1] == '/') {
            pos += 2;
            while (pos < json_str.size() && json_str[pos] != '\n') ++pos;
            continue;
        }
        // Block comment: skip until */
        if (c == '/' && pos + 1 < json_str.size() && json_str[pos + 1] == '*') {
            pos += 2;
            while (pos + 1 < json_str.size()) {
                if (json_str[pos] == '*' && json_str[pos + 1] == '/') {
                    pos += 2;
                    break;
                }
                ++pos;
            }
            continue;
        }
#endif
        break;
    }
}

char json_parser::peek() const
{
    if (pos < json_str.size())
    {
        return json_str[pos];
    }
    return 0;
}

std::string json_parser::parseJsonString()
{
    // We should be at the opening quote.
    jexpect('"', "Expected '\"' at the beginning of JSON string");

    ++pos; // skip opening quote

    std::string result;

    while (pos < json_str.size())
    {
        char c = json_str[pos];

        if (c == '\\')
        {
            // Escape sequence
            ++pos;
            if (pos >= json_str.size())
            {
                throw std::runtime_error("Unterminated escape sequence in JSON string");
            }
            char escaped = json_str[pos];
            switch (escaped)
            {
            case '"':  result += '"';  break;
            case '\\': result += '\\'; break;
            case '/':  result += '/';  break;
            case 'b':  result += '\b'; break;
            case 'f':  result += '\f'; break;
            case 'n':  result += '\n'; break;
            case 'r':  result += '\r'; break;
            case 't':  result += '\t'; break;
            case 'u': {
                // \uXXXX
                if (pos + 4 >= json_str.size()) {
                    throw std::runtime_error("Incomplete Unicode escape in JSON string");
                }
                std::string hex = json_str.substr(pos + 1, 4);
                uint32_t codepoint = std::stoul(hex, nullptr, 16);
                // Encode as UTF-8
                if (codepoint <= 0x7F) {
                    result += static_cast<char>(codepoint);
                } else if (codepoint <= 0x7FF) {
                    result += static_cast<char>(0xC0 | (codepoint >> 6));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                } else {
                    result += static_cast<char>(0xE0 | (codepoint >> 12));
                    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
                pos += 4;
                break;
            }
#ifdef SCL2_JSON_ENABLE_EXTENSIONS
            case 'x': {
                // \xNN
                if (pos + 2 >= json_str.size()) {
                    throw std::runtime_error("Incomplete hex escape in JSON string");
                }
                std::string hex = json_str.substr(pos + 1, 2);
                result += static_cast<char>(std::stoi(hex, nullptr, 16));
                pos += 2;
                break;
            }
#endif
            default:
                throw std::runtime_error(std::string("Invalid escape character in JSON string: \\") + escaped);
            }
        } else if (c == '"') {
            // Unescaped quote - end of string
            ++pos; // skip closing quote
            return result;
        } else {
            // Regular character (including spaces, unicode, etc.)
            result += c;
        }
        ++pos;
    }

    throw std::runtime_error("Unterminated JSON string");
}

json_value json_parser::parseJsonValue()
{
    char c = peek();
    if (c == '{') {
        return parseObject();
    } else if (c == '[') {
        return parseArray();
    } else if (c == '"') {
        return parseString();
    } else if (std::isdigit(c) || c == '-') {
        return parseNumber();
    } else if (c == 't' || c == 'f') {
        return parseBool();
    } else if (c == 'n') {
        return parseNull();
    } else {
        throw std::runtime_error(std::string("Unexpected character in JSON input: ") + c);
    }
}

json_value json_parser::parseNull()
{
    jexpect("null", "Expected 'null'");
    pos += 4;
    return json_value(nullptr);
}

json_value json_parser::parseBool()
{
    if (json_str.compare(pos, 4, "true") == 0) {
        pos += 4;
        return json_value(true);
    } else if (json_str.compare(pos, 5, "false") == 0) {
        pos += 5;
        return json_value(false);
    } else {
        throw std::runtime_error("Invalid JSON boolean value");
    }
}

json_value json_parser::parseObject()
{
    // create an empty object
    json_value obj = std::map<std::string, json_value>();

    // We should be at the opening brace.
    jexpect('{', "Expected '{' at the beginning of JSON object");
    ++pos; // skip opening brace

    if (peek() == '}') {
        ++pos; // skip closing brace
        return obj; // empty object
    }

    while(true) {
        auto p = getNextObject();
        obj.as_object().insert(std::move(p));

        skipWhitespace();
        // deal with comma or closing brace
        jinbound("Unexpected end of JSON input while parsing object");

        char c = peek();
        if (c == ',') {
            ++pos; // skip comma and continue to next pair
            skipWhitespace();
        } else if (c == '}') {
            ++pos; // skip closing brace and break
            break;
        } else {
            throw std::runtime_error(std::string("Expected ',' or '}' in JSON object, but got: ") + c);
        }
    }

    return obj;
}

json_value json_parser::parseArray()
{
    // create an empty array
    json_value arr = std::vector<json_value>();

    // We should be at the opening bracket.
    jexpect('[', "Expected '[' at the beginning of JSON array");
    ++pos; // skip opening bracket

    if (peek() == ']') {
        ++pos; // skip closing bracket
        return arr; // empty array
    }

    while(true) {
        skipWhitespace();
        auto v = parseJsonValue();
        arr.as_array().push_back(std::move(v));

        skipWhitespace();
        // deal with comma or closing brace
        jinbound("Unexpected end of JSON input while parsing array");

        char c = peek();
        if (c == ',') {
            ++pos; // skip comma and continue to next pair
            skipWhitespace();
        } else if (c == ']') {
            ++pos; // skip closing bracket and break
            break;
        } else {
            throw std::runtime_error(std::string("Expected ',' or ']' in JSON array, but got: ") + c);
        }
    }

    return arr;
}

json_value json_parser::parseString()
{
    // parseJsonString already handled the position.
    // Simply call it is enough.

    std::string str = parseJsonString();

#ifdef SCL2_JSON_ENABLE_EXTENSIONS
    if (!str.empty())
        if (str.starts_with("data:")) {
            // data URI, parse it to inline_data_uri
            return json_value(inline_data_uri::from_string(str));
        } else if (str.starts_with("base64:")) {
            // base64 string, decode it to bytearray
            return json_value(std::bytearray::fromBase64(str.substr(7)));
        }
#endif

    return json_value(str);
}

json_value json_parser::parseNumber()
{
    jinbound();

    bool is_floating = false;
    size_t start_pos = pos;

    // Optional minus
    if (json_str[pos] == '-') {
        ++pos;
    }

    jinbound("Invalid JSON number: expected digit");

    // Integer part
    if (json_str[pos] == '0') {
        ++pos;
        // Leading zero is only valid for the number 0 itself.
        // "01", "007" etc. are illegal in JSON.
        if (pos < json_str.size() && jisdigit(json_str[pos])) {
            throw std::runtime_error("Invalid JSON number: leading zero is not allowed");
        }
    } else if (jisdigit(json_str[pos])) {
        while (pos < json_str.size() && jisdigit(json_str[pos])) {
            ++pos;
        }
    } else {
        throw std::runtime_error("Invalid JSON number: expected digit");
    }

    // Fractional part
    if (pos < json_str.size() && json_str[pos] == '.') {
        is_floating = true;
        ++pos;
        jinbound("Invalid JSON number: expected digit after decimal point");
        if (!jisdigit(json_str[pos])) {
            throw std::runtime_error("Invalid JSON number: expected digit after decimal point");
        }
        while (pos < json_str.size() && jisdigit(json_str[pos])) {
            ++pos;
        }
    }

    // Exponent part
    if (pos < json_str.size() && (json_str[pos] == 'e' || json_str[pos] == 'E')) {
        is_floating = true;
        ++pos;
        jinbound("Invalid JSON number: expected digit in exponent");
        if (json_str[pos] == '+' || json_str[pos] == '-') {
            ++pos;
            jinbound("Invalid JSON number: expected digit in exponent");
        }
        if (!jisdigit(json_str[pos])) {
            throw std::runtime_error("Invalid JSON number: expected digit in exponent");
        }
        while (pos < json_str.size() && jisdigit(json_str[pos])) {
            ++pos;
        }
    }

    std::string num_str = json_str.substr(start_pos, pos - start_pos);

    if (is_floating) {
        return json_value(std::stod(num_str));
    } else {
        try {
            return json_value(static_cast<int64_t>(std::stoll(num_str)));
        } catch (const std::out_of_range&) {
            // Integer too large, fall back to double
            return json_value(std::stod(num_str));
        }
    }
}

std::pair<std::string, json_value> json_parser::getNextObject()
{
    // parse the "name" : value pair in object, or the value in array.
    skipWhitespace();
    if (pos >= json_str.size()) {
        throw std::runtime_error("Unexpected end of JSON input while parsing element");
    }

    char c = peek();
    jexpect('"', "Expected '\"' at the beginning of JSON element name");

    std::string name = parseJsonString();
    skipWhitespace();
    if (pos >= json_str.size() || json_str[pos] != ':') {
        throw std::runtime_error("Expected ':' after JSON element name");
    }
    ++pos; // skip colon
    skipWhitespace();

    // here is the value part.
    auto p = std::make_pair(name, parseJsonValue());
    return p;
}


void json_parser::jexpect(char c, const char *error_message) const
{
    if (peek() != c) {
        throw std::runtime_error(error_message);
    }
}

void json_parser::jexpect(const char *expected, const char *error_message) const
{
    jinbound(error_message);
    size_t len = std::strlen(expected);
    if (json_str.compare(pos, len, expected) != 0) {
        throw std::runtime_error(error_message);
    }
}

bool json_parser::jcompare(const char *expected) const
{
    return json_str.compare(pos, std::strlen(expected), expected) == 0;
}

void json_parser::jinbound() const
{
    if (pos >= json_str.size()) {
        throw std::runtime_error("Unexpected end of JSON input");
    }
}

void json_parser::jinbound(const char *error_message) const
{
    if (pos >= json_str.size()) {
        throw std::runtime_error(error_message);
    }
}

bool json_parser::jisdigit(char c) const
{
    return std::isdigit(static_cast<unsigned char>(c));
}

json_exporter json_exporter::compact_exporter()
{
    json_exporter exporter;
    exporter.isCompat = true;
    exporter.escapeNonAscii = true;
    exporter.indentStyle = indent_style::none;
    return exporter;
}

json_exporter json_exporter::inline_exporter()
{
    json_exporter exporter;
    exporter.isInline = true;
    exporter.indentStyle = indent_style::none;
    return exporter;
}

std::string json_exporter::exportToString(const json &j)
{
    result_str.clear();
    exportValue(j, 0);
    return result_str;
}

std::string json_exporter::exportToCompatString(const json &j)
{
    result_str.clear();
    isCompat = true;
    exportValue(j, 0);
    return result_str;
}

void json_exporter::exportValue(const json_value &value, size_t indentLevel)
{
    switch (value.type()) {
        case json_value_type::null:    exportNull(value, indentLevel);   break;
        case json_value_type::boolean: exportBool(value, indentLevel);   break;
        case json_value_type::integer:
        case json_value_type::floating: exportNumber(value, indentLevel); break;
        case json_value_type::string:  exportString(value, indentLevel); break;
        case json_value_type::array:   exportArray(value, indentLevel);  break;
        case json_value_type::object:  exportObject(value, indentLevel); break;
#ifdef SCL2_JSON_ENABLE_EXTENSIONS
        case json_value_type::bytearray:
            result_str += jquote("base64:" + value.as_bytearray().toBase64());
            break;
        case json_value_type::data_uri:
            result_str += jquote(value.as_data_uri().to_string());
            break;
#endif
    }
}

std::string json_exporter::escapeJsonString(const std::string &str)
{
    std::string escaped;
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = static_cast<unsigned char>(str[i]);

        // Handle standard JSON escapes
        switch (c) {
            case '"':  escaped += "\\\""; ++i; continue;
            case '\\': escaped += "\\\\"; ++i; continue;
            case '\b': escaped += "\\b";  ++i; continue;
            case '\f': escaped += "\\f";  ++i; continue;
            case '\n': escaped += "\\n";  ++i; continue;
            case '\r': escaped += "\\r";  ++i; continue;
            case '\t': escaped += "\\t";  ++i; continue;
        }

        // Control characters -> \uXXXX
        if (c < 0x20) {
            char buf[7];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            escaped += buf;
            ++i;
            continue;
        }

        // ASCII printable -> pass through
        if (c < 0x80) {
            escaped += static_cast<char>(c);
            ++i;
            continue;
        }

        // Non-ASCII byte -> either escape as \uXXXX or pass through as UTF-8
        if (escapeNonAscii) {
            // Decode UTF-8 sequence and emit \uXXXX (or \uXXXX\uXXXX for surrogates)
            auto decode_utf8 = [&](size_t& pos) -> int {
                unsigned char b = static_cast<unsigned char>(str[pos]);
                int cp, extra;
                if      ((b & 0xE0) == 0xC0) { cp = b & 0x1F; extra = 1; }
                else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; extra = 2; }
                else if ((b & 0xF8) == 0xF0) { cp = b & 0x07; extra = 3; }
                else { return -1; } // invalid byte - escape as-is
                for (int j = 0; j < extra; ++j) {
                    ++pos;
                    if (pos >= str.size()) return -1;
                    unsigned char nb = static_cast<unsigned char>(str[pos]);
                    if ((nb & 0xC0) != 0x80) return -1;
                    cp = (cp << 6) | (nb & 0x3F);
                }
                return cp;
            };
            int cp = decode_utf8(i);
            if (cp >= 0) {
                if (cp > 0xFFFF) {
                    // Surrogate pair for code points > U+FFFF
                    cp -= 0x10000;
                    char buf[13];
                    std::snprintf(buf, sizeof(buf), "\\u%04x\\u%04x",
                                  0xD800 | (cp >> 10), 0xDC00 | (cp & 0x3FF));
                    escaped += buf;
                } else {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", cp);
                    escaped += buf;
                }
                ++i; // decode_utf8 advanced i to the last byte
            } else {
                // Invalid UTF-8 - escape the single byte
                char buf[7];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                escaped += buf;
            }
            ++i;
        } else {
            // Pass through UTF-8 bytes unchanged
            escaped += static_cast<char>(c);
            ++i;
        }
    }
    return escaped;
}

void json_exporter::exportKey(const std::string &value, size_t indentLevel)
{
    jindent(indentLevel);
    result_str += std::string(
        jquote(escapeJsonString(value)) +
        (isCompat ? ":" : ": ")
    );
}

void json_exporter::exportNull(const json_value& value, size_t indentLevel)
{
    (void)indentLevel;
    result_str += "null";
}

void json_exporter::exportBool(const json_value& value, size_t indentLevel)
{
    (void)indentLevel;
    result_str += value.as_bool() ? "true" : "false";
}

void json_exporter::exportObject(const json_value& value, size_t indentLevel)
{
    result_str += "{";

    if (value.as_object().empty()) {
        result_str += "}";
        return;
    }

    jnline();

    for (auto it = value.as_object().begin(); it != value.as_object().end(); ++it) {
        exportKey(it->first, indentLevel + 1);

        // Non-empty containers: exportKey already provides ": " - bracket follows inline.
        // Empty arrays/objects stay compact: "key": []

        exportValue(it->second, indentLevel + 1);
        if (std::next(it) != value.as_object().end()) result_str += ",";
        jnline();
    }

    jindent(indentLevel);
    result_str += "}";
}

void json_exporter::exportArray(const json_value& value, size_t indentLevel)
{
    result_str += "[";

    if (value.as_array().empty()) {
        result_str += "]";
        return;
    }

    jnline();

    for (size_t i = 0; i < value.as_array().size(); ++i) {
        jindent(indentLevel + 1);
        exportValue(value.as_array()[i], indentLevel + 1);
        if (i != value.as_array().size() - 1) result_str += ",";
        jnline();
    }

    jindent(indentLevel);
    result_str += "]";
}

void json_exporter::exportString(const json_value& value, size_t indentLevel)
{
    (void)indentLevel;
    result_str += jquote(escapeJsonString(value.as_string()));
}

void json_exporter::exportNumber(const json_value &value, size_t indentLevel)
{
    (void)indentLevel;
    if (value.is_int()) {
        result_str += std::to_string(value.as_int());
    } else {
        result_str += std::to_string(value.as_double());
    }
}

void json_exporter::jindent(size_t indentLevel)
{
    if (isCompat || isInline) return;
    result_str += [this, indentLevel]() {
        switch (indentStyle) {
            case indent_style::none:
                return std::string();
            case indent_style::space2:
                return std::string(indentLevel * 2, ' ');
            case indent_style::space4:
                return std::string(indentLevel * 4, ' ');
            case indent_style::tab:
                return std::string(indentLevel, '\t');
            default:
                throw std::runtime_error("Invalid indent style");
        }
    }();
}

void json_exporter::jnline()
{
    if (isCompat) return;
    if (isInline) { result_str += " "; return; }
    result_str += "\n";
}

std::string json_exporter::jquote(const std::string &str)
{
    return std::string("\"" + escapeJsonString(str) + "\"");
}

} // namespace scl2