#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <cstdint>
#include <vector>

#include "basics.hpp"

namespace std {

// struct __ansiio_streamObject {
//     string content;
// };

struct cursor_position {
	int rows, cols;
};

/// @brief Predefined Color Patterns.
/// @attention t means Text, and b means Background. Do not misuse!
enum colors {
	tBlack = 30, tRed, tGreen, tYellow, tBlue, tPurple, tCyan, tWhite, tOrange = 208,
	bBlack = 40, bRed, bGreen, bYellow, bBlue, bPurple, bCyan, bWhite,
	tLightBlack = 90, tLightRed, tLightGreen, tLightYellow, tLightBlue, tLightPurple, tLightCyan, tLightWhite,
    nullColor = 0
};
// 黑、红、绿、黄、蓝、洋红、青、白

enum builtin_colors {
    biWhite, biRed, biOrange, biYellow, biGreen, biCyan, biBlue, biPurple, biBlack
};

class colorctl {
public:
    enum cctltype {cctlt_builtin,cctlt_console,cctlt_rgba,cctlt_cmyk};
    uint8_t type, v1, v2, v3, v4;

    inline colorctl(): type(cctlt_builtin), v1(builtin_colors::biWhite) {}
    inline colorctl(colors cid): type(cctlt_console),v1(cid) {}
    inline colorctl(builtin_colors bcid): type(cctlt_builtin), v1(bcid) {}
    inline colorctl(int r, int g, int b, int a = 255): type(cctlt_rgba),v1(r),v2(g),v3(b),v4(a) {}
    inline colorctl cmyk(int c, int m, int y, int k) {
        ///TODO: Complete convertion.
    }

    std::string to_ansi_code() const;
private:
    std::string map_builtin_to_ansi() const;
};

/** @brief Decompress a specially formatted string into colorinfo arrays.
 * See the document for more info.
 */
std::vector<colorctl> decompress(const std::string& input);

/** @brief Outputs Ansi Color Control Pattern (Text (and background)).
 * @param text Color of the Text, in colors::tXXXX
 * @param background Color of the background, in colors::bXXXX
 * 
 * @attention This function does not check the range of the
 * parameters, so make sure you use the right enum value.
 * 
 * To change background color only, use bgcolor() instead
*/
inline string textcolor(int text, int background = 0)
{
	string _text = std::to_string(text), _background = background?(std::to_string(background)):"";
	return string("\033[1;" + _text + (background?";":"") + _background + "m");
}

inline string rgbtext(int r, int g, int b)
{
    return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
}

inline wstring wtextcolor(int text, int background = 0)
{
	wstring _text = std::to_wstring(text), _background = background?(std::to_wstring(background)):L"";
	return wstring(L"\033[1;" + _text + (_background.empty()?L"":L";" + _background) + L"m");
}

/** @brief Outputs Ansi Color Control Pattern (Background).
 * @param background Color of the background, in colors::bXXXX
 * 
 * @attention This function does not check the range of the
 * parameters, so make sure you use the right enum value.
*/ 
inline string bgcolor(int background)
{
	return string("\033[1;" + std::to_string(background) + "m");
}

inline string rgbbg(int r, int g, int b)
{
    return "\033[48;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
}

inline wstring wbgcolor(int background)
{
    return wstring(L"\033[1;" + std::to_wstring(background) + L"m");
}

inline string coloredtext(string content, int text, int background = 0)
{
	string _text = std::to_string(text), _background = background?(std::to_string(background)):"";
	return string("\033[1;" + _text + (background?";":"") + _background + "m" + content + "\033[0m");
}

inline string bcoloredtext(bool enabled, string content, int text, int textDisabled = tWhite, int background = 0, int backgroundDisabled = 0)
{
	string _text = enabled?std::to_string(text):std::to_string(textDisabled),
		   _background = enabled?(background?(std::to_string(background)):""):(backgroundDisabled?(std::to_string(backgroundDisabled)):"");
	return string("\033[1;" + _text + (background?";":"") + _background + "m" + content + "\033[0m");
}

/// @brief The Reset Ansi Color Control Pattern.
const string clearcolor { "\033[0m" };

const wstring wclearcolor { L"\033[0m" };

/** @brief Generate a clickable URI.
 * @param content The actual URI to be executed.
 * @param display The text to display.
 * If not set, then uses the same as content by default.
 * @attention The function does not add anything to make
 * sure the availability, check by youself or use furi()
 * or wuri().
*/
inline string uri(string content, string display = "")
{
    return string("\033]8;;" + content + "\007" + (display.empty()?content:display) + "\033]8;;\007");
}

/** @brief Generate a clickable URI with file:// prefix.
 * @param content The actual URI to be executed.
 * @param tip The text to display.
 * If not set, then uses the same as content by default.
*/
inline string furi(string content, string tip = "") noexcept
{
    return string("\033]8;;" + string("file:///" + (content.at(0)=='/'?content.substr(1):content)) + "\007" + content + "\033]8;;\007");
}

/** @brief Generate a clickable URI with a specified protocol.
 * @param content The actual URI to be executed.
 * @param tip The text to display.
 * @param protocol The protocol to use, default is "http".
 * If tip is not set, then uses the same as content by default.
*/
inline string wuri(string content, string tip = "", string protocol = "http") noexcept
{
    return string("\033]8;;" + protocol + "://" + content + "\007" + (tip.empty() ? content : tip) + "\033]8;;\007");
}

void progress_bar_colored(double progress, int length = 140, int color = colors::tGreen);
void progress_bar_texted(double progress, int length = 140);
/**
 * @brief Show a progress bar using ansi colors.
 * @param current the current count
 * @param all the overall count
 */
void progress_bar_colored(int current, int all, int length = 140, int color = colors::tGreen);
void progress_bar_texted(int current, int all, int length = 140);

/** @brief Move the cursor to a specified position.
 * @param row The row number to move to (1-based).
 * @param col The column number to move to (1-based).
 * 
 * This function uses ANSI escape codes to move the cursor
 * to the specified position in the terminal.
 */
inline string movecursor(int row, int col)
{
    return string("\033[" + std::to_string(row) + ";" + std::to_string(col) + "H");
}

/// @brief Hide the cursor.
const string hidecursor { "\033[?25l" };

/// @brief Show the cursor.
const string showcursor { "\033[?25h" };

/// @brief Save the current cursor position.
const string savecursorpos { "\033[s" };

/// @brief Restore the cursor position saved by savecursorpos
const string restorecursorpos { "\033[u" };


/** @brief Get the current cursor position (rows and columns).
 * 
 * This function uses ANSI escape codes to get the current cursor
 * position and returns a struct containing the number of rows and columns.
 * 
 * @warning uses sscanf which is unsafe. Will be fixed in a newer release.
 */
inline cursor_position get_cursor_position() {
    std::cout << "\033[6n";

    std::string response;
    std::getline(std::cin, response);

    int rows, cols;
    // sscanf(response.c_str(), "\033[%d;%dR", &rows, &cols);
    sscanf_s(response.c_str(), "\033[%d;%dR", &rows, &cols);

    return cursor_position {.rows = rows, .cols = cols};
}

/** @brief Get the terminal size (rows and columns).
 * 
 * This function uses ANSI escape codes to get the terminal
 * size and returns a struct containing the number of rows and columns.
 */
inline cursor_position get_terminal_size() {
    std::cout << savecursorpos;
    movecursor(999,999);
    cursor_position pos = get_cursor_position();
    std::cout << restorecursorpos;
    return pos;
}

};
