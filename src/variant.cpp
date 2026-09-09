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

const variant &variant::operator[](size_t index) const
{
    if (!is_array()) throw std::runtime_error("variant: value is not an array");
    const auto& arr = std::get<6>(value);
    if (index >= arr.size()) throw std::runtime_error("variant: array index out of bounds");
    return arr[index];
}

variant &variant::operator[](size_t index)
{
    if (!is_array()) throw std::runtime_error("variant: value is not an array");
    auto& arr = std::get<6>(value);
    if (index >= arr.size()) throw std::runtime_error("variant: array index out of bounds");
    return arr[index];
}

size_t variant::array_size() const
{
    if (!is_array()) throw std::runtime_error("variant: value is not an array");
    const auto& arr = std::get<6>(value);
    return arr.size();
}

size_t variant::array_capacity() const
{
    if (!is_array()) throw std::runtime_error("variant: value is not an array");
    const auto& arr = std::get<6>(value);
    return arr.capacity();
}

bool variant::array_empty() const
{
    if (!is_array()) throw std::runtime_error("variant: value is not an array");
    const auto& arr = std::get<6>(value);
    return arr.empty();
}

void variant::push_back(const variant &v)
{
    if (!is_array()) throw std::runtime_error("variant: value is not an array");
    std::get<6>(value).push_back(v);
}

const variant &variant::operator[](const scl2::string &key) const
{
    if (!is_object()) throw std::runtime_error("variant: value is not an object");
    const auto& obj = std::get<7>(value);
    auto it = obj.find(key);
    if (it == obj.end()) throw std::runtime_error("variant: key not found");
    return it->second;
}

variant &variant::operator[](const scl2::string &key)
{
    if (!is_object()) throw std::runtime_error("variant: value is not an object");
    auto& obj = std::get<7>(value);
    return obj[key];
}

size_t variant::object_size() const
{
    if (!is_object()) throw std::runtime_error("variant: value is not an object");
    const auto& obj = std::get<7>(value);
    return obj.size();
}

bool variant::object_empty() const
{
    if (!is_object()) throw std::runtime_error("variant: value is not an object");
    const auto& obj = std::get<7>(value);
    return obj.empty();
}

void variant::insert(const scl2::string &key, const variant &value)
{
    if (!is_object()) throw std::runtime_error("variant: value is not an object");
    as_object()[key] = value;
}

void variant::append(const scl2::string &s)
{
    if (!is_string()) throw std::runtime_error("variant: value is not a string");
    std::get<2>(value) += s;
}

void variant::append(const char *s)
{
    if (!is_string()) throw std::runtime_error("variant: value is not a string");
    std::get<2>(value) += s;
}

size_t variant::bytearray_size() const
{
    if (!is_bytearray()) throw std::runtime_error("variant: value is not a bytearray");
    const auto& ba = std::get<1>(value);
    return ba.size();
}

void variant::bytearray_resize(size_t size)
{
    if (!is_bytearray()) throw std::runtime_error("variant: value is not a bytearray");
    std::get<1>(value).resize(size);
}

void variant::append(const scl2::bytearray &ba)
{
    if (!is_bytearray()) throw std::runtime_error("variant: value is not a bytearray");
    std::get<1>(value).append(ba);
}

size_t variant::size() const
{
    switch (type()) {
        case type_t::Array: return array_size();
        case type_t::Object: return object_size();
        case type_t::String: return as_string().size();
        case type_t::Bytearray: return bytearray_size();
        case type_t::Null: return 0;
        default:
            return 1;
    }
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

scl2::bytearray variant::dump() const
{
    scl2::bytearray data;
    data.append(static_cast<uint8_t>(type()));
    switch (type()) {
        case type_t::Null: break;
        case type_t::Bytearray: {
            data.append<size_t>(as_bytearray().size());
            data.append(as_bytearray());
            break;
        }
        case type_t::String: data.append(as_string()); break;
        case type_t::Bool: data.append<bool>(as_bool()); break;
        case type_t::Int: data.append(as_int()); break;
        case type_t::Double: data.append(as_double()); break;
        case type_t::Array: {
            const auto& arr = as_array();
            data.append<size_t>(arr.size());
            for (const auto& v : arr) {
                scl2::bytearray vdata = v.dump();
                data.append<size_t>(vdata.size());
                data.append(vdata);
            }
            break;
        }
        case type_t::Object: {
            const auto& obj = as_object();
            data.append<size_t>(obj.size());
            for (const auto& [key, value] : obj) {
                data.append(key);
                scl2::bytearray vdata = value.dump();
                data.append<size_t>(vdata.size());
                data.append(vdata);
            }
            break;
        }
        default:
            throw std::runtime_error("variant: unknown type in dump");
    }
    return data;
}

variant variant::load(const scl2::bytearray &data)
{
    variant v;
    type_t expectedType = data.read<type_t>();

    switch (expectedType) {
        case type_t::Null: v = variant(nullptr); break;
        case type_t::Bytearray: {
            size_t bsize = data.read<size_t>();
            v = variant(data.readBytes(bsize));
            break;
        }
        case type_t::String: v = variant(data.readString()); break;
        case type_t::Bool: v = variant(data.read<bool>()); break;
        case type_t::Int: v = variant(data.read<int64_t>()); break;
        case type_t::Double: v = variant(data.read<long double>()); break;
        case type_t::Array: {
            size_t arrSize = data.read<size_t>();
            std::vector<variant> arr;
            for (size_t i = 0; i < arrSize; ++i) {
                size_t elemSize = data.read<size_t>();
                scl2::bytearray elemData = data.readBytes(elemSize);
                arr.push_back(variant::load(elemData));
            }
            v = variant(std::move(arr));
            break;
        }
        case type_t::Object: {
            size_t objSize = data.read<size_t>();
            std::map<scl2::string, variant> obj;
            for (size_t i = 0; i < objSize; ++i) {
                scl2::string key = data.readString();
                size_t valueSize = data.read<size_t>();
                scl2::bytearray valueData = data.readBytes(valueSize);
                obj[key] = variant::load(valueData);
            }
            v = variant(std::move(obj));
            break;
        }
        default:
            throw std::runtime_error("variant: unknown type in load");
    }
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
