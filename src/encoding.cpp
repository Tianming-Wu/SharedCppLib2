#include "encoding.hpp"

#include <cstdint>
#include <stdexcept>

#if defined(_WIN32) || defined(_WIN64)
    #define SCL2_ENCODING_WINDOWS
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

namespace scl2 {

namespace {

#ifdef SCL2_ENCODING_WINDOWS
// CP_ACP is 0 and means "the code page of the system", which takes a call of its own.
unsigned int resolve_codepage(unsigned int codepage) {
    return codepage == 0u ? static_cast<unsigned int>(GetACP()) : codepage;
}
#endif

std::string codepage_error(const char* what, unsigned int codepage) {
    return std::string(what) + ": the system does not accept code page "
         + std::to_string(codepage);
}

} // namespace

bool is_valid_utf8(std::string_view text) noexcept
{
    const auto* p = reinterpret_cast<const unsigned char*>(text.data());
    const auto* end = p + text.size();

    while (p < end) {
        const unsigned char lead = *p++;
        if (lead < 0x80) continue;                  // plain ASCII

        int extra = 0;
        std::uint32_t codepoint = 0;
        if (lead >= 0xC2 && lead <= 0xDF)      { extra = 1; codepoint = lead & 0x1Fu; }
        else if (lead >= 0xE0 && lead <= 0xEF) { extra = 2; codepoint = lead & 0x0Fu; }
        else if (lead >= 0xF0 && lead <= 0xF4) { extra = 3; codepoint = lead & 0x07u; }
        else return false;                          // continuation byte, C0, C1, F5..FF

        for (int i = 0; i < extra; ++i) {
            if (p >= end) return false;             // truncated sequence
            const unsigned char next = *p++;
            if ((next & 0xC0) != 0x80) return false;
            codepoint = (codepoint << 6) | (next & 0x3Fu);
        }

        // Only the shortest form is UTF-8.
        static constexpr std::uint32_t minimum[4] = { 0x00u, 0x80u, 0x800u, 0x10000u };
        if (codepoint < minimum[extra]) return false;
        if (codepoint > 0x10FFFFu) return false;
        if (codepoint >= 0xD800u && codepoint <= 0xDFFFu) return false;   // surrogate half
    }
    return true;
}

std::string path_to_utf8(const std::filesystem::path& path)
{
    // u8string() is std::string up to C++17 and std::u8string from C++20 on; both hold UTF-8,
    // and char8_t has to be looked at through char to get back into a std::string.
    const auto utf8 = path.u8string();
    if constexpr (std::is_same_v<decltype(utf8), std::string>)
        return utf8;
    else
        return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

#ifdef SCL2_ENCODING_WINDOWS

std::string from_codepage(unsigned int codepage, std::string_view text)
{
    if (text.empty()) return {};
    const unsigned int cp = resolve_codepage(codepage);

    const int wide_len = MultiByteToWideChar(cp, 0, text.data(),
                                             static_cast<int>(text.size()), nullptr, 0);
    if (wide_len <= 0) throw std::runtime_error(codepage_error("from_codepage", cp));

    std::wstring wide(static_cast<std::size_t>(wide_len), L'\0');
    MultiByteToWideChar(cp, 0, text.data(), static_cast<int>(text.size()),
                        wide.data(), wide_len);

    const int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_len,
                                             nullptr, 0, nullptr, nullptr);
    if (utf8_len <= 0) throw std::runtime_error(codepage_error("from_codepage", CP_UTF8));

    std::string result(static_cast<std::size_t>(utf8_len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_len,
                        result.data(), utf8_len, nullptr, nullptr);
    return result;
}

std::string to_codepage(unsigned int codepage, std::string_view text)
{
    if (text.empty()) return {};
    const unsigned int cp = resolve_codepage(codepage);

    const int wide_len = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                             static_cast<int>(text.size()), nullptr, 0);
    if (wide_len <= 0) throw std::runtime_error(codepage_error("to_codepage", CP_UTF8));

    std::wstring wide(static_cast<std::size_t>(wide_len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        wide.data(), wide_len);

    const int narrow_len = WideCharToMultiByte(cp, 0, wide.data(), wide_len,
                                               nullptr, 0, nullptr, nullptr);
    if (narrow_len <= 0) throw std::runtime_error(codepage_error("to_codepage", cp));

    std::string result(static_cast<std::size_t>(narrow_len), '\0');
    WideCharToMultiByte(cp, 0, wide.data(), wide_len,
                        result.data(), narrow_len, nullptr, nullptr);
    return result;
}

#else

std::string from_codepage(unsigned int codepage, std::string_view text)
{
    // There is one encoding here, and this is it.
    (void)codepage;
    return std::string(text);
}

std::string to_codepage(unsigned int codepage, std::string_view text)
{
    (void)codepage;
    return std::string(text);
}

#endif // SCL2_ENCODING_WINDOWS

std::string from_ansi(std::string_view text) { return from_codepage(0u, text); }
std::string to_ansi(std::string_view text)   { return to_codepage(0u, text); }

} // namespace scl2
