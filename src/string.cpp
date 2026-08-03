#include "string.hpp"

#include "stringlist_regex.hpp"
#include "platform.hpp"

namespace scl2 {

template <typename CharT>
scl2::basic_stringlist<CharT> basic_string<CharT>::split(CharT delim) const
{ return scl2::basic_stringlist<CharT>::split(*this, delim); }

template <typename CharT>
scl2::basic_stringlist<CharT> basic_string<CharT>::split(const string_type &delim) const
{ return scl2::basic_stringlist<CharT>::split(*this, delim); }

template <typename CharT>
scl2::basic_stringlist<CharT> basic_string<CharT>::split(const scl2::basic_stringlist<CharT> &delims) const
{ return scl2::basic_stringlist<CharT>::split(*this, delims); }

template <typename CharT>
scl2::basic_stringlist<CharT> basic_string<CharT>::xsplit(const string_type &delim, const string_type &begin_bind, string_type end_bind, bool remove_binding) const
{ return scl2::basic_stringlist<CharT>::xsplit(*this, delim, begin_bind, end_bind, remove_binding); }

template <typename CharT>
scl2::basic_stringlist<CharT> basic_string<CharT>::exsplit(const string_type &delim, const string_type &begin_bind, string_type end_bind, bool remove_binding, bool strict) const
{ return scl2::basic_stringlist<CharT>::exsplit(*this, delim, begin_bind, end_bind, remove_binding, strict); }

template <typename CharT>
constexpr const CharT* whitespace_charset() {
    if constexpr (std::is_same_v<CharT, char>)
        return " \t\n\r\f\v";
    else
        return L" \t\n\r\f\v";
}

template <typename CharT>
scl2::basic_string<CharT> basic_string<CharT>::trim()
{
    string_type result = *this;
    auto start = result.find_first_not_of(whitespace_charset<CharT>());
    if (start == string_type::npos) {
        result.clear();
        return result;
    }
    auto end = result.find_last_not_of(whitespace_charset<CharT>());
    result = result.substr(start, end - start + 1);
    return result;
}

// Regex-based methods commented out: regex_chop/extract are char-only,
// but templates must compile for both char and wchar_t.

// template <typename CharT>
// scl2::basic_stringlist<CharT> basic_string<CharT>::split(const std::regex &regex_delim) const
// { return regex_chop(*this, regex_delim); }

// template <typename CharT>
// scl2::basic_stringlist<CharT> basic_string<CharT>::extract(const std::regex &regex_pattern) const
// { return regex_extract(*this, regex_pattern); }

template class basic_string<char>;
template class basic_string<wchar_t>;

// --- String conversion utilities ---

std::wstring str_to_wstr(const std::string& str)
{
#ifdef OS_WINDOWS
    if (str.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), result.data(), len);
    return result;
#else
    if (str.empty()) return {};
    std::wstring result;
    result.reserve(str.size());
    const auto* p = reinterpret_cast<const uint8_t*>(str.data());
    const auto* end = p + str.size();
    while (p < end) {
        wchar_t cp;
        if (*p < 0x80) {
            cp = *p++;
        } else if (*p < 0xE0) {
            cp = static_cast<wchar_t>(*p++ & 0x1F) << 6;
            cp |= (*p++ & 0x3F);
        } else if (*p < 0xF0) {
            cp = static_cast<wchar_t>(*p++ & 0x0F) << 12;
            cp |= static_cast<wchar_t>(*p++ & 0x3F) << 6;
            cp |= (*p++ & 0x3F);
        } else {
            cp = static_cast<wchar_t>(*p++ & 0x07) << 18;
            cp |= static_cast<wchar_t>(*p++ & 0x3F) << 12;
            cp |= static_cast<wchar_t>(*p++ & 0x3F) << 6;
            cp |= (*p++ & 0x3F);
        }
        result += cp;
    }
    return result;
#endif
}

std::string wstr_to_str(const std::wstring& wstr)
{
#ifdef OS_WINDOWS
    if (wstr.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), result.data(), len, nullptr, nullptr);
    return result;
#else
    if (wstr.empty()) return {};
    std::string result;
    result.reserve(wstr.size() * 3);
    for (wchar_t cp : wstr) {
        if (cp < 0x80) {
            result += static_cast<char>(cp);
        } else if (cp < 0x800) {
            result += static_cast<char>(0xC0 | (cp >> 6));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            result += static_cast<char>(0xE0 | (cp >> 12));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            result += static_cast<char>(0xF0 | (cp >> 18));
            result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return result;
#endif
}

} // namespace scl2