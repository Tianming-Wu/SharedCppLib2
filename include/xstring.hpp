/*
    xstring module for SharedCppLib2.

    This module provides a unified string class that can hold all kinds of strings,
    and provide a unified interface for all operations.

    This is part of a larger plan to support make modules support all kinds of strings
    without having to write separate code for each string type, or using templates.

    This module provides convenient conversion between different string types.
    And it does not do the simple version of converting everything to std::string,
    which may cause performance downgrade and loss of information.
    
    Instead, it will try to keep the original string type as much as possible, and only
    convert when necessary.
*/
#pragma once

#include "string.hpp"

#include <string>
#include <string_view>
#include <variant>
#include <stdexcept>
#include <type_traits>

namespace scl2 {

enum class xstype {
    null, ansi, wchar, u8char, u16char, u32char
};

class xstring {
public:
    using stype = xstype;

    // --- constructors ---

    xstring() = default;
    xstring(const xstring& other) = default;
    xstring(xstring&& other) noexcept = default;

    xstring(const scl2::string& s)  : value(s) {}
    xstring(const scl2::wstring& s) : value(s) {}
    xstring(const std::u8string& s)  : value(s) {}
    xstring(const std::u16string& s) : value(s) {}
    xstring(const std::u32string& s) : value(s) {}

    // --- type queries ---

    stype type() const;
    bool empty() const;
    size_t size() const;
    size_t length() const { return size(); }

    // --- typed access (copies) ---

    scl2::string a() const;
    scl2::wstring w() const;
    std::u8string u8() const;
    std::u16string u16() const;
    std::u32string u32() const;

    scl2::string str() const { return a(); }
    scl2::wstring wstr() const { return w(); }
    std::u8string u8str() const { return u8(); }
    std::u16string u16str() const { return u16(); }
    std::u32string u32str() const { return u32(); }

    // --- typed access (views, zero-copy when type matches) ---

    std::string_view a_view() const;
    std::wstring_view w_view() const;
    std::u8string_view u8_view() const;
    std::u16string_view u16_view() const;
    std::u32string_view u32_view() const;

    // --- conversions ---

    operator scl2::string() const;
    operator scl2::wstring() const;

    explicit operator std::string() const;
    explicit operator std::wstring() const;

    operator std::u8string() const;
    operator std::u16string() const;
    operator std::u32string() const;

    // --- assignment (switch type to match source) ---

    xstring& operator=(const xstring& other)      { value = other.value; return *this; }
    xstring& operator=(xstring&& other) noexcept   { value = std::move(other.value); return *this; }
    xstring& operator=(const scl2::string& s)      { value = s; return *this; }
    xstring& operator=(const scl2::wstring& s)     { value = s; return *this; }
    xstring& operator=(const std::u8string& s)     { value = s; return *this; }
    xstring& operator=(const std::u16string& s)    { value = s; return *this; }
    xstring& operator=(const std::u32string& s)    { value = s; return *this; }

    // --- assignment (keep current type, convert input) ---

    xstring& assign(const xstring& s);

    // --- type conversion ---

    xstring& convert(stype target);
    xstring converted(stype target) const;

    // --- mutation ---

    void clear();
    void reset();

    // --- passthrough operations ---

    xstring substr(size_t pos, size_t n) const;
    xstring substr(size_t pos) const;

    size_t find(const xstring& str, size_t pos = 0) const;
    size_t rfind(const xstring& str, size_t pos = std::string::npos) const;

    bool contains(const xstring& str) const;
    bool starts_with(const xstring& prefix) const;
    bool ends_with(const xstring& suffix) const;

    // --- comparison ---

    bool operator==(const xstring& other) const;
    bool operator!=(const xstring& other) const { return !(*this == other); }

private:
    using variant_type = std::variant<
        std::monostate,
        scl2::string,
        scl2::wstring,
        std::u8string,
        std::u16string,
        std::u32string
    >;
    variant_type value;

    // --- internal helpers ---

    template<typename T>
    T _match(const xstring& arg) const {
        if constexpr (std::is_same_v<T, scl2::string>)
            return arg.a();
        else if constexpr (std::is_same_v<T, scl2::wstring>)
            return arg.w();
        else if constexpr (std::is_same_v<T, std::u8string>)
            return arg.u8();
        else if constexpr (std::is_same_v<T, std::u16string>)
            return arg.u16();
        else if constexpr (std::is_same_v<T, std::u32string>)
            return arg.u32();
        else
            static_assert(sizeof(T) == 0, "xstring::_match: unsupported type");
    }

    template<typename F>
    xstring _call(F&& f) const {
        return std::visit([&]<typename T>(const T& s) -> xstring {
            if constexpr (std::is_same_v<T, std::monostate>)
                throw std::runtime_error("xstring: operation on null type");
            else
                return xstring(f(s));
        }, value);
    }

    template<typename R, typename F>
    R _query(F&& f) const {
        return std::visit([&]<typename T>(const T& s) -> R {
            if constexpr (std::is_same_v<T, std::monostate>) {
                if constexpr (std::is_same_v<R, size_t>)  return 0;
                if constexpr (std::is_same_v<R, bool>)    return false;
                throw std::runtime_error("xstring: query on null type");
            } else {
                return R(f(s));
            }
        }, value);
    }
};

/// @brief Convert a value to xstring via std::to_string / std::to_wstring.
/// @tparam T A type supported by std::to_string.
/// @tparam prefType Preferred string type (default: ansi). Determined at compile time.
template<typename T, xstype prefType = xstype::ansi>
xstring to_xstring(const T& val) {
    if constexpr (prefType == xstype::wchar) {
        return xstring(scl2::wstring(std::to_wstring(val)));
    } else {
        xstring result(scl2::string(std::to_string(val)));
        if constexpr (prefType != xstype::ansi)
            result.convert(prefType);
        return result;
    }
}

} // namespace scl2