/*
    String module for SharedCppLib2.

    This layer provides some better intergration with SharedCppLib2
    for std::string.
*/

#pragma once

#include "stringlist.hpp"

#include <string>
#include <regex>

namespace scl2 {

template <typename CharT>
class basic_string : public std::basic_string<CharT> {
public:
    using std::basic_string<CharT>::basic_string; // inherit all constructors

    typedef std::basic_string<CharT> string_type;
    typedef string_type value_type;

    // MSVC sometimes fails to inherit conversion from base type
    basic_string(const string_type& s) : string_type(s) {}
    basic_string(string_type&& s) noexcept : string_type(std::move(s)) {}

    // stringlist-related methods

    scl2::basic_stringlist<CharT> split(CharT delim) const;
    scl2::basic_stringlist<CharT> split(const string_type &delim) const;
    scl2::basic_stringlist<CharT> split(const scl2::basic_stringlist<CharT> &delims) const;

    /// @brief split a string into a stringlist, while the binding characters will be treated as a whole and not be split
    /// @param delim delimiter
    /// @param begin_bind a series of chars that is treated as combinitions
    /// @param end_bind paired one-by-one to the @c begin_bind , and will be the same as it if left empty
    scl2::basic_stringlist<CharT> xsplit(const string_type &delim, const string_type &begin_bind, string_type end_bind = string_type(), bool remove_binding = true) const;

    /// @brief almost the same as xsplit, while it supports binding characters to be found inside the string
    scl2::basic_stringlist<CharT> exsplit(const string_type &delim, const string_type &begin_bind, string_type end_bind = string_type(), bool remove_binding = false, bool strict = false) const;

    /// @brief Split a string into a stringlist using a regex as the delimiter.
    /// @param regex_delim
    // scl2::basic_stringlist<CharT> split(const std::regex& regex_delim) const;

    /// @brief Extract substrings from the string that match the given regex pattern.
    /// @param pattern
    // scl2::basic_stringlist<CharT> extract(const std::regex& pattern) const;

    // utility methods

    /// @brief Remove whitespace from the beginning and end of the string.
    scl2::basic_string<CharT> trim();
};

extern template class basic_string<char>;
extern template class basic_string<wchar_t>;

using string = basic_string<char>;
using wstring = basic_string<wchar_t>;

// --- String conversion utilities ---

/// @brief Convert UTF-8 string to wide string.
std::wstring str_to_wstr(const std::string& str);

/// @brief Convert wide string to UTF-8 string.
std::string wstr_to_str(const std::wstring& wstr);

} // namespace scl2