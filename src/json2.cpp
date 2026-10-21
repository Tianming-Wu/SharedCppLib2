#include "json2.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>

namespace scl2::json2 {

namespace {

// ── Small helpers ─────────────────────────────────────────────────────

constexpr bool is_ws(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

constexpr bool is_digit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

/// A number that has no JSON spelling. JSON has no infinity and no NaN, so writing one is a
/// mistake on the caller's side rather than something to encode.
std::string format_double(double d)
{
    if (!std::isfinite(d))
        throw std::runtime_error("json2: cannot write a number that is not finite");

    char buffer[48];
    // `%g` with the shortest precision that reads back as the same double: 0.1 stays "0.1", and a
    // value that needs 17 digits gets them.
    for (int precision = 1; precision <= 17; ++precision) {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, d);
        if (std::strtod(buffer, nullptr) != d) continue;

        std::string text(buffer);
        // Keep it a double when it is read back: "1" would come back as an integer, and a value
        // that changes type on a round trip is a value that changed.
        if (text.find_first_of(".eE") == std::string::npos) text += ".0";
        return text;
    }

    return std::string(buffer); // unreachable for a finite value
}

/// One code point as `\uXXXX`, or as a surrogate pair when it needs one.
void append_unicode_escape(std::string& out, std::uint32_t code)
{
    char buffer[13];
    if (code > 0xFFFF) {
        code -= 0x10000;
        std::snprintf(buffer, sizeof(buffer), "\\u%04x\\u%04x",
                      0xD800u | (code >> 10), 0xDC00u | (code & 0x3FF));
    } else {
        std::snprintf(buffer, sizeof(buffer), "\\u%04x", code);
    }
    out += buffer;
}

/// JSON string escaping, minimally: `"` and `\`, the short forms for the usual controls, and
/// `\u00XX` for the rest. Bytes above 0x7F are passed through, which keeps UTF-8 as it is —
/// unless `escape_non_ascii` is set, in which case they are decoded and written as `\uXXXX`,
/// which is what a consumer that cannot be trusted with UTF-8 wants. Invalid UTF-8 becomes
/// U+FFFD, the same as the conversion helpers elsewhere in the library.
void append_escaped(const std::string& text, std::string& out, bool escape_non_ascii = false)
{
    static const char* hex = "0123456789abcdef";

    out += '"';
    for (std::size_t i = 0; i < text.size(); ) {
        const unsigned char c = static_cast<unsigned char>(text[i]);

        if (c < 0x80) {
            switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out += hex[(c >> 4) & 0x0F];
                    out += hex[c & 0x0F];
                } else {
                    out += static_cast<char>(c);
                }
            }
            ++i;
            continue;
        }

        if (!escape_non_ascii) {
            out += static_cast<char>(c);
            ++i;
            continue;
        }

        // One UTF-8 sequence. A truncated or malformed one costs a single byte and turns into
        // U+FFFD, so nothing is passed through undecoded.
        std::uint32_t code = 0xFFFD;
        int extra = 0;
        if      ((c & 0xE0) == 0xC0) { code = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { code = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { code = c & 0x07; extra = 3; }

        std::size_t consumed = 1;
        bool valid = extra != 0;
        for (int k = 0; valid && k < extra; ++k) {
            if (i + 1 + static_cast<std::size_t>(k) >= text.size()) { valid = false; break; }
            const unsigned char next_byte =
                static_cast<unsigned char>(text[i + 1 + static_cast<std::size_t>(k)]);
            if ((next_byte & 0xC0) != 0x80) { valid = false; break; }
            code = (code << 6) | (next_byte & 0x3F);
        }

        if (valid) {
            consumed = static_cast<std::size_t>(extra) + 1;
        } else {
            code = 0xFFFD;
        }

        i += consumed;
        append_unicode_escape(out, code);
    }
    out += '"';
}

void append_utf8(std::string& out, std::uint32_t code)
{
    if (code <= 0x7F) {
        out += static_cast<char>(code);
    } else if (code <= 0x7FF) {
        out += static_cast<char>(0xC0 | (code >> 6));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else if (code <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (code >> 12));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (code >> 18));
        out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    }
}

[[noreturn]] void throw_wrong_type(const char* wanted, const value& v)
{
    throw std::runtime_error(std::string("json2: the value is a ") + v.type_name()
                             + ", not a " + wanted);
}

std::string read_whole_file(const std::filesystem::path& path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
        throw std::runtime_error("json2: cannot open for reading: " + path.string());

    std::string text;
    ifs.seekg(0, std::ios::end);
    const std::streamoff size = ifs.tellg();
    if (size > 0) {
        text.resize(static_cast<std::size_t>(size));
        ifs.seekg(0, std::ios::beg);
        ifs.read(text.data(), size);
    }
    return text;
}

// ── The parser ────────────────────────────────────────────────────────

/// How deep nesting may go. The parser is recursive, so this bounds its stack use.
constexpr std::size_t max_depth = 256;

class parser {
public:
    parser(const std::string& text, fidelity mode) : src_(text), mode_(mode) {}

    void run(document& doc);

private:
    [[noreturn]] void fail(const std::string& message) const
    {
        throw parsing_error(message + " (at offset " + std::to_string(pos_) + ")");
    }

    bool at_end() const { return pos_ >= src_.size(); }
    char peek() const { return at_end() ? '\0' : src_[pos_]; }
    char at(std::size_t offset) const
    {
        const std::size_t i = pos_ + offset;
        return i < src_.size() ? src_[i] : '\0';
    }

    void skip_ws()
    {
        while (!at_end() && is_ws(src_[pos_])) ++pos_;
    }

    /// The text between `from` and the current position, when the source is being kept.
    std::string slice(std::size_t from) const
    {
        return mode_ == fidelity::raw ? src_.substr(from, pos_ - from) : std::string();
    }

    void expect(char c)
    {
        if (peek() != c)
            fail(std::string("expected '") + c + "'");
        ++pos_;
    }

    value parse_value(std::size_t depth);
    value parse_array(std::size_t depth);
    value parse_object(std::size_t depth);
    value parse_string();
    value parse_number();
    value parse_literal(const char* text, const value& v);

    /// A string token, decoded. Leaves the cursor after the closing quote.
    std::string parse_string_token();
    std::uint32_t parse_hex4();

    const std::string& src_;
    fidelity mode_;
    std::size_t pos_ = 0;
};

std::uint32_t parser::parse_hex4()
{
    std::uint32_t code = 0;
    for (int i = 0; i < 4; ++i) {
        if (at_end()) fail("truncated \\u escape");
        const char c = src_[pos_++];
        code <<= 4;
        if (c >= '0' && c <= '9')      code |= static_cast<std::uint32_t>(c - '0');
        else if (c >= 'a' && c <= 'f') code |= static_cast<std::uint32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') code |= static_cast<std::uint32_t>(c - 'A' + 10);
        else fail("bad hex digit in a \\u escape");
    }
    return code;
}

std::string parser::parse_string_token()
{
    expect('"');

    std::string out;
    for (;;) {
        if (at_end()) fail("unterminated string");
        const unsigned char c = static_cast<unsigned char>(src_[pos_]);

        if (c == '"') { ++pos_; return out; }

        if (c == '\\') {
            ++pos_;
            if (at_end()) fail("unterminated escape");
            const char esc = src_[pos_++];
            switch (esc) {
            case '"':  out += '"';  break;
            case '\\': out += '\\'; break;
            case '/':  out += '/';  break;
            case 'b':  out += '\b'; break;
            case 'f':  out += '\f'; break;
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            case 't':  out += '\t'; break;
            case 'u': {
                std::uint32_t code = parse_hex4();
                // A high surrogate has to be followed by a low one; they encode one code
                // point together.
                if (code >= 0xD800 && code <= 0xDBFF && peek() == '\\' && at(1) == 'u') {
                    const std::size_t save = pos_;
                    pos_ += 2;
                    const std::uint32_t low = parse_hex4();
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                    } else {
                        pos_ = save; // not a pair after all; keep it as it was
                    }
                }
                append_utf8(out, code);
                break;
            }
            default:
                fail("unknown escape");
            }
            continue;
        }

        if (c < 0x20) fail("a control character has to be escaped inside a string");

        out += static_cast<char>(c);
        ++pos_;
    }
}

value parser::parse_string()
{
    const std::size_t start = pos_;
    value v(parse_string_token());
    if (mode_ == fidelity::raw) v.set_raw(src_.substr(start, pos_ - start));
    return v;
}

value parser::parse_number()
{
    const std::size_t start = pos_;
    bool is_floating = false;

    if (peek() == '-') ++pos_;
    if (peek() == '0') {
        ++pos_;
    } else if (is_digit(peek())) {
        while (is_digit(peek())) ++pos_;
    } else {
        fail("a number needs at least one digit");
    }

    if (peek() == '.') {
        is_floating = true;
        ++pos_;
        if (!is_digit(peek())) fail("a fraction needs at least one digit");
        while (is_digit(peek())) ++pos_;
    }

    if (peek() == 'e' || peek() == 'E') {
        is_floating = true;
        ++pos_;
        if (peek() == '+' || peek() == '-') ++pos_;
        if (!is_digit(peek())) fail("an exponent needs at least one digit");
        while (is_digit(peek())) ++pos_;
    }

    const std::string token = src_.substr(start, pos_ - start);

    value v;
    if (is_floating) {
        v = value(std::strtod(token.c_str(), nullptr));
    } else {
        errno = 0;
        const long long number = std::strtoll(token.c_str(), nullptr, 10);
        // An integer that does not fit in 64 bits is kept as a double, which loses precision
        // but keeps the document readable. The raw text is preserved either way.
        if (errno == ERANGE) v = value(std::strtod(token.c_str(), nullptr));
        else                 v = value(static_cast<std::int64_t>(number));
    }

    if (mode_ == fidelity::raw) v.set_raw(token);
    return v;
}

value parser::parse_literal(const char* text, const value& v)
{
    const std::size_t length = std::char_traits<char>::length(text);
    if (src_.compare(pos_, length, text) != 0) fail(std::string("expected ") + text);
    pos_ += length;

    value result = v;
    if (mode_ == fidelity::raw) result.set_raw(text);
    return result;
}

value parser::parse_array(std::size_t depth)
{
    expect('[');

    value v = value::make_array();
    array_body& body = v.as_array_body();

    for (;;) {
        const std::size_t lead_start = pos_;
        skip_ws();

        if (body.items.empty()) {
            if (peek() == ']') { body.tail = slice(lead_start); ++pos_; return v; }
        } else {
            if (peek() != ',') {
                if (peek() == ']') { body.tail = slice(lead_start); ++pos_; return v; }
                fail("expected ',' or ']'");
            }
            ++pos_;          // the comma belongs to the element that follows
            skip_ws();
            if (peek() == ']') fail("a trailing comma");
        }

        if (at_end()) fail("unterminated array");

        item element;
        element.lead = slice(lead_start);
        element.val = parse_value(depth + 1);
        body.items.push_back(std::move(element));
    }
}

value parser::parse_object(std::size_t depth)
{
    expect('{');

    value v = value::make_object();
    object_body& body = v.as_object_body();

    for (;;) {
        const std::size_t lead_start = pos_;
        skip_ws();

        if (body.members().empty()) {
            if (peek() == '}') { body.tail = slice(lead_start); ++pos_; return v; }
        } else {
            if (peek() != ',') {
                if (peek() == '}') { body.tail = slice(lead_start); ++pos_; return v; }
                fail("expected ',' or '}'");
            }
            ++pos_;
            skip_ws();
            if (peek() == '}') fail("a trailing comma");
        }

        if (at_end()) fail("unterminated object");
        if (peek() != '"') fail("a key has to be a string");

        member m;
        m.lead = slice(lead_start);

        const std::size_t key_start = pos_;
        m.key = parse_string_token();
        if (mode_ == fidelity::raw) m.raw_key = src_.substr(key_start, pos_ - key_start);

        const std::size_t between_start = pos_;
        skip_ws();
        expect(':');
        skip_ws();
        m.between = slice(between_start);

        m.val = parse_value(depth + 1);

        // In semantic mode the first member with a key wins, which is what std::map::insert
        // does in the json module. In raw mode every member is kept: a faithful round trip
        // cannot drop what the file contains.
        if (mode_ == fidelity::semantic && body.find(m.key) != nullptr) continue;
        body.members().push_back(std::move(m));
    }
}

value parser::parse_value(std::size_t depth)
{
    if (depth > max_depth) fail("nesting is too deep");
    if (at_end()) fail("unexpected end of input");

    switch (peek()) {
    case '{': return parse_object(depth);
    case '[': return parse_array(depth);
    case '"': return parse_string();
    case 't': return parse_literal("true", value(true));
    case 'f': return parse_literal("false", value(false));
    case 'n': return parse_literal("null", value(nullptr));
    default:
        if (peek() == '-' || is_digit(peek())) return parse_number();

        // A byte that is not printable is almost always an encoding problem (a byte order
        // mark in the middle, or UTF-16 text), so show it in hex rather than as a squiggle.
        {
            const unsigned char c = static_cast<unsigned char>(peek());
            if (c < 0x20 || c >= 0x7F) {
                char buffer[16];
                std::snprintf(buffer, sizeof(buffer), "\\x%02X", c);
                fail(std::string("unexpected byte '") + buffer + "'");
            }
            fail(std::string("unexpected character '") + static_cast<char>(c) + "'");
        }
    }
}

void parser::run(document& doc)
{
    // A byte order mark belongs to the prolog: it is kept, and stepped over. Only at the
    // very start is it a byte order mark; anywhere else it is just a byte.
    if (src_.compare(0, 3, "\xEF\xBB\xBF") == 0) pos_ = 3;

    const std::size_t prolog_start = 0;
    skip_ws();
    doc.set_prolog(slice(prolog_start));

    doc.root() = parse_value(0);

    const std::size_t epilog_start = pos_;
    skip_ws();
    if (!at_end()) fail("trailing text after the root value");
    doc.set_epilog(slice(epilog_start));
}

} // namespace <unnamed>

// ── object_body ───────────────────────────────────────────────────────

void object_body::ensure_index() const
{
    if (index_ready_ && index_.size() == members_.size()) return;

    index_.resize(members_.size());
    for (std::size_t i = 0; i < index_.size(); ++i)
        index_[i] = static_cast<std::uint32_t>(i);

    std::sort(index_.begin(), index_.end(), [this](std::uint32_t a, std::uint32_t b) {
        return members_[a].key < members_[b].key;
    });
    index_ready_ = true;
}

member* object_body::find(std::string_view key)
{
    if (members_.size() <= index_threshold) {
        for (member& m : members_)
            if (m.key == key) return &m;
        return nullptr;
    }

    ensure_index();
    const auto it = std::lower_bound(index_.begin(), index_.end(), key,
                                     [this](std::uint32_t position, std::string_view wanted) {
                                         return members_[position].key < wanted;
                                     });
    if (it == index_.end() || members_[*it].key != key) return nullptr;
    return &members_[*it];
}

const member* object_body::find(std::string_view key) const
{
    return const_cast<object_body*>(this)->find(key);
}

value* object_body::find_value(std::string_view key)
{
    member* m = find(key);
    return m ? &m->val : nullptr;
}

const value* object_body::find_value(std::string_view key) const
{
    const member* m = find(key);
    return m ? &m->val : nullptr;
}

member& object_body::append(std::string key)
{
    member m;
    // What a new member gets written with. The first one simply follows the '{'; later ones
    // copy what the object already puts around its last member, so an insertion into a
    // formatted document keeps that formatting instead of jamming itself in.
    if (!members_.empty()) {
        m.lead = ",";
        m.lead += tail;
        m.between = members_.back().between;
    }
    m.key = std::move(key);
    members_.push_back(std::move(m));
    index_ready_ = false;
    return members_.back();
}

member& object_body::operator[](std::string key)
{
    if (member* existing = find(key)) return *existing;

    // Not there yet. Note that find() may have built the index; append() drops it again.
    return append(std::move(key));
}

bool object_body::erase(std::string_view key)
{
    for (auto it = members_.begin(); it != members_.end(); ++it) {
        if (it->key != key) continue;

        // The comma that separated this member from the previous one lives in this member's
        // own lead, so removing the member takes the comma with it — except when it is the
        // first one, where the comma sits in the *next* member's lead and has to go instead.
        if (it == members_.begin() && it + 1 != members_.end()) {
            std::string& next_lead = (it + 1)->lead;
            const std::size_t comma = next_lead.find(',');
            if (comma != std::string::npos) next_lead.erase(comma, 1);
        }

        members_.erase(it);
        index_ready_ = false;
        return true;
    }
    return false;
}

void object_body::clear()
{
    members_.clear();
    index_.clear();
    index_ready_ = false;
}

// ── value ─────────────────────────────────────────────────────────────

value value::make_array()
{
    value v;
    v.data_ = array_body{};
    return v;
}

value value::make_object()
{
    value v;
    v.data_ = object_body{};
    return v;
}

const char* value::type_name() const noexcept
{
    switch (type()) {
    case value_type::null:     return "null";
    case value_type::boolean:  return "boolean";
    case value_type::integer:  return "integer";
    case value_type::floating: return "number";
    case value_type::string:   return "string";
    case value_type::array:    return "array";
    case value_type::object:   return "object";
    }
    return "value";
}

bool value::as_bool() const
{
    if (!is_bool()) throw_wrong_type("boolean", *this);
    return std::get<bool>(data_);
}

std::int64_t value::as_int() const
{
    if (is_double()) return static_cast<std::int64_t>(std::get<double>(data_));
    if (!is_int()) throw_wrong_type("number", *this);
    return std::get<std::int64_t>(data_);
}

double value::as_double() const
{
    if (is_int()) return static_cast<double>(std::get<std::int64_t>(data_));
    if (!is_double()) throw_wrong_type("number", *this);
    return std::get<double>(data_);
}

const std::string& value::as_string() const
{
    if (!is_string()) throw_wrong_type("string", *this);
    return std::get<std::string>(data_);
}

std::size_t value::size() const
{
    switch (type()) {
    case value_type::array:  return std::get<array_body>(data_).items.size();
    case value_type::object: return std::get<object_body>(data_).size();
    case value_type::string: return std::get<std::string>(data_).size();
    case value_type::null:   return 0;
    default:                 return 1;
    }
}

bool value::empty() const
{
    if (is_null()) return true;
    return size() == 0;
}

array_body& value::as_array_body()
{
    if (!is_array()) throw_wrong_type("array", *this);
    return std::get<array_body>(data_);
}

const array_body& value::as_array_body() const
{
    if (!is_array()) throw_wrong_type("array", *this);
    return std::get<array_body>(data_);
}

object_body& value::as_object_body()
{
    if (!is_object()) throw_wrong_type("object", *this);
    return std::get<object_body>(data_);
}

const object_body& value::as_object_body() const
{
    if (!is_object()) throw_wrong_type("object", *this);
    return std::get<object_body>(data_);
}

value& value::operator[](std::size_t index)
{
    array_body& body = as_array_body();
    if (index >= body.items.size())
        throw std::out_of_range("json2: element " + std::to_string(index) + " is out of range");
    return body.items[index].val;
}

const value& value::operator[](std::size_t index) const
{
    const array_body& body = as_array_body();
    if (index >= body.items.size())
        throw std::out_of_range("json2: element " + std::to_string(index) + " is out of range");
    return body.items[index].val;
}

void value::push_back(value v, std::string lead)
{
    if (is_null()) set_array();
    if (!is_array()) throw_wrong_type("array", *this);

    array_body& body = as_array_body();
    item element;
    element.lead = body.items.empty() ? std::string() : std::move(lead);
    element.val = std::move(v);
    body.items.push_back(std::move(element));
}

bool value::erase(std::size_t index)
{
    if (!is_array()) throw_wrong_type("array", *this);

    array_body& body = as_array_body();
    if (index >= body.items.size()) return false;

    // The comma that separated an element from the one before it sits in the *later* lead, so
    // when the first element goes, the next one has to lose it.
    if (index == 0 && body.items.size() > 1) {
        std::string& next_lead = body.items[1].lead;
        const std::size_t comma = next_lead.find(',');
        if (comma != std::string::npos) next_lead.erase(comma, 1);
    }

    body.items.erase(body.items.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool value::has_key(std::string_view key) const
{
    return is_object() && as_object_body().find(key) != nullptr;
}

value* value::find(std::string_view key)
{
    return is_object() ? as_object_body().find_value(key) : nullptr;
}

const value* value::find(std::string_view key) const
{
    return is_object() ? as_object_body().find_value(key) : nullptr;
}

value& value::operator[](std::string key)
{
    if (is_null()) set_object();
    if (!is_object()) throw_wrong_type("object", *this);
    return as_object_body()[std::move(key)].val;
}

bool value::erase_key(std::string_view key)
{
    return is_object() && as_object_body().erase(key);
}

void value::set_null()
{
    data_ = nullptr;
    raw_.clear();
}

void value::set_bool(bool b)
{
    data_ = b;
    raw_.clear();
}

void value::set_int(std::int64_t i)
{
    data_ = i;
    raw_.clear();
}

void value::set_double(double d)
{
    data_ = d;
    raw_.clear();
}

void value::set_string(std::string s)
{
    data_ = std::move(s);
    raw_.clear();
}

void value::set_array()
{
    data_ = array_body{};
    raw_.clear();
}

void value::set_object()
{
    data_ = object_body{};
    raw_.clear();
}

void value::clear()
{
    set_null();
}

void value::dump_to(std::string& out) const
{
    switch (type()) {
    case value_type::null:
        out += raw_.empty() ? "null" : raw_;
        return;

    case value_type::boolean:
        if (raw_.empty()) out += (std::get<bool>(data_) ? "true" : "false");
        else              out += raw_;
        return;

    case value_type::integer:
        out += raw_.empty() ? std::to_string(std::get<std::int64_t>(data_)) : raw_;
        return;

    case value_type::floating:
        out += raw_.empty() ? format_double(std::get<double>(data_)) : raw_;
        return;

    case value_type::string:
        if (raw_.empty()) append_escaped(std::get<std::string>(data_), out);
        else              out += raw_;
        return;

    case value_type::array: {
        const array_body& body = std::get<array_body>(data_);
        out += '[';
        for (std::size_t i = 0; i < body.items.size(); ++i) {
            const item& element = body.items[i];
            // The comma lives in the lead of the element that follows it. When there is no lead
            // to carry one — a hand built value, or a semantic parse — write it here.
            if (i != 0 && element.lead.find(',') == std::string::npos) out += ',';
            out += element.lead;
            element.val.dump_to(out);
        }
        out += body.tail;
        out += ']';
        return;
    }

    case value_type::object: {
        const object_body& body = std::get<object_body>(data_);
        out += '{';
        const std::vector<member>& members = body.members();
        for (std::size_t i = 0; i < members.size(); ++i) {
            const member& m = members[i];
            if (i != 0 && m.lead.find(',') == std::string::npos) out += ',';
            out += m.lead;

            if (m.key_dirty || m.raw_key.empty()) append_escaped(m.key, out);
            else                                  out += m.raw_key;

            if (m.between.empty()) out += ": ";
            else                   out += m.between;

            m.val.dump_to(out);
        }
        out += body.tail;
        out += '}';
        return;
    }
    }
}

std::string value::to_string(const exporter& fmt) const
{
    std::string out;
    regenerate(out, fmt, 0);
    return out;
}

std::string value::to_compact_string() const
{
    return to_string(exporter::compact_exporter());
}

void value::regenerate(std::string& out, const exporter& fmt, std::size_t level) const
{
    // The layout follows json_exporter's, defaults included: one member or element per line,
    // `": "` after a key, and an empty container on one line.
    const auto newline = [&] {
        if (fmt.isCompact) return;
        out += fmt.isInline ? " " : "\n";
    };
    const auto indent = [&](std::size_t depth) {
        if (fmt.isCompact || fmt.isInline) return;
        switch (fmt.indentStyle) {
        case indent_style::none:   break;
        case indent_style::space2: out.append(depth * 2, ' '); break;
        case indent_style::space4: out.append(depth * 4, ' '); break;
        case indent_style::tab:    out.append(depth, '\t');    break;
        }
    };

    switch (type()) {
    case value_type::null:
        out += "null";
        return;

    case value_type::boolean:
        out += (std::get<bool>(data_) ? "true" : "false");
        return;

    case value_type::integer:
        out += std::to_string(std::get<std::int64_t>(data_));
        return;

    case value_type::floating:
        out += format_double(std::get<double>(data_));
        return;

    case value_type::string:
        append_escaped(std::get<std::string>(data_), out, fmt.escapeNonAscii);
        return;

    case value_type::array: {
        const array_body& body = std::get<array_body>(data_);
        out += '[';
        if (!body.items.empty()) {
            newline();
            for (std::size_t i = 0; i < body.items.size(); ++i) {
                indent(level + 1);
                body.items[i].val.regenerate(out, fmt, level + 1);
                if (i + 1 != body.items.size()) out += ',';
                newline();
            }
            indent(level);
        }
        out += ']';
        return;
    }

    case value_type::object: {
        const object_body& body = std::get<object_body>(data_);
        out += '{';
        const std::vector<member>& members = body.members();
        if (!members.empty()) {
            newline();
            for (std::size_t i = 0; i < members.size(); ++i) {
                indent(level + 1);
                append_escaped(members[i].key, out, fmt.escapeNonAscii);
                out += fmt.isCompact ? ":" : ": ";
                members[i].val.regenerate(out, fmt, level + 1);
                if (i + 1 != members.size()) out += ',';
                newline();
            }
            indent(level);
        }
        out += '}';
        return;
    }
    }
}

std::string value::dump() const
{
    std::string out;
    dump_to(out);
    return out;
}

// ── document ──────────────────────────────────────────────────────────

document document::parse(std::string text, fidelity f)
{
    document doc;
    doc.mode_ = f;

    parser p(text, f);
    p.run(doc);

    return doc;
}

document document::from_file(const std::filesystem::path& path, fidelity f)
{
    return parse(read_whole_file(path), f);
}

std::string document::serialize() const
{
    std::string out;
    out += prolog_;
    root_.dump_to(out);
    out += epilog_;
    return out;
}

std::size_t document::to_file(const std::filesystem::path& path) const
{
    const std::string text = serialize();

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs)
        throw std::runtime_error("json2: cannot open for writing: " + path.string());

    ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!ofs)
        throw std::runtime_error("json2: cannot write: " + path.string());

    return text.size();
}

// ── pointer ───────────────────────────────────────────────────────────

namespace {

/// `~` becomes `~0` and `/` becomes `~1`, in that order.
std::string escape_segment(std::string_view segment)
{
    std::string out;
    out.reserve(segment.size());
    for (char c : segment) {
        if (c == '~')      out += "~0";
        else if (c == '/') out += "~1";
        else               out += c;
    }
    return out;
}

/// The other way round: `~1` is a `/` and `~0` is a `~`, so `~1` has to go first.
std::string unescape_segment(std::string_view segment)
{
    std::string out;
    out.reserve(segment.size());
    for (std::size_t i = 0; i < segment.size(); ++i) {
        if (segment[i] != '~' || i + 1 >= segment.size()) {
            out += segment[i];
            continue;
        }
        const char next = segment[++i];
        if (next == '0')      out += '~';
        else if (next == '1') out += '/';
        else {
            // Not an escape after all: RFC 6901 leaves this undefined, so keep it as written
            // rather than pretending it meant something.
            out += '~';
            out += next;
        }
    }
    return out;
}

} // namespace <unnamed>

void pointer::assign(std::string_view text)
{
    segments_.clear();

    if (text.empty()) return; // the whole document
    if (text.front() != '/')
        throw std::invalid_argument("json2: a JSON Pointer starts with '/': " + std::string(text));

    std::size_t start = 1;
    for (;;) {
        const std::size_t slash = text.find('/', start);
        const std::size_t end = (slash == std::string_view::npos) ? text.size() : slash;
        segments_.push_back(unescape_segment(text.substr(start, end - start)));
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
}

std::string pointer::str() const
{
    std::string out;
    for (const std::string& segment : segments_) {
        out += '/';
        out += escape_segment(segment);
    }
    return out;
}

// ── the accessors json also has ───────────────────────────────────────

const value& value::at(std::string_view key) const
{
    const value* found = find(key);
    if (!found) {
        if (!is_object()) throw std::out_of_range(std::string("json2: the value is a ")
                                                  + type_name() + ", not an object");
        throw std::out_of_range("json2: no member named '" + std::string(key) + "'");
    }
    return *found;
}

const value& value::front() const
{
    const array_body& body = as_array_body();
    if (body.items.empty()) throw std::out_of_range("json2: front() on an empty array");
    return body.items.front().val;
}

const value& value::back() const
{
    const array_body& body = as_array_body();
    if (body.items.empty()) throw std::out_of_range("json2: back() on an empty array");
    return body.items.back().val;
}

void value::pop_back()
{
    array_body& body = as_array_body();
    if (!body.items.empty()) body.items.pop_back();
}

void value::clear_as_array()
{
    data_ = array_body{};
    raw_.clear();
}

void value::clear_as_object()
{
    data_ = object_body{};
    raw_.clear();
}

// ── JSON Pointer ──────────────────────────────────────────────────────

value* value::find_path(const pointer& p)
{
    value* current = this;

    for (const std::string& segment : p.segments()) {
        if (current->is_object()) {
            member* found = current->as_object_body().find(segment);
            if (!found) return nullptr;
            current = &found->val;
            continue;
        }

        if (current->is_array()) {
            if (segment.empty()) return nullptr;

            // "-" is the token that means "one past the end" when writing, so it never names
            // anything here. A leading zero is not a valid index either.
            if (segment[0] == '0' && segment.size() > 1) return nullptr;

            std::size_t index = 0;
            for (char c : segment) {
                if (!is_digit(c)) return nullptr;
                if (index > (std::numeric_limits<std::size_t>::max() - 9) / 10) return nullptr;
                index = index * 10 + static_cast<std::size_t>(c - '0');
            }

            array_body& body = current->as_array_body();
            if (index >= body.items.size()) return nullptr;
            current = &body.items[index].val;
            continue;
        }

        // A path cannot go through a scalar.
        return nullptr;
    }

    return current;
}

const value* value::find_path(const pointer& p) const
{
    return const_cast<value*>(this)->find_path(p);
}

value& value::at_path(const pointer& p)
{
    if (value* found = find_path(p)) return *found;
    throw std::out_of_range("json2: the pointer does not name a value here: " + p.str());
}

const value& value::at_path(const pointer& p) const
{
    if (const value* found = find_path(p)) return *found;
    throw std::out_of_range("json2: the pointer does not name a value here: " + p.str());
}

// ── comparing ─────────────────────────────────────────────────────────

bool value::operator==(const value& other) const
{
    if (type() != other.type()) return false;

    switch (type()) {
    case value_type::null:
        return true;
    case value_type::boolean:
        return std::get<bool>(data_) == std::get<bool>(other.data_);
    case value_type::integer:
        return std::get<std::int64_t>(data_) == std::get<std::int64_t>(other.data_);
    case value_type::floating:
        return std::get<double>(data_) == std::get<double>(other.data_);
    case value_type::string:
        return std::get<std::string>(data_) == std::get<std::string>(other.data_);

    case value_type::array: {
        const array_body& a = std::get<array_body>(data_);
        const array_body& b = std::get<array_body>(other.data_);
        if (a.items.size() != b.items.size()) return false;
        for (std::size_t i = 0; i < a.items.size(); ++i)
            if (!(a.items[i].val == b.items[i].val)) return false;
        return true;
    }

    case value_type::object: {
        // By key, not by position: an object is a mapping, so member order is not part of what
        // it holds. A duplicate key is looked up as the first one, which is what lookup does
        // everywhere else here.
        const object_body& a = std::get<object_body>(data_);
        const object_body& b = std::get<object_body>(other.data_);
        if (a.size() != b.size()) return false;
        for (const member& m : b.members()) {
            const value* mine = a.find_value(m.key);
            if (!mine || !(*mine == m.val)) return false;
        }
        return true;
    }
    }
    return false;
}

// ── document, continued ───────────────────────────────────────────────

std::string document::to_string(const exporter& fmt) const
{
    // The prolog and the epilog go with the layout: what is left is the values.
    return root_.to_string(fmt);
}

std::string document::to_compact_string() const
{
    return root_.to_compact_string();
}

} // namespace scl2::json2
