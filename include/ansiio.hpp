/*
    ANSI IO library for Ansi terminal features.

    Note: cursor control functions are just not working correnctly. You'd better
    not use them.
*/
#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <cstdint>
#include <vector>

#include "basics.hpp"
#include "color.hpp"
#include "scltypes.hpp"
#include "stringlist.hpp" // in basic anyway, no additional cost

namespace scl2 {
inline namespace ansi {

int map_system_color(const color& c, bool is_background);
std::string to_ansi_code(const color& c, const color& b = colors::null);

inline std::wstring wto_ansi_code(const color& c, const color& b = colors::null) {
    auto s = to_ansi_code(c, b);
    return {s.begin(), s.end()};
}

/// @brief Reset all ANSI attributes (colors, bold, underline, etc.) to default.
constexpr std::string_view reset_color = "\033[0m";
constexpr std::wstring_view wreset_color = L"\033[0m";

struct text_style {
    tri_state bold = tri_state::not_set;
    tri_state italic = tri_state::not_set;
    tri_state underline = tri_state::not_set;
    bool is_inherit = false;
    color text_color = colors::null;
    bool text_color_inherit = false;
    color background_color = colors::null;
    bool background_color_inherit = false;
};

typedef std::vector<text_style> sentence_style; 

/** @brief Decompress a specially formatted string into text_style arrays.
 * See the document for more info.
 */
std::vector<text_style> make_style(const std::string& input);

/** @brief Apply all inherit flags in the text_style array.
 */
std::vector<text_style>& compile_style(std::vector<text_style>& styles);

/// @brief Convert a text_style to a complete ANSI SGR sequence.
/// @details Uses full-reset approach, e.g., `\033[0;1;31m`.
/// Properties with not_set/no are treated as off (handled by the initial reset).
/// When no attributes are set, returns `\033[0m` (reset only).
std::string style_to_sgr(const text_style& style);

/// @brief Wrap text with ANSI style codes.
inline std::string apply_style(const std::string& input, const text_style &style) {
    return style_to_sgr(style) + input + std::string(reset_color);
}

/** @brief Apply styles to a stringlist, returning a new stringlist with ANSI codes.
 * @param lines The stringlist to apply styles to.
 * @param styles The styles to apply.
 * 
 * This version adds the "reset" code at the end of each block.
 */
std::stringlist apply_style(const std::stringlist& lines, const std::vector<text_style>& styles);

/** @brief Apply compiled styles to a stringlist, returning a single string with ANSI codes.
 * @param lines The stringlist to apply styles to.
 * @param styles The compiled styles to apply.
 * @attention The styles must be compiled using compile_style() before passing to this function.
 * 
 * This version does not add the "reset" code at the end of each block.
 * 
 * The stringlist is joined without any separator. Keep your own separators and new lines in the stringlist.
 */
std::string apply_style_inline(const std::stringlist& lines, const std::vector<text_style>& styles);

/** @brief Outputs Ansi Color Control Pattern (Text (and background)).
 * @param text Color of the Text, in colors::tXXXX
 * @param background Color of the background, in colors::bXXXX
 * 
 * @attention This function does not check the range of the
 * parameters, so make sure you use the right enum value.
 * 
 * To change background color only, use bgcolor() instead
*/
inline std::string textColor(const color &text, const color &background = colors::null) {
	return to_ansi_code(text, background);
}

inline std::wstring wtextColor(const color &text, const color &background = colors::null) {
    return wto_ansi_code(text, background);
}

/** @brief Outputs Ansi Color Control Pattern (Background).
 * @param background Color of the background, in colors::bXXXX
 * 
 * @attention This function does not check the range of the
 * parameters, so make sure you use the right enum value.
*/ 
inline std::string bgcolor(const color &background) {
    return to_ansi_code(colors::null, background);
}

inline std::wstring wbgcolor(const color &background)
{
    return wto_ansi_code(colors::null, background);
}

inline std::string coloredText(std::string content, const color &text, const color &background = colors::null) {
    return textColor(text, background) + content + std::string(reset_color);
}

inline std::string conditionalText(bool enabled, std::string content, const color &text, const color &textDisabled = colors::white, const color &background = colors::null, const color &backgroundDisabled = colors::null)
{
    return enabled ? coloredText(content, text, background) : coloredText(content, textDisabled, backgroundDisabled);
}

/** @brief Generate a clickable hyperlink in the terminal (OSC 8).
 * @param target The target URI.
 * @param display The text to display. Defaults to target if empty.
*/
inline std::string hyperlink(std::string target, std::string display = "")
{
    return std::string("\033]8;;" + target + "\007" + (display.empty() ? target : display) + "\033]8;;\007");
}

/** @brief Generate a clickable file:// hyperlink.
 * @param path The file path.
 * @param display The text to display. Defaults to path if empty.
*/
inline std::string file_link(std::string path, std::string display = "") noexcept
{
    std::string uri = "file:///" + (path.at(0) == '/' ? path.substr(1) : path);
    return std::string("\033]8;;" + uri + "\007" + (display.empty() ? path : display) + "\033]8;;\007");
}

/** @brief Generate a clickable hyperlink with a specified protocol.
 * @param target The target (without protocol prefix).
 * @param display The text to display. Defaults to target if empty.
 * @param protocol The protocol to use, default is "http".
*/
inline std::string web_link(std::string target, std::string display = "", std::string protocol = "http") noexcept
{
    return std::string("\033]8;;" + protocol + "://" + target + "\007" + (display.empty() ? target : display) + "\033]8;;\007");
}

void progress_bar_colored(double progress, int length = 140, const color &color = colors::green);
void progress_bar_texted(double progress, int length = 140);
/**
 * @brief Show a progress bar using ansi colors.
 * @param current the current count
 * @param all the overall count
 */
void progress_bar_colored(int current, int all, int length = 140, const color &color = colors::green);
void progress_bar_texted(int current, int all, int length = 140);

} } // namespace scl2::ansi
