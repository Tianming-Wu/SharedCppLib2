/*
    Variant module for SharedCppLib2

    Don't be confused, this is more close to QVariant than std::variant.

    It wraps the std::variant (why not) and provides some friendly conversion and access methods to them,
    especially between types and strings.

    Supported automatic conversions (used by the as_xxx / as<T> accessors):
        int <-> double            numeric casts
        int / double <-> string   parse / format
        bool <-> string           "true"/"false", "1"/"0", "yes"/"no", "on"/"off", or numeric
        int / double -> bool      non-zero

    Part of the `basic` library. See src/variant.cpp for the implementation.
    It looks so familiar... json_value!?
*/

#pragma once

#include <variant>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <type_traits>

#include "string.hpp"
#include "bytearray.hpp"

namespace scl2 {

class variant {
public:
    // Order must match the std::variant alternatives below.
    enum class type_t : uint8_t {
        Null, Bytearray, String, Bool, Int, Double, Array, Object
    };

    // ---- construction ----
    variant() = default;
    variant(std::nullptr_t);
    variant(const scl2::bytearray& b);
    variant(scl2::bytearray&& b);
    variant(const scl2::string& s);
    variant(scl2::string&& s);
    variant(const char* s);
    variant(bool b);
    variant(int v);
    variant(long v);
    variant(long long v);
    variant(unsigned v);
    variant(unsigned long v);
    variant(unsigned long long v);
    variant(float v);
    variant(double v);
    variant(long double v);
    variant(const std::vector<variant>& v);
    variant(std::vector<variant>&& v);
    variant(const std::map<scl2::string, variant>& m);
    variant(std::map<scl2::string, variant>&& m);

    // ---- type probing ----
    type_t type() const noexcept { return static_cast<type_t>(value.index()); }
    bool is_null() const noexcept { return type() == type_t::Null; }
    bool is_bytearray() const noexcept { return type() == type_t::Bytearray; }
    bool is_string() const noexcept { return type() == type_t::String; }
    bool is_bool() const noexcept { return type() == type_t::Bool; }
    bool is_int() const noexcept { return type() == type_t::Int; }
    bool is_double() const noexcept { return type() == type_t::Double; }
    bool is_number() const noexcept { return is_int() || is_double(); }
    bool is_array() const noexcept { return type() == type_t::Array; }
    bool is_object() const noexcept { return type() == type_t::Object; }

    // ---- conversion accessors (auto-convert where sensible) ----
    bool as_bool() const;
    int64_t as_int() const;
    long double as_double() const;
    scl2::string as_string() const;

    // ---- structured access (no conversion) ----
    /// @brief Convert to a bytearray using bytearray's serialize helpers:
    /// strings are length-prefixed (`append(string)`), scalars are raw bytes.
    /// @throw std::runtime_error for array/object values.
    scl2::bytearray to_bytearray() const;
    const scl2::bytearray& as_bytearray() const;
    scl2::bytearray& as_bytearray();
    const std::vector<variant>& as_array() const;
    std::vector<variant>& as_array();
    const std::map<scl2::string, variant>& as_object() const;
    std::map<scl2::string, variant>& as_object();

    // ---- type specific operations ----
    // -- array --
    const variant& operator[](size_t index) const;
    variant& operator[](size_t index);
    size_t array_size() const;
    size_t array_capacity() const;
    bool array_empty() const;
    void push_back(const variant& v);

    // -- object --
    const variant& operator[](const scl2::string& key) const;
    variant& operator[](const scl2::string& key);
    size_t object_size() const;
    bool object_empty() const;
    void insert(const scl2::string& key, const variant& value);
    
    // -- string --
    void append(const scl2::string& s);
    void append(const char* s);

    // -- bytearray --
    size_t bytearray_size() const;
    void bytearray_resize(size_t size);
    void append(const scl2::bytearray& ba);

    // ---- type specific operations, but automatically routed based on type ----
    /// @brief Get the size of the value, if applicable.
    /// @return For string/bytearray, the length; for array/object, the number of elements; for scalars, 1; for null, 0.
    size_t size() const;

    /// @brief Generic typed access; auto-converts scalar types.
    /// @throw std::runtime_error if the value cannot be converted.
    template<typename T>
    T as() const {
        if constexpr (std::is_same_v<T, bool>)                        return as_bool();
        else if constexpr (std::is_integral_v<T>)                     return static_cast<T>(as_int());
        else if constexpr (std::is_floating_point_v<T>)               return static_cast<T>(as_double());
        else if constexpr (std::is_same_v<T, scl2::string>)           return as_string();
        else if constexpr (std::is_same_v<T, std::string>)            return std::string(as_string());
        else if constexpr (std::is_same_v<T, scl2::bytearray>)        return as_bytearray();
        else if constexpr (std::is_same_v<T, std::vector<variant>>)   return as_array();
        else if constexpr (std::is_same_v<T, std::map<scl2::string, variant>>) return as_object();
        else static_assert(sizeof(T) == 0, "variant::as<T>: unsupported type T");
    }

    // ---- string round-trip ----
    scl2::string to_string() const { return as_string(); }

    /// @brief Parse a string into the most specific scalar type:
    /// "true"/"false" -> bool, numeric -> int / double, otherwise string.
    static variant fromString(const scl2::string& s);

    // ---- comparison ----
    bool operator==(const variant& o) const;
    bool operator!=(const variant& o) const { return !(*this == o); }

    // ---- load / dump serial ----
    scl2::bytearray dump() const;
    static variant load(const scl2::bytearray& data);

private:
    static bool parse_bool(const scl2::string& s);
    static int64_t parse_int(const scl2::string& s);
    static long double parse_double(const scl2::string& s);
    static scl2::string format_double(long double d);

    std::variant<
        std::nullptr_t,
        scl2::bytearray,
        scl2::string,
        bool,
        int64_t,
        long double,
        std::vector<variant>,
        std::map<scl2::string, variant>
    > value;
};

} // namespace scl2
