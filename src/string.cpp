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

template <typename CharT>
void basic_string<CharT>::find_and_replace(const scl2::basic_string<CharT> &target, const scl2::basic_string<CharT> &replacement)
{
    size_t pos = std::basic_string<CharT>::npos, lpos = 0;
    while ( (pos = this->find(target, lpos)) != std::basic_string<CharT>::npos) {
        this->replace(pos, target.length(), replacement);
        lpos = pos + replacement.length(); // advance cursor to avoid infinite loop if replacement contains target
    }
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

namespace {

// Append one Unicode code point to a UTF-8 string.
// Code points that are not Unicode scalar values become U+FFFD.
// (The standalone json module carries its own copy of these two helpers,
//  since it must not depend on this module.)
void append_utf8(std::string& out, uint32_t codepoint)
{
    if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF))
        codepoint = 0xFFFD;

    if (codepoint <= 0x7F) {
        out += static_cast<char>(codepoint);
    } else if (codepoint <= 0x7FF) {
        out += static_cast<char>(0xC0 | (codepoint >> 6));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (codepoint >> 12));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (codepoint >> 18));
        out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
}

// Decode one UTF-8 sequence and advance p past it.
// Truncated or invalid input yields U+FFFD and consumes one byte, so the
// caller never reads past the end and never sees a partial sequence.
uint32_t next_utf8(const uint8_t*& p, const uint8_t* end)
{
    const uint8_t b = *p++;
    if (b < 0x80) return b;

    int extra = 0;
    uint32_t codepoint = 0;
    if      ((b & 0xE0) == 0xC0) { extra = 1; codepoint = b & 0x1F; }
    else if ((b & 0xF0) == 0xE0) { extra = 2; codepoint = b & 0x0F; }
    else if ((b & 0xF8) == 0xF0) { extra = 3; codepoint = b & 0x07; }
    else return 0xFFFD;

    for (int i = 0; i < extra; ++i) {
        if (p >= end) return 0xFFFD;
        const uint8_t nb = *p;
        if ((nb & 0xC0) != 0x80) return 0xFFFD;
        ++p;
        codepoint = (codepoint << 6) | (nb & 0x3F);
    }

    // Only the shortest form is valid UTF-8.
    static constexpr uint32_t minimum[4] = { 0x00, 0x80, 0x800, 0x10000 };
    if (codepoint < minimum[extra]) return 0xFFFD;

    if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF))
        return 0xFFFD;
    return codepoint;
}

} // namespace

std::wstring str_to_wstr(const std::string& str)
{
#ifdef OS_WINDOWS
    if (str.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), result.data(), len);
    return result;
#else
    // wchar_t is a Unicode scalar value on these platforms.
    std::wstring result;
    result.reserve(str.size());
    const auto* p = reinterpret_cast<const uint8_t*>(str.data());
    const auto* end = p + str.size();
    while (p < end) {
        result += static_cast<wchar_t>(next_utf8(p, end));
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
    std::string result;
    result.reserve(wstr.size() * 3);
    for (wchar_t wc : wstr) {
        append_utf8(result, static_cast<uint32_t>(wc));
    }
    return result;
#endif
}

} // namespace scl2