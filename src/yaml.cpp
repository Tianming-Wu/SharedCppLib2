/*
    [SCL_STANDALONE_MODULE]
    version: 0.1.0
*/

#include "yaml.hpp"

#include <stdexcept>
#include <algorithm>

namespace scl2::yaml {


value::value(std::nullptr_t) : _value(nullptr) {}
value::value(bool b) : _value(b) {}
value::value(int64_t i) : _value(i) {}
value::value(double d) : _value(d) {}
value::value(const std::string& s) : _value(s) {}
value::value(std::string&& s) : _value(std::move(s)) {}
value::value(const std::vector<value>& arr) : _value(arr) {}
value::value(std::vector<value>&& arr) : _value(std::move(arr)) {}
value::value(const std::map<std::string, value>& obj) : _value(obj) {}
value::value(std::map<std::string, value>&& obj) : _value(std::move(obj)) {}

bool value::is_null() const { return std::holds_alternative<std::nullptr_t>(_value); }
bool value::is_bool() const { return std::holds_alternative<bool>(_value); }
bool value::is_int() const { return std::holds_alternative<int64_t>(_value); }
bool value::is_double() const { return std::holds_alternative<double>(_value); }
bool value::is_string() const { return std::holds_alternative<std::string>(_value); }
bool value::is_array() const { return std::holds_alternative<std::vector<value>>(_value); }
bool value::is_object() const { return std::holds_alternative<std::map<std::string, value>>(_value); }
bool value::is_alias() const { return std::holds_alternative<alias_ref>(_value); }

nullptr_t value::as_null() const { return nullptr; }
bool value::as_bool() const { return std::get<bool>(_value); }
int64_t value::as_int() const { return std::get<int64_t>(_value); }
double value::as_double() const { return std::get<double>(_value); }
const std::string& value::as_string() const { return std::get<std::string>(_value); }
const std::vector<value>& value::as_array() const { return std::get<std::vector<value>>(_value); }
const std::map<std::string, value>& value::as_object() const { return std::get<std::map<std::string, value>>(_value); }

nullptr_t& value::as_null() { return std::get<std::nullptr_t>(_value); }
bool& value::as_bool() {
    if (is_null()) _value = false;
    return std::get<bool>(_value);
}
int64_t& value::as_int() {
    if (is_null()) _value = int64_t{0};
    return std::get<int64_t>(_value);
}
double& value::as_double() {
    if (is_null()) _value = double{0};
    return std::get<double>(_value);
}
std::string& value::as_string() {
    if (is_null()) _value = std::string{};
    return std::get<std::string>(_value);
}
std::vector<value>& value::as_array() {
    if (is_null()) _value = std::vector<value>();
    return std::get<std::vector<value>>(_value);
}
std::map<std::string, value>& value::as_object() {
    if (is_null()) _value = std::map<std::string, value>();
    return std::get<std::map<std::string, value>>(_value);
}

const alias_ref& value::as_alias() const { return std::get<alias_ref>(_value); }
alias_ref& value::as_alias() { return std::get<alias_ref>(_value); }

#ifdef __cpp_lib_generator
std::generator<const value &> value::array_elements() const
{
    if (!is_array())
        throw yaml_exception("value::array_elements: not an array");
    for (const auto &elem : std::get<std::vector<value>>(_value))
    {
        co_yield elem;
    }
}
#endif

value &value::operator[](size_t index)
{
    if (is_null()) _value = std::vector<value>();
    if (!is_array())
        throw yaml_exception("value::operator[]: not an array");
    auto& arr = std::get<std::vector<value>>(_value);
    if (index >= arr.size()) arr.resize(index + 1);
    return arr[index];
}

const value &value::operator[](size_t index) const
{
    if (!is_array())
        throw yaml_exception("value::operator[]: not an array");
    return std::get<std::vector<value>>(_value)[index];
}

const value &value::at(size_t index) const
{
    return operator[](index);
}

size_t value::array_size() const
{
    if (!is_array())
        throw yaml_exception("value::array_size: not an array");
    return std::get<std::vector<value>>(_value).size();
}

void value::push_back(const value& v)
{
    as_array().push_back(v);
}
void value::push_back(value&& v)
{
    as_array().push_back(std::move(v));
}

#ifdef __cpp_lib_generator
std::generator<std::pair<const std::string &, const value &>> value::object_members() const
{
    if (!is_object())
        throw yaml_exception("value::object_members: not an object");
    for (const auto &pair : std::get<std::map<std::string, value>>(_value))
    {
        co_yield pair;
    }
}
#endif

bool value::has_key(const std::string &key) const
{
    if (!is_object())
        throw yaml_exception("value::has_key: not an object");
    return std::get<std::map<std::string, value>>(_value).find(key) != std::get<std::map<std::string, value>>(_value).end();
}

const value &value::operator[](const std::string &key) const
{
    if (!is_object())
        throw yaml_exception("value::operator[]: not an object");
    return std::get<std::map<std::string, value>>(_value).at(key);
}

value &value::operator[](const std::string &key)
{
    if (is_null()) _value = std::map<std::string, value>();
    if (!is_object())
        throw yaml_exception("value::operator[]: not an object");
    return std::get<std::map<std::string, value>>(_value)[key];
}

const value &value::at(const std::string &key) const
{
    return operator[](key);
}

size_t value::object_size() const
{
    if (!is_object())
        throw yaml_exception("value::object_size: not an object");
    return std::get<std::map<std::string, value>>(_value).size();
}

size_t value::length() const
{
    if (!is_string())
        throw yaml_exception("value::length: not a string");
    return as_string().size();
}

size_t value::size() const
{
    if (is_array())
        return array_size();
    else if (is_object())
        return object_size();
    else if (is_string())
        return length();
    else if (is_alias())
        return 0;
    else
        return 0;
}

void value::clear()
{
    _value = nullptr;
}

type value::type() const
{
    return static_cast<scl2::yaml::type>(_value.index());
}

bool value::remove_member(const std::string& key)
{
    if (!is_object()) return false;
    return as_object().erase(key) > 0;
}

bool value::operator==(const value& other) const
{
    if (_value.index() != other._value.index()) return false;
    return std::visit([&other](const auto& a) -> bool {
        using T = std::decay_t<decltype(a)>;
        return a == std::get<T>(other._value);
    }, _value);
}

document parser::fromStream(std::istream &input, features feat)
{
    parser p;
    p.feat = feat;

    document result;
    if (!p.parseNext(input, result))
        throw yaml_exception("fromStream: failed to parse input");
    return result;
}

document parser::fromString(const std::string &input, features feat)
{
    auto stream = std::istringstream(input);
    return fromStream(stream, feat);
}

bool parser::parseNext(std::istream &input, document &out)
{
    if (input.eof())
        return false;

    // Push root frame
    _stack.clear();
    _stack.push_back({0, value(), false});

    while (readLine(input)) {
        parseLine();
    }

    // Pop all remaining frames so parent containers receive their children
    while (_stack.size() > 1) {
        popIndent();
    }

    if (!_stack.empty()) {
        out = document(std::move(_stack[0].container));
        return true;
    }
    return false;
}

document document::fromStream(std::istream& input, features feat)
{
    return parser::fromStream(input, feat);
}

document document::fromString(const std::string& input, features feat)
{
    return parser::fromString(input, feat);
}

bool parser::readLine(std::istream &input)
{
    _line_num += 1;
    _line_pos = 0; // reset line cursor
    return std::getline(input, _line) ? true : false;
}

void parser::parseLine()
{
    // we should have a full string in line buffer.
    // now parse the indentation, and trim the content.

    _indent_level = countIndentLevel();

    while (!_stack.empty() && _indent_level < _stack.back().indent_level) {
        popIndent();
    }

    // Push if indent is deeper than current scope
    if (_stack.empty() || _indent_level > _stack.back().indent_level) {
        pushIndent(_indent_level, false);
        // Inherit pending_key from parent as this frame's parent_key
        if (_stack.size() >= 2) {
            _stack.back().parent_key = std::move(_stack[_stack.size() - 2].pending_key);
        }
    }

    _line = stripComments(_line);
    if (_line.empty()) return; // empty line, skip

    // Trim leading whitespace (indent was already counted)
    _line = _line.substr(_line_pos);

    // ---- Detect : -> mapping key (handles both "key: value" and "- key: value") ----
    // Find first unquoted colon
    size_t colon = std::string::npos;
    {
        bool in_quote = false;
        char q = 0;
        for (size_t i = 0; i < _line.size(); ++i) {
            char c = _line[i];
            if ((c == '"' || c == '\'') && !in_quote) { in_quote = true; q = c; }
            else if (in_quote && c == q) { in_quote = false; }
            else if (c == ':' && !in_quote) { colon = i; break; }
        }
    }

    if (colon != std::string::npos) {
        std::string key = _line.substr(0, colon);
        // trim trailing whitespace from key
        while (!key.empty() && std::isspace(key.back())) key.pop_back();
        bool is_seq_entry = (key.size() >= 2 && key[0] == '-' && key[1] == ' ');
        if (is_seq_entry) key = key.substr(2);

        // If this is a sequence entry with a mapping, create the sequence + sub-frame
        if (is_seq_entry) {
            auto& parent = _stack.back();
            if (parent.is_mapping || parent.container.is_null()) {
                parent.is_mapping = false;
                parent.container = value(std::vector<value>{});
            }
            // Push new map as sequence element
            parent.container.as_array().push_back(value(std::map<std::string, value>{}));
            size_t parent_idx = _stack.size() - 1; // save before push - realloc invalidates parent ref
            // Push sub-frame for this map
            pushIndent(_indent_level + 1, true);
            _stack.back().container = _stack[parent_idx].container.as_array().back();
            _stack.back().pending_key = key;
        }

        dispatch_mapping_key(key);

        std::string value = _line.substr(colon + 1);
        // trim leading whitespace
        size_t vstart = 0;
        while (vstart < value.size() && std::isspace(value[vstart])) ++vstart;
        value = value.substr(vstart);

        if (!value.empty()) {
            dispatch_scalar(value);
        }
        return;
    }

    // ---- Detect - -> sequence entry ----
    if (_line.size() >= 2 && _line[0] == '-' && _line[1] == ' ') {
        dispatch_sequence_entry();
        return;
    }

    // ---- Otherwise -> scalar ----
    dispatch_scalar(_line);
}

void parser::dispatch_scalar(const std::string &text)
{
    // Quoted text is always a string - skip type detection entirely
    if (text.size() >= 2) {
        char first = text.front(), last = text.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            // trim the quotes, and for double-quoted, also process escape sequences
            std::string inner = text.substr(1, text.size() - 2);
            if (first == '"') {
                inner = processDoubleQuotedEscapes(inner);
            }
            // Single-quoted: no escape processing (even \ is literal)
            addValue(value(std::move(inner)));
            return;
        }
    }

    // Unquoted -> type detection
    if (isNull(text)) {
        addValue(value(nullptr));
    } else if (isBool(text)) {
        addValue(value(text == "true" || text == "True" || text == "TRUE"
                    || text == "yes" || text == "Yes" || text == "YES"
                    || text == "on" || text == "On" || text == "ON"));
    } else if (isInteger(text)) {
        addValue(value(std::stoll(text)));
    } else if (isDouble(text)) {
        addValue(value(std::stod(text)));
    } else {
        addValue(value(text));
    }
}

void parser::dispatch_mapping_key(const std::string &key)
{
    auto& frame = _stack.back();
    if (!frame.is_mapping || frame.container.is_null()) {
        frame.is_mapping = true;
        frame.container = value(std::map<std::string, value>{});
    }
    frame.pending_key = key;
    // Placeholder - overwritten by addValue if inline value follows
    frame.container.as_object()[key] = value(nullptr);
}

void parser::dispatch_sequence_entry()
{
    auto& frame = _stack.back();
    if (frame.is_mapping || frame.container.is_null()) {
        frame.is_mapping = false;
        frame.container = value(std::vector<value>{});
    }
    std::string rest = _line.substr(2); // trim "- "
    // trim leading whitespace
    size_t start = 0;
    while (start < rest.size() && std::isspace(rest[start])) ++start;
    rest = rest.substr(start);

    if (rest.empty()) {
        // "- " empty entry - placeholder null
        frame.container.as_array().push_back(value(nullptr));
    } else {
        // "- value" inline scalar
        dispatch_scalar(rest);
    }
}

bool parser::isNull(const std::string &s)
{
    return (s == "null" || s == "Null" || s == "NULL" || s == "~");
}

bool parser::isBool(const std::string &s)
{
    return (s == "true" || s == "True" || s == "TRUE" || s == "false" || s == "False" || s == "FALSE"
            || s == "yes" || s == "Yes" || s == "YES" || s == "no" || s == "No" || s == "NO"
            || s == "on" || s == "On" || s == "ON" || s == "off" || s == "Off" || s == "OFF");
}

bool parser::isInteger(const std::string &s)
{
    ///TODO: consider switching to regex.
    return !s.empty() && (s[0] == '-' || s[0] == '+' || isdigit(s[0])) && std::all_of(s.begin() + 1, s.end(), ::isdigit);
}

bool parser::isDouble(const std::string &s)
{
    if (s.empty()) return false;
    size_t i = 0;
    if (s[i] == '-' || s[i] == '+') ++i;
    if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    bool has_dot = false;
    for (; i < s.size(); ++i) {
        char c = s[i];
        if (c == '.') {
            if (has_dot) return false; // only one dot
            has_dot = true;
        } else if (c == 'e' || c == 'E') {
            // exponent: optional +/-, then digits
            ++i;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
            if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) return false;
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
            return i == s.size();
        } else if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return has_dot; // must have a dot (or exponent) to be a double
}

std::string parser::processDoubleQuotedEscapes(const std::string &s)
{
    // we get the string without quotes here.

    std::string result;
    size_t pos = 0;
    while (pos < s.size())
    {
        char c = s[pos];

        if (c == '\\')
        {
            // Escape sequence
            ++pos;
            if (pos >= s.size())
            {
                throw yaml_exception("Unterminated escape sequence in string");
            }
            char escaped = s[pos];
            switch (escaped)
            {
            case '"':  result += '"';  break;
            case '\\': result += '\\'; break;
            case 'b':  result += '\b'; break;
            case 'f':  result += '\f'; break;
            case 'n':  result += '\n'; break;
            case 'r':  result += '\r'; break;
            case 't':  result += '\t'; break;
            case 'a':  result += '\a'; break;
            case 'v':  result += '\v'; break;
            case 'e':  result += '\x1B'; break;
            case 'u': {
                // \uXXXX
                if (pos + 4 >= s.size()) {
                    throw yaml_exception("Incomplete Unicode escape in string");
                }
                std::string hex = s.substr(pos + 1, 4);
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
            case 'U': {
                // \uXXXXXXXX
                if (pos + 8 >= s.size()) {
                    throw yaml_exception("Incomplete Unicode escape in string");
                }
                std::string hex = s.substr(pos + 1, 8);
                uint32_t codepoint = std::stoul(hex, nullptr, 16);
                // Encode as UTF-8
                if (codepoint <= 0x7F) {
                    result += static_cast<char>(codepoint);
                } else if (codepoint <= 0x7FF) {
                    result += static_cast<char>(0xC0 | (codepoint >> 6));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                } else if (codepoint <= 0xFFFF) {
                    result += static_cast<char>(0xE0 | (codepoint >> 12));
                    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                } else {
                    result += static_cast<char>(0xF0 | (codepoint >> 18));
                    result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
                pos += 8;
                break;
            }
            case 'x': {
                // \xNN
                if (pos + 2 >= s.size()) {
                    throw yaml_exception("Incomplete hex escape in string");
                }
                std::string hex = s.substr(pos + 1, 2);
                result += static_cast<char>(std::stoi(hex, nullptr, 16));
                pos += 2;
                break;
            }
            default:
                throw yaml_exception(std::string("Invalid escape character in string: \\") + escaped);
            }
        } else {
            // Regular character (including spaces, unicode, etc.)
            result += c;
        }
        ++pos;
    }

    return result;
}

int parser::countIndentLevel()
{
    int count = 0;
    for (char c : _line) {
        if (c == ' ') {
            count += 1;
            _line_pos += 1;
        } else if (c == '\t') {
            throw yaml_exception("tabs are not allowed for indentation");
        } else break;
    }
    if (count != 0) {
        if (_indent_unit == 0) {
            _indent_unit = count; // set indent unit to the first indent we see
        } else if (count % _indent_unit != 0) {
            throw yaml_exception("inconsistent indentation");
        }
        return count / _indent_unit;
    }
    return 0;
}

void parser::pushIndent(int indent, bool is_mapping)
{
    indent_frame frame;
    frame.indent_level = indent;
    frame.is_mapping = is_mapping;
    // container starts as null - dispatchers create the actual container
    _stack.push_back(std::move(frame));
}

void parser::popIndent()
{
    if (_stack.empty()) {
        throw yaml_exception("indent stack underflow");
    }
    auto frame = std::move(_stack.back());
    _stack.pop_back();

    if (!_stack.empty()) {
        auto& parent = _stack.back();
        // Compact syntax sub-frame: write back to parent's last array element
        if (frame.parent_key.empty()) {
            if (!parent.is_mapping && !parent.container.is_null() && !parent.container.as_array().empty()) {
                parent.container.as_array().back() = std::move(frame.container);
            }
        }
        // Normal case: write this frame's container to parent's pending key
        else if (parent.is_mapping) {
            parent.container.as_object()[frame.parent_key] = std::move(frame.container);
        }
    }
}

value &parser::currentContainer()
{
    return _stack.back().container;
}

void parser::addValue(value &&v)
{
    auto& frame = _stack.back();
    if (frame.container.is_null()) {
        // First value at this level - store directly
        frame.container = std::move(v);
    } else if (frame.is_mapping) {
        frame.container.as_object()[frame.pending_key] = std::move(v);
    } else {
        frame.container.as_array().push_back(std::move(v));
    }
}

std::string parser::stripComments(const std::string &line) const
{
    // remove comments from the line as well as leading and trailing whitespace
    // should not even be called when handling multi-line scalars.
    // handles # in strings correctly.

    std::string result;
    bool in_string = false;
    char quote_char = '\0';

    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if ((c == '"' || c == '\'') && !in_string) {
            // Enter string: any quote when not already in a string
            in_string = true;
            quote_char = c;
            result += c;
        } else if (in_string && c == quote_char) {
            // Exit string: matching close quote
            in_string = false;
            quote_char = '\0';
            result += c;
        } else if (c == '#' && !in_string) {
            // go back and trim trailing whitespace from result
            size_t end = result.size();
            while (end > 0 && isspace(result[end - 1])) {
                end--;
            }
            result = result.substr(0, end);
            break; // ignore the rest of the line
        } else {
            result += c;
        }
    }
    return result;
}

} // namespace scl2::yaml