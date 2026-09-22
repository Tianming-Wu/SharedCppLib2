#include "toml.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>

// 内部实现定义在本文件末尾，这里先给出声明供 toml::fromString / toString 调用
namespace scl2::parser_impl {
    toml parse(const std::string& str);
}
namespace scl2::serializer_impl {
    std::string serialize(const toml& doc);
}

namespace scl2 {

// =============================== toml_value ===============================

toml_value::toml_value() = default;
toml_value::toml_value(const std::string& s) : value(s) {}
toml_value::toml_value(std::string&& s) : value(std::move(s)) {}
toml_value::toml_value(int64_t i) : value(i) {}
toml_value::toml_value(double d) : value(d) {}
toml_value::toml_value(bool b) : value(b) {}
toml_value::toml_value(const toml_datetime& dt) : value(dt) {}
toml_value::toml_value(toml_datetime&& dt) : value(std::move(dt)) {}
toml_value::toml_value(const std::vector<toml_value>& arr) : value(arr) {}
toml_value::toml_value(std::vector<toml_value>&& arr) : value(std::move(arr)) {}
toml_value::toml_value(const std::map<std::string, toml_value>& obj) : value(obj) {}
toml_value::toml_value(std::map<std::string, toml_value>&& obj) : value(std::move(obj)) {}

bool toml_value::is_bool() const { return std::holds_alternative<bool>(value); }
bool toml_value::is_int() const { return std::holds_alternative<int64_t>(value); }
bool toml_value::is_double() const { return std::holds_alternative<double>(value); }
bool toml_value::is_string() const { return std::holds_alternative<std::string>(value); }
bool toml_value::is_datetime() const { return std::holds_alternative<toml_datetime>(value); }
bool toml_value::is_array() const { return std::holds_alternative<std::vector<toml_value>>(value); }
bool toml_value::is_table() const { return std::holds_alternative<std::map<std::string, toml_value>>(value); }

toml_value_type toml_value::type() const
{
    if (is_bool()) return toml_value_type::boolean;
    if (is_int()) return toml_value_type::integer;
    if (is_double()) return toml_value_type::floating;
    if (is_datetime()) return toml_value_type::datetime;
    if (is_string()) return toml_value_type::string;
    if (is_array()) return toml_value_type::array;
    return toml_value_type::table;
}

bool toml_value::as_bool() const { return std::get<bool>(value); }
int64_t toml_value::as_int() const { return std::get<int64_t>(value); }
double toml_value::as_double() const { return std::get<double>(value); }
const std::string& toml_value::as_string() const { return std::get<std::string>(value); }
const std::string& toml_value::as_datetime() const { return std::get<toml_datetime>(value).text; }
const std::vector<toml_value>& toml_value::as_array() const { return std::get<std::vector<toml_value>>(value); }
const std::map<std::string, toml_value>& toml_value::as_table() const { return std::get<std::map<std::string, toml_value>>(value); }
bool& toml_value::as_bool() { return std::get<bool>(value); }
int64_t& toml_value::as_int() { return std::get<int64_t>(value); }
double& toml_value::as_double() { return std::get<double>(value); }
std::string& toml_value::as_string() { return std::get<std::string>(value); }
std::vector<toml_value>& toml_value::as_array() { return std::get<std::vector<toml_value>>(value); }
std::map<std::string, toml_value>& toml_value::as_table() { return std::get<std::map<std::string, toml_value>>(value); }

// array
toml_value& toml_value::operator[](size_t index) { return as_array()[index]; }
const toml_value& toml_value::operator[](size_t index) const { return as_array()[index]; }
size_t toml_value::array_size() const { return is_array() ? as_array().size() : 0; }
void toml_value::push_back(const toml_value& v) { as_array().push_back(v); }
bool toml_value::empty_as_array() const { return is_array() && as_array().empty(); }

// table
bool toml_value::has_key(const std::string& key) const { return is_table() && as_table().count(key) > 0; }
toml_value& toml_value::operator[](const std::string& key) { return as_table()[key]; }
const toml_value& toml_value::operator[](const std::string& key) const
{
    const auto& t = as_table();
    const auto it = t.find(key);
    if (it == t.end()) throw std::out_of_range("toml: key not found: " + key);
    return it->second;
}
const toml_value& toml_value::at(const std::string& key) const { return (*this)[key]; }
size_t toml_value::table_size() const { return is_table() ? as_table().size() : 0; }

size_t toml_value::size() const
{
    if (is_table()) return as_table().size();
    if (is_array()) return as_array().size();
    if (is_string() || is_datetime()) return as_string().size();
    return 0;
}
bool toml_value::empty() const
{
    if (is_table()) return as_table().empty();
    if (is_array()) return as_array().empty();
    if (is_string()) return as_string().empty();
    return false;
}

bool toml_value::operator==(const toml_value& other) const { return value == other.value; }

// =================================== toml ==================================

toml toml::fromString(const std::string& str) { return parser_impl::parse(str); }

toml toml::fromFile(const std::filesystem::path& path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) throw std::runtime_error("toml: cannot open file: " + path.string());
    std::string data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return fromString(data);
}

std::string toml::toString() const { return serializer_impl::serialize(*this); }

std::string toml::toFile(const std::filesystem::path& path) const
{
    std::string out = toString();
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) throw std::runtime_error("toml: cannot write file: " + path.string());
    ofs << out;
    return out;
}

bool toml::has_key(const std::string& key) const { return m_table.count(key) > 0; }
toml_value& toml::operator[](const std::string& key) { return m_table[key]; }
const toml_value& toml::operator[](const std::string& key) const
{
    const auto it = m_table.find(key);
    if (it == m_table.end()) throw std::out_of_range("toml: key not found: " + key);
    return it->second;
}
size_t toml::size() const { return m_table.size(); }
bool toml::empty() const { return m_table.empty(); }
toml_table& toml::table() { return m_table; }
const toml_table& toml::table() const { return m_table; }

} // namespace scl2

// ============================== parser impl ===============================

namespace scl2::parser_impl {

class parser {
public:
    explicit parser(std::string input) : src(std::move(input)) {}

    toml parse()
    {
        toml root;
        std::vector<std::string> cur;
        skipBlank();
        while (pos < src.size()) {
            if (src[pos] == '[') {
                const bool is_arr = (pos + 1 < src.size() && src[pos + 1] == '[');
                pos += is_arr ? 2 : 1;
                const auto path = parseKeyPath();
                expect(']');
                if (is_arr) expect(']');
                skipToEol();
                expectEol();
                cur = is_arr ? startArrayOfTables(root, path) : startTable(root, path);
            } else {
                auto keys = parseKeyPath();
                skipWs();
                expect('=');
                skipWs();
                toml_value v = parseValue();
                skipToEol();
                expectEol();
                assignKey(root, cur, std::move(keys), std::move(v));
            }
            skipBlank();
        }
        return root;
    }

private:
    std::string src;
    size_t pos = 0;

    bool atEnd() const { return pos >= src.size(); }
    std::runtime_error error(const std::string& msg) const
    {
        size_t line = 1;
        for (size_t i = 0; i < pos && i < src.size(); ++i)
            if (src[i] == '\n') ++line;
        return std::runtime_error("toml: parse error at line " + std::to_string(line) + ": " + msg);
    }
    void expect(char c)
    {
        if (atEnd() || src[pos] != c) throw error(std::string("expected '") + c + "'");
        ++pos;
    }

    void skipWs() { while (!atEnd() && (src[pos] == ' ' || src[pos] == '\t')) ++pos; }
    void skipComment() { while (!atEnd() && src[pos] != '\n' && src[pos] != '\r') ++pos; }
    void skipToEol()
    {
        skipWs();
        if (!atEnd() && src[pos] == '#') skipComment();
    }
    void expectEol()
    {
        if (!atEnd() && src[pos] != '\n' && src[pos] != '\r')
            throw error("expected end of line");
    }
    void skipBlank()
    {
        while (!atEnd()) {
            if (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r') { ++pos; continue; }
            if (src[pos] == '#') { skipComment(); continue; }
            break;
        }
    }
    void skipWsNl()
    {
        while (!atEnd()) {
            const char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++pos; continue; }
            if (c == '#') { skipComment(); continue; }
            break;
        }
    }

    // ---- keys ----
    std::string parseKey()
    {
        skipWs();
        if (atEnd()) throw error("expected key");
        if (src[pos] == '"' || src[pos] == '\'') {
            const auto v = parseString();
            return v.as_string();
        }
        const size_t start = pos;
        while (!atEnd()) {
            const char c = src[pos];
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') ++pos;
            else break;
        }
        if (pos == start) throw error("expected key");
        return src.substr(start, pos - start);
    }

    std::vector<std::string> parseKeyPath()
    {
        std::vector<std::string> keys;
        skipWs();
        keys.push_back(parseKey());
        skipWs();
        while (!atEnd() && src[pos] == '.') {
            ++pos;
            keys.push_back(parseKey());
            skipWs();
        }
        return keys;
    }

    // ---- navigation ----
    // Walk a path of table segments, auto-creating missing tables; when a
    // segment is an array-of-tables, descend into its last element.
    static toml_table& descend(toml_table& root, const std::vector<std::string>& path)
    {
        toml_table* cur = &root;
        for (const auto& seg : path) {
            toml_value& nxt = (*cur)[seg];
            if (nxt.is_array() && !nxt.as_array().empty() && nxt.as_array().back().is_table())
                cur = &nxt.as_array().back().as_table();
            else {
                if (!nxt.is_table()) nxt = toml_table{};
                cur = &nxt.as_table();
            }
        }
        return *cur;
    }

    std::vector<std::string> startTable(toml& root, const std::vector<std::string>& path)
    {
        descend(root.table(), path);
        return path;
    }

    std::vector<std::string> startArrayOfTables(toml& root, const std::vector<std::string>& path)
    {
        if (path.empty()) throw error("empty array-of-tables header");
        toml_table& parent = descend(root.table(),
                                     std::vector<std::string>(path.begin(), path.end() - 1));
        const std::string& name = path.back();
        toml_value& entry = parent[name];
        if (!entry.is_array()) entry = toml_array{};
        entry.push_back(toml_value(toml_table{}));
        return path;
    }

    void assignKey(toml& root, const std::vector<std::string>& cur,
                   std::vector<std::string> keys, toml_value&& v)
    {
        if (keys.empty()) throw error("empty key");
        toml_table& t = descend(root.table(), cur);
        if (keys.size() > 1) {
            toml_table& sub = descend(t, std::vector<std::string>(keys.begin(), keys.end() - 1));
            sub[keys.back()] = std::move(v);
        } else {
            t[keys[0]] = std::move(v);
        }
    }

    // ---- values ----
    toml_value parseValue()
    {
        skipWs();
        if (atEnd()) throw error("expected value");
        const char c = src[pos];
        if (c == '"' || c == '\'') return parseString();
        if (c == '[') return parseArray();
        if (c == '{') return parseInlineTable();
        if (c == 't' || c == 'f') return parseBool();
        if (c == '-' || c == '+' || std::isdigit(static_cast<unsigned char>(c)))
            return parseNumberOrDate();
        throw error("unexpected value");
    }

    toml_value parseBool()
    {
        if (src.compare(pos, 4, "true") == 0) { pos += 4; return toml_value(true); }
        if (src.compare(pos, 5, "false") == 0) { pos += 5; return toml_value(false); }
        throw error("invalid boolean");
    }

    toml_value parseNumberOrDate()
    {
        // a date/time starts with a 4-digit year followed by '-'
        const bool looks_date = pos + 4 <= src.size()
            && std::isdigit(static_cast<unsigned char>(src[pos]))
            && std::isdigit(static_cast<unsigned char>(src[pos + 1]))
            && std::isdigit(static_cast<unsigned char>(src[pos + 2]))
            && std::isdigit(static_cast<unsigned char>(src[pos + 3]))
            && pos + 4 < src.size() && src[pos + 4] == '-';
        if (looks_date) {
            const size_t start = pos;
            while (!atEnd()) {
                const char c = src[pos];
                if (c == '\n' || c == '\r' || c == '#' || c == ',' || c == ']' || c == '}')
                    break;
                ++pos;
            }
            return toml_value(toml_datetime{src.substr(start, pos - start)});
        }
        return parseNumber();
    }

    toml_value parseNumber()
    {
        const size_t start = pos;
        if (!atEnd() && (src[pos] == '-' || src[pos] == '+')) ++pos;
        if (src.compare(pos, 3, "inf") == 0) {
            pos += 3;
            const double d = std::numeric_limits<double>::infinity();
            return toml_value(src[start] == '-' ? -d : d);
        }
        if (src.compare(pos, 3, "nan") == 0) {
            pos += 3;
            return toml_value(std::numeric_limits<double>::quiet_NaN());
        }
        if (pos + 1 < src.size() && src[pos] == '0'
            && (src[pos + 1] == 'x' || src[pos + 1] == 'o' || src[pos + 1] == 'b')) {
            const int base = src[pos + 1] == 'x' ? 16 : (src[pos + 1] == 'o' ? 8 : 2);
            pos += 2;
            const size_t dstart = pos;
            while (!atEnd() && (std::isalnum(static_cast<unsigned char>(src[pos])) || src[pos] == '_'))
                ++pos;
            std::string tok = src.substr(dstart, pos - dstart);
            tok.erase(std::remove(tok.begin(), tok.end(), '_'), tok.end());
            long long val = 0;
            const auto [p, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), val, base);
            if (ec != std::errc()) throw error("invalid integer");
            return toml_value(static_cast<int64_t>(src[start] == '-' ? -val : val));
        }
        bool has_dot = false, has_exp = false;
        while (!atEnd()) {
            const char c = src[pos];
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '_') { ++pos; continue; }
            if (c == '.' && !has_dot && !has_exp) { has_dot = true; ++pos; continue; }
            if ((c == 'e' || c == 'E') && !has_exp) {
                has_exp = true;
                ++pos;
                if (!atEnd() && (src[pos] == '+' || src[pos] == '-')) ++pos;
                continue;
            }
            break;
        }
        std::string tok = src.substr(start, pos - start);
        tok.erase(std::remove(tok.begin(), tok.end(), '_'), tok.end());
        if (has_dot || has_exp) {
            double d = 0;
            const auto [p, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), d);
            if (ec != std::errc()) throw error("invalid float");
            return toml_value(d);
        }
        long long i = 0;
        const auto [p, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), i);
        if (ec != std::errc()) throw error("invalid integer");
        return toml_value(static_cast<int64_t>(i));
    }

    static int hexVal(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
    static std::string utf8FromCp(uint32_t cp)
    {
        std::string out;
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        return out;
    }

    std::string decodeEscapes(const std::string& in)
    {
        std::string out;
        out.reserve(in.size());
        for (size_t i = 0; i < in.size(); ++i) {
            if (in[i] != '\\' || i + 1 >= in.size()) { out += in[i]; continue; }
            const char e = in[++i];
            switch (e) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case '"': out += '"'; break;
                case '\'': out += '\''; break;
                case '\\': out += '\\'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case '\n':
                    while (i + 1 < in.size()
                           && (in[i + 1] == ' ' || in[i + 1] == '\t' || in[i + 1] == '\n' || in[i + 1] == '\r'))
                        ++i;
                    break;
                case 'u': {
                    if (i + 4 >= in.size()) throw error("bad \\u escape");
                    uint32_t cp = 0;
                    for (int j = 1; j <= 4; ++j) {
                        const int h = hexVal(in[i + j]);
                        if (h < 0) throw error("bad \\u escape");
                        cp = cp * 16 + static_cast<uint32_t>(h);
                    }
                    i += 4;
                    out += utf8FromCp(cp);
                    break;
                }
                case 'U': {
                    if (i + 8 >= in.size()) throw error("bad \\U escape");
                    uint32_t cp = 0;
                    for (int j = 1; j <= 8; ++j) {
                        const int h = hexVal(in[i + j]);
                        if (h < 0) throw error("bad \\U escape");
                        cp = cp * 16 + static_cast<uint32_t>(h);
                    }
                    i += 8;
                    out += utf8FromCp(cp);
                    break;
                }
                default: out += e;
            }
        }
        return out;
    }

    toml_value parseString()
    {
        const char q = src[pos];
        const bool multiline = pos + 2 < src.size() && src[pos + 1] == q && src[pos + 2] == q;
        if (multiline) {
            pos += 3;
            if (!atEnd() && src[pos] == '\r') ++pos;
            if (!atEnd() && src[pos] == '\n') ++pos;
            std::string raw;
            while (!atEnd()) {
                if (src.compare(pos, 3, std::string(3, q)) == 0) {
                    size_t extra = 0;
                    while (pos + 3 + extra < src.size() && src[pos + 3 + extra] == q) ++extra;
                    pos += 3 + extra;
                    raw.append(extra, q);
                    return toml_value(q == '"' ? decodeEscapes(raw) : raw);
                }
                raw += src[pos++];
            }
            throw error("unterminated multiline string");
        }
        ++pos;
        std::string raw;
        while (!atEnd() && src[pos] != q) {
            if (src[pos] == '\n' || src[pos] == '\r') throw error("unterminated string");
            if (src[pos] == '\\' && pos + 1 < src.size()) {
                raw += src[pos++];  // keep the backslash + escaped char for decodeEscapes
                raw += src[pos++];
                continue;
            }
            raw += src[pos++];
        }
        if (atEnd()) throw error("unterminated string");
        ++pos;
        return toml_value(q == '"' ? decodeEscapes(raw) : raw);
    }

    toml_value parseArray()
    {
        expect('[');
        toml_array arr;
        skipWsNl();
        if (!atEnd() && src[pos] == ']') { ++pos; return toml_value(std::move(arr)); }
        while (true) {
            arr.push_back(parseValue());
            skipWsNl();
            if (!atEnd() && src[pos] == ',') { ++pos; skipWsNl(); continue; }
            if (!atEnd() && src[pos] == ']') { ++pos; break; }
            throw error("expected ',' or ']' in array");
        }
        return toml_value(std::move(arr));
    }

    toml_value parseInlineTable()
    {
        // Note: TOML spec requires inline tables to be single-line, but many
        // real-world files (e.g. neoforge.mods.toml) span multiple lines, so we
        // accept newlines/comments here (lenient).
        expect('{');
        toml_table t;
        skipWsNl();
        if (!atEnd() && src[pos] == '}') { ++pos; return toml_value(std::move(t)); }
        while (true) {
            auto keys = parseKeyPath();
            skipWsNl();
            expect('=');
            skipWsNl();
            toml_value v = parseValue();
            if (keys.size() == 1) {
                t[keys[0]] = std::move(v);
            } else {
                toml_table& sub = descend(t, std::vector<std::string>(keys.begin(), keys.end() - 1));
                sub[keys.back()] = std::move(v);
            }
            skipWsNl();
            if (!atEnd() && src[pos] == ',') { ++pos; skipWsNl(); continue; }
            if (!atEnd() && src[pos] == '}') { ++pos; break; }
            throw error("expected ',' or '}' in inline table");
        }
        return toml_value(std::move(t));
    }
};

toml parse(const std::string& str)
{
    parser p(str);
    return p.parse();
}

} // namespace scl2::parser_impl

// ============================ serializer impl =============================

namespace scl2::serializer_impl {

std::string escapeString(const std::string& s)
{
    std::string out = "\"";
    for (const char c : s) {
        switch (c) {
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            default: out += c;
        }
    }
    out += "\"";
    return out;
}

std::string scalar(const scl2::toml_value& v)
{
    switch (v.type()) {
        case scl2::toml_value_type::boolean: return v.as_bool() ? "true" : "false";
        case scl2::toml_value_type::integer: return std::to_string(v.as_int());
        case scl2::toml_value_type::floating: {
            const double d = v.as_double();
            if (std::isinf(d)) return d < 0 ? "-inf" : "inf";
            if (std::isnan(d)) return "nan";
            std::ostringstream oss;
            oss << std::setprecision(17) << d;
            std::string s = oss.str();
            if (s.find('.') == std::string::npos
                && s.find('e') == std::string::npos && s.find('E') == std::string::npos)
                s += ".0";
            return s;
        }
        case scl2::toml_value_type::string: return escapeString(v.as_string());
        case scl2::toml_value_type::datetime: return v.as_datetime();
        case scl2::toml_value_type::array: {
            std::string out = "[";
            const auto& a = v.as_array();
            for (size_t i = 0; i < a.size(); ++i) {
                if (i) out += ", ";
                out += scalar(a[i]);
            }
            return out + "]";
        }
        default: return "null";
    }
}

bool arrayOfTables(const scl2::toml_value& v)
{
    if (!v.is_array()) return false;
    for (const auto& e : v.as_array())
        if (e.is_table()) return true;
    return false;
}

std::string joinPath(const std::vector<std::string>& p)
{
    std::string s;
    for (size_t i = 0; i < p.size(); ++i) {
        if (i) s += '.';
        s += p[i];
    }
    return s;
}

void table(const std::vector<std::string>& path, const scl2::toml_table& t, std::string& out)
{
    bool wrote = false;
    for (const auto& [k, v] : t) {
        if (v.is_table() || arrayOfTables(v)) continue;
        out += k + " = " + scalar(v) + "\n";
        wrote = true;
    }
    if (wrote) out += "\n";

    for (const auto& [k, v] : t) {
        if (!v.is_table()) continue;
        auto np = path;
        np.push_back(k);
        out += "[" + joinPath(np) + "]\n";
        table(np, v.as_table(), out);
    }
    for (const auto& [k, v] : t) {
        if (!arrayOfTables(v)) continue;
        auto np = path;
        np.push_back(k);
        for (const auto& e : v.as_array()) {
            out += "[[" + joinPath(np) + "]]\n";
            table(np, e.as_table(), out);
        }
    }
}

std::string serialize(const scl2::toml& doc)
{
    std::string out;
    table({}, doc.table(), out);
    return out;
}

} // namespace scl2::serializer_impl
