/*
    encoding — the encoding boundary of SharedCppLib2.

    The library follows one rule, and this module is where it is written down: text inside is
    UTF-8 (a std::string holds UTF-8 bytes), text is converted where it meets the outside, and
    an invalid sequence becomes U+FFFD instead of being passed along.

    The helpers here cover the boundaries nothing else covers:

      - path_to_utf8()  : a path on its way to a person, a log or another UTF-8 file.
                          path.string() is not that — on Windows it converts through the ANSI
                          code page, so a path the code page cannot hold throws or mangles.
      - from_codepage() : text that is not UTF-8 at all (a GBK file, a *A system call).
      - to_codepage()   : the other direction, for a call that only takes ANSI.
      - from_ansi() / to_ansi() : the same, in the code page of the system.
      - is_valid_utf8() : what a caller wants to know before converting anything.

    UTF-8 to wide and back lives in `string` (scl2::str_to_wstr / scl2::wstr_to_str), because
    that is a text operation rather than a boundary one. The console is in
    platform::windows::enable_utf8_console(), next to the other Win32 specifics.

    No SharedCppLib2 dependency, so this is a standalone module that can be copied on its own.
    On Windows the code pages are the system's (MultiByteToWideChar / WideCharToMultiByte);
    elsewhere code pages do not exist and those conversions return the text unchanged.

    [SCL_STANDALONE_MODULE]
    version: 1.0.0
    cpp_generation: cxx17 - cxx23
*/

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace scl2 {

/// @brief Whether the bytes are valid UTF-8.
/// @param text The bytes to look at.
/// @return true when the whole text is UTF-8: no truncated sequence, no stray continuation
///         byte, no overlong form, no surrogate half, nothing above U+10FFFF.
bool is_valid_utf8(std::string_view text) noexcept;

/// @brief The path as UTF-8 text.
/// @param path The path.
/// @return The path in UTF-8, which is what std::filesystem::path::u8string() gives.
/// @note Use this instead of path.string() whenever the text goes to a person, a log, an error
///       message or another UTF-8 file: on Windows string() converts through the ANSI code page
///       and cannot represent everything a path may hold.
std::string path_to_utf8(const std::filesystem::path& path);

/// @brief Convert text that is in `codepage` into UTF-8.
/// @param codepage The code page the text is in (936 for GBK, 65001 for UTF-8, 0 for the
///                 system's ANSI code page).
/// @param text The text.
/// @return The text in UTF-8.
/// @throws std::runtime_error when the system does not know the code page.
/// @note Bytes the code page cannot map become the replacement character rather than being
///       rejected, so the result is always usable text.
/// @note Outside Windows there is one encoding and the text is returned unchanged.
std::string from_codepage(unsigned int codepage, std::string_view text);

/// @brief Convert UTF-8 text into `codepage`.
/// @param codepage The code page to write in.
/// @param text UTF-8 text.
/// @return The text in that code page.
/// @throws std::runtime_error when the system does not know the code page.
/// @note This is the lossy direction: a character the code page cannot hold becomes its
///       replacement character. Keep it as the last step before a call that needs it, so
///       nothing else has to work with the result.
/// @note Outside Windows there is one encoding and the text is returned unchanged.
std::string to_codepage(unsigned int codepage, std::string_view text);

/// @brief from_codepage() in the code page of the system.
/// @param text The text.
/// @return The text in UTF-8.
std::string from_ansi(std::string_view text);

/// @brief to_codepage() in the code page of the system.
/// @param text UTF-8 text.
/// @return The text in the code page of the system.
std::string to_ansi(std::string_view text);

} // namespace scl2
