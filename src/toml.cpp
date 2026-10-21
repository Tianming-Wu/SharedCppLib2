#include "toml.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

// The parser and the serializer live at the end of this file; the declarations are up here so that
// toml::fromString() and toml::toString() can call them.
namespace scl2::parser_impl {
    toml parse(const std::string& str, fidelity f);
}
namespace scl2::serializer_impl {
    std::string serialize(const toml& doc);
}

namespace scl2 {

namespace {

// The shape a table used to have (std::map), so that code building one with it keeps working.
toml_table to_ordered(const std::map<std::string, toml_value>& m)
{
    toml_table t;
    for (const auto& [k, v] : m) t.insert(k, v);
    return t;
}

// The comma of an array element (or of an inline table member) sits in the lead of the one that
// follows it, so removing the first element has to take it with it.
void strip_leading_separator(toml_value& v)
{
    if (!v.has_source()) return;
    std::string lead = v.lead();
    size_t comma = std::string::npos;
    for (size_t i = 0; i < lead.size(); ++i) {
        if (lead[i] == '#') {   // a comma inside a comment is not a separator
            while (i < lead.size() && lead[i] != '\n' && lead[i] != '\r') ++i;
            continue;
        }
        if (lead[i] == ',') { comma = i; break; }
    }
    if (comma == std::string::npos) return;
    lead.erase(comma, 1);
    v.set_lead(std::move(lead));
}

// The separator an appended element should use, judged by the one before it.
std::string separator_style(const std::string& previous_lead)
{
    const size_t nl = previous_lead.find_last_of("\r\n");
    if (nl == std::string::npos) return ", ";
    std::string ind;
    for (size_t i = nl + 1; i < previous_lead.size(); ++i) {
        if (previous_lead[i] == ' ' || previous_lead[i] == '\t') ind += previous_lead[i];
        else break;
    }
    return ",\n" + ind;
}

} // namespace

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
toml_value::toml_value(const toml_table& obj) : value(obj) {}
toml_value::toml_value(toml_table&& obj) : value(std::move(obj)) {}
toml_value::toml_value(const std::map<std::string, toml_value>& obj) : value(to_ordered(obj)) {}
toml_value::toml_value(std::map<std::string, toml_value>&& obj) : value(to_ordered(obj)) {}

toml_value& toml_value::operator=(const toml_value& other)
{
    if (this == &other) return *this;
    value = other.value;
    // The member's own text — its lead, its key, its separator and its trailing comment — stays.
    // Only the value is written again.
    raw_.clear();
    header_.clear();
    tail_.clear();
    dirty_ = true;
    style_ = other.style_ == toml_table_style::inline_table ? toml_table_style::inline_table
                                                            : toml_table_style::generated;
    return *this;
}

toml_value& toml_value::operator=(toml_value&& other) noexcept
{
    if (this == &other) return *this;
    value = std::move(other.value);
    raw_.clear();
    header_.clear();
    tail_.clear();
    dirty_ = true;
    style_ = other.style_ == toml_table_style::inline_table ? toml_table_style::inline_table
                                                            : toml_table_style::generated;
    return *this;
}

bool toml_value::is_bool() const { return std::holds_alternative<bool>(value); }
bool toml_value::is_int() const { return std::holds_alternative<int64_t>(value); }
bool toml_value::is_double() const { return std::holds_alternative<double>(value); }
bool toml_value::is_string() const { return std::holds_alternative<std::string>(value); }
bool toml_value::is_datetime() const { return std::holds_alternative<toml_datetime>(value); }
bool toml_value::is_array() const { return std::holds_alternative<std::vector<toml_value>>(value); }
bool toml_value::is_table() const { return std::holds_alternative<toml_table>(value); }

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
const toml_table& toml_value::as_table() const { return std::get<toml_table>(value); }
bool& toml_value::as_bool() { return std::get<bool>(value); }
int64_t& toml_value::as_int() { return std::get<int64_t>(value); }
double& toml_value::as_double() { return std::get<double>(value); }
std::string& toml_value::as_string() { return std::get<std::string>(value); }
std::vector<toml_value>& toml_value::as_array() { return std::get<std::vector<toml_value>>(value); }
toml_table& toml_value::as_table() { return std::get<toml_table>(value); }

// array
toml_value& toml_value::operator[](size_t index) { return as_array()[index]; }
const toml_value& toml_value::operator[](size_t index) const { return as_array()[index]; }
size_t toml_value::array_size() const { return is_array() ? as_array().size() : 0; }
bool toml_value::empty_as_array() const { return is_array() && as_array().empty(); }

void toml_value::push_back(const toml_value& v)
{
    auto& a = as_array();
    toml_value element = v;
    if (!element.has_source()) {
        // The comma belongs to the element that follows one: keep the style of the array.
        if (!a.empty()) element.set_lead(separator_style(a.back().lead()));
    }
    a.push_back(std::move(element));
}

bool toml_value::erase(size_t index)
{
    if (!is_array()) return false;
    auto& a = as_array();
    if (index >= a.size()) return false;
    a.erase(a.begin() + static_cast<std::ptrdiff_t>(index));
    if (index == 0 && !a.empty()) strip_leading_separator(a.front());
    return true;
}

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

bool toml_value::erase(const std::string& key)
{
    if (!is_table()) return false;
    return as_table().erase(key);
}

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

void toml_value::set_raw(std::string text)
{
    raw_ = std::move(text);
    dirty_ = false;
}

void toml_value::set_lead(std::string text) { lead_ = std::move(text); }

void toml_value::set_trail(std::string text) { trail_ = std::move(text); }

void toml_value::set_comment(std::string text)
{
    // Keep the line ending that is already there, and add the one a line has when there is none.
    std::string ending;
    const size_t pos = trail_.find_first_of("\r\n");
    if (pos != std::string::npos) ending = trail_.substr(pos);
    else ending = "\n";

    trail_.clear();
    if (!text.empty()) {
        trail_ = " # ";
        trail_ += text;
    }
    trail_ += ending;
}

void toml_value::set_inline_table(bool on)
{
    if (on) {
        style_ = toml_table_style::inline_table;
        header_.clear();
    } else {
        style_ = toml_table_style::generated;
    }
}

bool toml_value::operator==(const toml_value& other) const
{
    // Values compare by what they hold: the source text is not part of it, the way a value was
    // spelled is not, and a table is a map, so its member order does not decide the answer.
    if (value.index() != other.value.index()) return false;

    if (is_array()) {
        const auto& a = as_array();
        const auto& b = other.as_array();
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (!(a[i] == b[i])) return false;
        return true;
    }
    if (is_table()) {
        const auto& a = as_table();
        const auto& b = other.as_table();
        if (a.size() != b.size()) return false;
        for (const auto& [k, v] : a) {
            const auto it = b.find(k);
            if (it == b.end() || !(it->second == v)) return false;
        }
        return true;
    }

    if (is_bool()) return as_bool() == other.as_bool();
    if (is_int()) return as_int() == other.as_int();
    if (is_double()) return as_double() == other.as_double();
    if (is_datetime()) return as_datetime() == other.as_datetime();
    return as_string() == other.as_string();
}

// =================================== toml ==================================

toml toml::fromString(const std::string& str, fidelity f) { return parser_impl::parse(str, f); }

toml toml::fromFile(const std::filesystem::path& path, fidelity f)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) throw std::runtime_error("toml: cannot open file: " + path.string());
    std::string data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return fromString(data, f);
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
bool toml::erase(const std::string& key) { return m_table.erase(key); }
toml_table& toml::table() { return m_table; }
const toml_table& toml::table() const { return m_table; }

} // namespace scl2

// ============================== parser impl ===============================

namespace scl2::parser_impl {

class parser {
public:
    parser(std::string input, fidelity f) : src(std::move(input)), mode(f) {}

    toml parse()
    {
        toml doc;
        doc.m_fidelity = mode;

        // A byte order mark is not text: it belongs to the layout in front of the first member.
        if (src.compare(0, 3, "\xEF\xBB\xBF") == 0) {
            if (raw()) pending += src.substr(0, 3);
            pos = 3;
        }

        cur_path.clear();
        while (true) {
            collectBlank();
            if (atEnd()) {
                doc.m_epilog = std::move(pending);
                pending.clear();
                break;
            }
            if (src[pos] == '[') parseHeader(doc);
            else parseKeyValue(doc);
        }
        return doc;
    }

private:
    std::string src;
    fidelity mode;
    size_t pos = 0;
    std::vector<std::string> cur_path;  // the table the next key/value line belongs to
    std::string pending;                // blank lines, comments and indentation before the next entry

    bool raw() const { return mode == fidelity::raw; }
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

    // Consume whitespace, line endings and comments; the text comes back as it stands, so that it
    // can be kept.
    std::string takeGap()
    {
        const size_t start = pos;
        while (!atEnd()) {
            const char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++pos; continue; }
            if (c == '#') {
                while (!atEnd() && src[pos] != '\n' && src[pos] != '\r') ++pos;
                continue;
            }
            break;
        }
        return src.substr(start, pos - start);
    }

    // The rest of the current line, line ending included.
    std::string takeLineEnding()
    {
        const size_t start = pos;
        while (!atEnd() && src[pos] != '\n' && src[pos] != '\r') ++pos;
        if (!atEnd() && src[pos] == '\r') ++pos;
        if (!atEnd() && src[pos] == '\n') ++pos;
        return src.substr(start, pos - start);
    }

    // Everything in front of the next entry: blank lines, comment lines, indentation.
    void collectBlank()
    {
        const size_t start = pos;
        while (!atEnd()) {
            const char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++pos; continue; }
            if (c == '#') {
                while (!atEnd() && src[pos] != '\n' && src[pos] != '\r') ++pos;
                continue;
            }
            break;
        }
        if (raw()) pending += src.substr(start, pos - start);
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
    // Walk (and create) a path of table segments. A segment that is an array-of-tables goes into
    // its last element; a segment that is missing becomes a table that exists only as the prefix of
    // this key path, which is what keeps `a.b = 1` writing back as a single line.
    toml_table& descend(toml_table& root, std::vector<std::string>::const_iterator begin,
                        std::vector<std::string>::const_iterator end)
    {
        toml_table* t = &root;
        for (auto it = begin; it != end; ++it) {
            toml_value& slot = (*t)[*it];
            if (slot.is_array() && !slot.as_array().empty() && slot.as_array().back().is_table()) {
                t = &slot.as_array().back().as_table();
            } else if (slot.is_table()) {
                t = &slot.as_table();
            } else {
                slot.value = toml_table{};
                if (raw()) {
                    slot.sourced_ = true;
                    slot.style_ = toml_table_style::dotted;
                }
                t = &slot.as_table();
            }
        }
        return *t;
    }

    toml_table& current_table(toml& doc)
    {
        return descend(doc.table(), cur_path.begin(), cur_path.end());
    }

    // Everything of @p src but the text a member owns, which the caller sets.
    static void move_value(toml_value& dst, toml_value&& src)
    {
        dst.value = std::move(src.value);
        dst.raw_ = std::move(src.raw_);
        dst.header_ = std::move(src.header_);
        dst.lead_ = std::move(src.lead_);
        dst.key_text_ = std::move(src.key_text_);
        dst.separator_ = std::move(src.separator_);
        dst.trail_ = std::move(src.trail_);
        dst.tail_ = std::move(src.tail_);
        dst.style_ = src.style_;
        dst.sourced_ = src.sourced_;
        dst.dirty_ = false;
    }

    // ---- entries ----
    void parseHeader(toml& doc)
    {
        const size_t line_start = pos;
        const bool is_arr = (pos + 1 < src.size() && src[pos + 1] == '[');
        pos += is_arr ? 2 : 1;
        const std::vector<std::string> path = parseKeyPath();
        expect(']');
        if (is_arr) expect(']');
        size_t probe = pos;
        while (probe < src.size() && (src[probe] == ' ' || src[probe] == '\t')) ++probe;
        if (probe < src.size() && src[probe] != '#' && src[probe] != '\n' && src[probe] != '\r')
            throw error("expected end of line");
        takeLineEnding();
        const std::string header = src.substr(line_start, pos - line_start);
        if (path.empty()) throw error("empty table header");

        toml_table& parent = descend(doc.table(), path.begin(), path.end() - 1);
        const std::string& name = path.back();
        toml_value& entry = parent[name];

        if (is_arr) {
            if (!entry.is_array()) {
                entry.value = toml_array{};
                entry.style_ = toml_table_style::generated;
            }
            auto& arr = entry.as_array();
            toml_value element;
            element.value = toml_table{};
            if (raw()) {
                element.sourced_ = true;
                element.style_ = toml_table_style::header;
                element.header_ = header;
                if (arr.empty()) {
                    // What is in front of the first header belongs to the array (it is the member);
                    // in front of any later one it belongs to that element.
                    entry.sourced_ = true;
                    entry.lead_ = std::move(pending);
                } else {
                    element.lead_ = std::move(pending);
                }
            }
            pending.clear();
            arr.push_back(std::move(element));
            cur_path = path;
            return;
        }

        if (!entry.is_table()) {
            entry.value = toml_table{};
            entry.style_ = toml_table_style::header;
            if (raw()) {
                entry.sourced_ = true;
                entry.header_ = header;
                entry.lead_ = std::move(pending);
            }
        } else if (raw()) {
            entry.sourced_ = true;
            if (entry.style_ == toml_table_style::dotted) {
                // It existed only as the prefix of a dotted key; now it has a header of its own.
                entry.style_ = toml_table_style::header;
                entry.header_ = std::move(pending) + header;
            } else {
                // Reopened: keep the text of both headers, one after the other.
                entry.header_ += std::move(pending) + header;
            }
        }
        pending.clear();
        cur_path = path;
    }

    void parseKeyValue(toml& doc)
    {
        const size_t key_start = pos;
        const std::vector<std::string> keys = parseKeyPath();
        if (keys.empty()) throw error("empty key");
        const std::string key_text = src.substr(key_start, pos - key_start);

        const size_t sep_start = pos;
        skipWs();
        expect('=');
        skipWs();
        const std::string separator = src.substr(sep_start, pos - sep_start);

        toml_value value = parseValue();
        // The text up to the line ending — the comment included, and the spaces in front of it —
        // belongs to the member, so look ahead without consuming anything.
        size_t probe = pos;
        while (probe < src.size() && (src[probe] == ' ' || src[probe] == '\t')) ++probe;
        if (probe < src.size() && src[probe] != '#' && src[probe] != '\n' && src[probe] != '\r')
            throw error("expected end of line");
        const std::string trail = takeLineEnding();

        toml_table& table = current_table(doc);
        toml_table& target = descend(table, keys.begin(), keys.end() - 1);
        toml_value& slot = target[keys.back()];

        move_value(slot, std::move(value));
        if (raw()) {
            slot.sourced_ = true;
            slot.lead_ = std::move(pending);
            slot.key_text_ = key_text;
            slot.separator_ = separator;
            slot.trail_ = trail;
        }
        pending.clear();
    }

    // ---- values ----
    toml_value parseValue()
    {
        skipWs();
        if (atEnd()) throw error("expected value");
        const size_t start = pos;
        toml_value v = parseValueImpl();
        if (raw()) {
            v.sourced_ = true;
            // A container is not a token: it writes itself out of its members.
            if (!v.is_array() && !v.is_table()) v.raw_ = src.substr(start, pos - start);
        }
        return v;
    }

    toml_value parseValueImpl()
    {
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
                           && (in[i + 1] == ' ' || in[i + 1] == '\t'
                               || in[i + 1] == '\n' || in[i + 1] == '\r'))
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
            std::string decoded;
            while (!atEnd()) {
                if (src.compare(pos, 3, std::string(3, q)) == 0) {
                    size_t extra = 0;
                    while (pos + 3 + extra < src.size() && src[pos + 3 + extra] == q) ++extra;
                    pos += 3 + extra;
                    decoded.append(extra, q);
                    return toml_value(q == '"' ? decodeEscapes(decoded) : decoded);
                }
                decoded += src[pos++];
            }
            throw error("unterminated multiline string");
        }
        ++pos;
        std::string decoded;
        while (!atEnd() && src[pos] != q) {
            if (src[pos] == '\n' || src[pos] == '\r') throw error("unterminated string");
            if (src[pos] == '\\' && pos + 1 < src.size()) {
                decoded += src[pos++];  // keep the backslash + escaped char for decodeEscapes
                decoded += src[pos++];
                continue;
            }
            decoded += src[pos++];
        }
        if (atEnd()) throw error("unterminated string");
        ++pos;
        return toml_value(q == '"' ? decodeEscapes(decoded) : decoded);
    }

    toml_value parseArray()
    {
        expect('[');
        toml_value result(toml_array{});
        auto& arr = result.as_array();
        if (raw()) result.sourced_ = true;

        // The text after '[' is the first element's lead (what is in front of it, separator
        // included); when it runs straight into ']' the array is empty and the text is its tail.
        std::string lead = takeGap();
        if (!atEnd() && src[pos] == ']') {
            ++pos;
            if (raw()) result.tail_ = std::move(lead);
            return result;
        }

        while (true) {
            toml_value element = parseValue();
            toml_value& slot = arr.emplace_back(std::move(element));
            if (raw()) slot.lead_ = lead;

            const size_t gap_start = pos;
            const std::string gap = takeGap();
            if (!atEnd() && src[pos] == ',') {
                if (raw()) slot.trail_ = src.substr(gap_start, pos - gap_start);
                ++pos;
                lead = "," + takeGap();
                if (!atEnd() && src[pos] == ']') {   // a trailing comma
                    ++pos;
                    if (raw()) result.tail_ = std::move(lead);
                    break;
                }
                continue;
            }
            if (!atEnd() && src[pos] == ']') {
                if (raw()) slot.trail_ = src.substr(gap_start, pos - gap_start);
                ++pos;
                break;
            }
            throw error("expected ',' or ']' in array");
        }
        return result;
    }

    toml_value parseInlineTable()
    {
        // The TOML spec keeps inline tables on one line, but real-world files (e.g.
        // neoforge.mods.toml) span them over several lines, so newlines and comments are accepted
        // here as well.
        expect('{');
        toml_value result(toml_table{});
        result.style_ = toml_table_style::inline_table;
        if (raw()) result.sourced_ = true;

        std::string lead = takeGap();
        if (!atEnd() && src[pos] == '}') {
            ++pos;
            if (raw()) result.tail_ = std::move(lead);
            return result;
        }

        while (true) {
            const size_t key_start = pos;
            const std::vector<std::string> keys = parseKeyPath();
            if (keys.empty()) throw error("empty key");
            const std::string key_text = src.substr(key_start, pos - key_start);

            const size_t sep_start = pos;
            skipWs();
            expect('=');
            skipWs();
            const std::string separator = src.substr(sep_start, pos - sep_start);

            toml_value value = parseValue();

            toml_table& target = descend(result.as_table(), keys.begin(), keys.end() - 1);
            toml_value& slot = target[keys.back()];
            move_value(slot, std::move(value));
            if (raw()) {
                slot.sourced_ = true;
                slot.lead_ = std::move(lead);
                slot.key_text_ = key_text;
                slot.separator_ = separator;
            }

            const size_t gap_start = pos;
            const std::string gap = takeGap();
            if (!atEnd() && src[pos] == ',') {
                if (raw()) slot.trail_ = src.substr(gap_start, pos - gap_start);
                ++pos;
                lead = "," + takeGap();
                if (!atEnd() && src[pos] == '}') {   // a trailing comma
                    ++pos;
                    if (raw()) result.tail_ = std::move(lead);
                    break;
                }
                continue;
            }
            if (!atEnd() && src[pos] == '}') {
                if (raw()) slot.trail_ = src.substr(gap_start, pos - gap_start);
                ++pos;
                break;
            }
            throw error("expected ',' or '}' in inline table");
        }
        return result;
    }
};

toml parse(const std::string& str, fidelity f)
{
    parser p(str, f);
    return p.parse();
}

} // namespace scl2::parser_impl

// ============================ serializer impl =============================

namespace scl2::serializer_impl {

namespace {

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

// A key may be written bare only when it is made of the characters TOML allows there: `a.b` and
// `a b` have to be quoted, or they would mean something else.
bool bare_key_ok(const std::string& key)
{
    if (key.empty()) return false;
    for (const char c : key) {
        const unsigned char u = static_cast<unsigned char>(c);
        if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9')
            || c == '_' || c == '-')
            continue;
        return false;
    }
    return true;
}

std::string quote_key(const std::string& key) { return bare_key_ok(key) ? key : escapeString(key); }

// The shortest text that reads back as the same number, the way json writes them, so that a value
// written from code is `0.1` and not `0.10000000000000001`.
std::string format_double(double d)
{
    if (std::isinf(d)) return d < 0 ? "-inf" : "inf";
    if (std::isnan(d)) return "nan";

    char buf[48];
    for (int precision = 1; precision <= 17; ++precision) {
        std::snprintf(buf, sizeof(buf), "%.*g", precision, d);
        if (std::strtod(buf, nullptr) == d) break;
    }
    std::string s(buf);
    if (s.find_first_of(".eE") == std::string::npos) s += ".0";
    return s;
}

std::string scalar_or_container_text(const scl2::toml_value& v);
std::string nested_text(const scl2::toml_value& v);
std::string array_text(const scl2::toml_value& v);
std::string inline_table_text(const scl2::toml_value& v);
std::string member_text(const std::string& key, const scl2::toml_value& v);
bool is_aot_array(const scl2::toml_value& v);
bool is_block_member(const scl2::toml_value& v);
std::string join_path(const std::string& prefix, const std::string& key);
void write_section(std::string& out, const scl2::toml_table& t, const std::string& prefix);
void write_member_block(std::string& out, const std::string& key, const scl2::toml_value& v,
                        const std::string& prefix);
void start_block(std::string& out);

std::string scalar_or_container_text(const scl2::toml_value& v)
{
    // The spelling of the file wins when there is one and the value was not touched.
    const bool use_raw = !v.dirty() && !v.raw().empty();
    switch (v.type()) {
        case scl2::toml_value_type::boolean:
            return use_raw ? v.raw() : (v.as_bool() ? std::string("true") : std::string("false"));
        case scl2::toml_value_type::integer:
            return use_raw ? v.raw() : std::to_string(v.as_int());
        case scl2::toml_value_type::floating:
            return use_raw ? v.raw() : format_double(v.as_double());
        case scl2::toml_value_type::datetime:
            return use_raw ? v.raw() : v.as_datetime();
        case scl2::toml_value_type::string:
            return use_raw ? v.raw() : escapeString(v.as_string());
        case scl2::toml_value_type::array:
            return array_text(v);
        default:
            return std::string();   // a table: whether it goes on the line or on its own is decided above
    }
}

std::string nested_text(const scl2::toml_value& v)
{
    // Inside an array or an inline table a table has to stay on the line.
    return v.is_table() ? inline_table_text(v) : scalar_or_container_text(v);
}

std::string array_text(const scl2::toml_value& v)
{
    const auto& a = v.as_array();
    std::string out = "[";
    std::string previous_lead;
    for (size_t i = 0; i < a.size(); ++i) {
        std::string lead = a[i].lead();
        if (i > 0 && lead.find(',') == std::string::npos) lead = separator_style(previous_lead) + lead;
        out += lead;
        out += nested_text(a[i]);
        out += a[i].trail();
        previous_lead = a[i].lead();
    }
    out += v.tail();
    out += "]";
    return out;
}

struct inline_state {
    bool wrote = false;
    std::string previous_lead;
};

void write_inline_members(std::string& out, const scl2::toml_table& t, const std::string& prefix,
                          inline_state& st)
{
    for (const auto& [key, v] : t) {
        if (v.is_table() && v.table_style() == scl2::toml_table_style::dotted) {
            // It exists only as the prefix of a dotted key, so it has no member of its own.
            write_inline_members(out, v.as_table(), join_path(prefix, key), st);
            continue;
        }

        std::string lead = v.lead();
        if (st.wrote && lead.find(',') == std::string::npos)
            lead = separator_style(st.previous_lead) + lead;
        out += lead;

        out += v.has_source() ? v.key_text() : quote_key(prefix.empty() ? key : join_path(prefix, key));
        if (v.has_source()) out += v.separator();
        else out += v.separator().empty() ? " = " : v.separator();
        out += nested_text(v);
        out += v.trail();

        st.previous_lead = v.lead();
        st.wrote = true;
    }
}

std::string inline_table_text(const scl2::toml_value& v)
{
    std::string out = "{";
    inline_state st;
    write_inline_members(out, v.as_table(), std::string(), st);
    out += v.tail();
    out += "}";
    return out;
}

// One member of a table that is written on its own line. What the file said is used as it stands;
// a member that was built by hand gets the text the caller set, or the canonical shape.
std::string member_text(const std::string& key, const scl2::toml_value& v)
{
    const bool sourced = v.has_source();

    std::string out = v.lead();
    out += sourced ? v.key_text() : quote_key(key);

    if (sourced) out += v.separator();
    else out += v.separator().empty() ? " = " : v.separator();

    out += nested_text(v);

    if (sourced) out += v.trail();
    else out += v.trail().empty() ? "\n" : v.trail();

    return out;
}

// An array written with [[name]] headers. An array of inline tables is a value, not this.
bool is_aot_array(const scl2::toml_value& v)
{
    if (!v.is_array()) return false;
    const auto& a = v.as_array();
    if (a.empty()) return false;
    for (const auto& e : a) {
        if (!e.is_table() || e.is_inline_table()) return false;
    }
    return true;
}

// A member that becomes a [header] block: a table, or an array of tables.
bool is_block_member(const scl2::toml_value& v)
{
    if (is_aot_array(v)) return true;
    return v.is_table() && !v.is_inline_table();
}

std::string join_path(const std::string& prefix, const std::string& key)
{
    const std::string k = quote_key(key);
    return prefix.empty() ? k : prefix + "." + k;
}

// A new section starts on a fresh line, with a blank one in front when there is room for it.
void start_block(std::string& out) { if (!out.empty()) out += "\n"; }

void write_member_block(std::string& out, const std::string& key, const scl2::toml_value& v,
                        const std::string& prefix)
{
    const std::string path = join_path(prefix, key);

    if (is_aot_array(v)) {
        const auto& a = v.as_array();
        if (!v.lead().empty()) out += v.lead();
        else if (!v.has_source()) start_block(out);
        for (size_t i = 0; i < a.size(); ++i) {
            if (i > 0 || !v.has_source()) {
                if (!a[i].lead().empty()) out += a[i].lead();
                else if (!a[i].has_source()) start_block(out);
            }
            out += (a[i].has_source() && !a[i].header_text().empty()) ? a[i].header_text()
                                                                      : "[[" + path + "]]\n";
            write_section(out, a[i].as_table(), path);
        }
        return;
    }

    if (!v.lead().empty()) out += v.lead();
    else if (!v.has_source()) start_block(out);
    out += (v.has_source() && !v.header_text().empty()) ? v.header_text() : "[" + path + "]\n";
    write_section(out, v.as_table(), path);
}

// One pass over the members of a section. The key/value lines of a section have to come before its
// sub-tables — a line after a [header] belongs to that table — so the two are written in two
// passes rather than in one order. A table that exists only as the prefix of a dotted key is
// transparent here: its members are members of this section, at this very place.
void write_section_pass(std::string& out, const scl2::toml_table& t, const std::string& prefix,
                        int pass)
{
    for (const auto& [key, v] : t) {
        if (v.is_table() && v.table_style() == scl2::toml_table_style::dotted) {
            write_section_pass(out, v.as_table(), join_path(prefix, key), pass);
            continue;
        }
        const bool block = is_block_member(v);
        if ((pass == 0) == block) continue;
        if (pass == 0) out += member_text(key, v);
        else write_member_block(out, key, v, prefix);
    }
}

void write_section(std::string& out, const scl2::toml_table& t, const std::string& prefix)
{
    write_section_pass(out, t, prefix, 0);
    write_section_pass(out, t, prefix, 1);
}

} // namespace

std::string serialize(const scl2::toml& doc)
{
    std::string out;
    write_section(out, doc.table(), std::string());
    out += doc.epilog();
    return out;
}

} // namespace scl2::serializer_impl
