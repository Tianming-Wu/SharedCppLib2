#include "variant.hpp"

#include <charconv>
#include <sstream>
#include <stdexcept>

namespace scl2 {

// ---- construction ----

variant::variant(std::nullptr_t) : value(nullptr) {}
variant::variant(const scl2::bytearray& b) : value(b) {}
variant::variant(scl2::bytearray&& b) : value(std::move(b)) {}
variant::variant(const scl2::string& s) : value(s) {}
variant::variant(scl2::string&& s) : value(std::move(s)) {}
variant::variant(const char* s) : value(scl2::string(s)) {}
variant::variant(bool b) : value(b) {}
variant::variant(int v) : value(static_cast<int64_t>(v)) {}
variant::variant(long v) : value(static_cast<int64_t>(v)) {}
variant::variant(long long v) : value(static_cast<int64_t>(v)) {}
variant::variant(unsigned v) : value(static_cast<int64_t>(v)) {}
variant::variant(unsigned long v) : value(static_cast<int64_t>(v)) {}
variant::variant(unsigned long long v) : value(static_cast<int64_t>(v)) {}
variant::variant(float v) : value(static_cast<long double>(v)) {}
variant::variant(double v) : value(static_cast<long double>(v)) {}
variant::variant(long double v) : value(v) {}
variant::variant(const std::vector<variant>& v) : value(v) {}
variant::variant(std::vector<variant>&& v) : value(std::move(v)) {}
variant::variant(const std::map<scl2::string, variant>& m) : value(m) {}
variant::variant(std::map<scl2::string, variant>&& m) : value(std::move(m)) {}

// ---- conversion accessors ----

bool variant::as_bool() const
{
    switch (type()) {
        case type_t::Bool:   return std::get<3>(value);
        case type_t::Int:    return std::get<4>(value) != 0;
        case type_t::Double: return std::get<5>(value) != 0.0L;
        case type_t::String: return parse_bool(std::get<2>(value));
        default: throw std::runtime_error("variant: value has no bool");
    }
}

int64_t variant::as_int() const
{
    switch (type()) {
        case type_t::Int:    return std::get<4>(value);
        case type_t::Double: return static_cast<int64_t>(std::get<5>(value));
        case type_t::Bool:   return std::get<3>(value) ? 1 : 0;
        case type_t::String: return parse_int(std::get<2>(value));
        default: throw std::runtime_error("variant: value has no int");
    }
}

long double variant::as_double() const
{
    switch (type()) {
        case type_t::Double: return std::get<5>(value);
        case type_t::Int:    return static_cast<long double>(std::get<4>(value));
        case type_t::Bool:   return std::get<3>(value) ? 1.0L : 0.0L;
        case type_t::String: return parse_double(std::get<2>(value));
        default: throw std::runtime_error("variant: value has no double");
    }
}

scl2::string variant::as_string() const
{
    switch (type()) {
        case type_t::String: return std::get<2>(value);
        case type_t::Int:    return scl2::string(std::to_string(std::get<4>(value)));
        case type_t::Double: return format_double(std::get<5>(value));
        case type_t::Bool:   return std::get<3>(value) ? scl2::string("true") : scl2::string("false");
        case type_t::Null:   return scl2::string();
        default: throw std::runtime_error("variant: value has no string");
    }
}

// ---- structured access ----

scl2::bytearray variant::to_bytearray() const
{
    switch (type()) {
        case type_t::Bytearray: return std::get<1>(value);
        case type_t::String: {
            scl2::bytearray ba;
            ba.append(std::string(std::get<2>(value)));
            return ba;
        }
        case type_t::Bool:   { scl2::bytearray ba; ba.append(std::get<3>(value)); return ba; }
        case type_t::Int:    { scl2::bytearray ba; ba.append(std::get<4>(value)); return ba; }
        case type_t::Double: { scl2::bytearray ba; ba.append(std::get<5>(value)); return ba; }
        case type_t::Null:   return scl2::bytearray();
        default: throw std::runtime_error("variant: cannot convert array/object to bytearray");
    }
}

const scl2::bytearray& variant::as_bytearray() const
{
    if (!is_bytearray()) throw std::runtime_error("variant: value is not a bytearray");
    return std::get<1>(value);
}

scl2::bytearray& variant::as_bytearray()
{
    if (!is_bytearray()) throw std::runtime_error("variant: value is not a bytearray");
    return std::get<1>(value);
}

const std::vector<variant>& variant::as_array() const
{
    if (!is_array()) throw std::runtime_error("variant: value is not an array");
    return std::get<6>(value);
}

std::vector<variant>& variant::as_array()
{
    if (!is_array()) throw std::runtime_error("variant: value is not an array");
    return std::get<6>(value);
}

const std::map<scl2::string, variant>& variant::as_object() const
{
    if (!is_object()) throw std::runtime_error("variant: value is not an object");
    return std::get<7>(value);
}

std::map<scl2::string, variant>& variant::as_object()
{
    if (!is_object()) throw std::runtime_error("variant: value is not an object");
    return std::get<7>(value);
}

// ---- string round-trip ----

variant variant::fromString(const scl2::string& s)
{
    if (s == "true" || s == "false") return variant(s == "true");
    // whole integer?
    {
        int64_t iv = 0;
        auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), iv);
        if (ec == std::errc{} && p == s.data() + s.size()) return variant(iv);
    }
    // floating point?
    try {
        size_t pos = 0;
        long double dv = std::stold(s, &pos);
        if (pos == s.size()) return variant(dv);
    } catch (...) { /* fall through */ }
    return variant(s);
}

// ---- comparison ----

bool variant::operator==(const variant& o) const
{
    if (type() == o.type()) return value == o.value;
    if (is_number() && o.is_number()) return as_double() == o.as_double();
    return false;
}

// ---- helpers ----

bool variant::parse_bool(const scl2::string& s)
{
    if (s == "true" || s == "1" || s == "yes" || s == "on") return true;
    if (s == "false" || s == "0" || s == "no" || s == "off") return false;
    try {
        return std::stold(s) != 0.0L;
    } catch (...) {
        throw std::runtime_error("variant: cannot convert string to bool");
    }
}

int64_t variant::parse_int(const scl2::string& s)
{
    int64_t v = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size())
        throw std::runtime_error("variant: cannot convert string to int");
    return v;
}

long double variant::parse_double(const scl2::string& s)
{
    try {
        size_t pos = 0;
        long double v = std::stold(s, &pos);
        if (pos != s.size())
            throw std::runtime_error("variant: cannot convert string to double");
        return v;
    } catch (const std::runtime_error&) { throw; }
    catch (...) {
        throw std::runtime_error("variant: cannot convert string to double");
    }
}

scl2::string variant::format_double(long double d)
{
    std::ostringstream oss;
    oss << d;
    return scl2::string(oss.str());
}

} // namespace scl2
