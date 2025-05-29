#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

#include "basics.hpp"

namespace std {

// struct __ansiio_streamObject {
//     string content;
// };

struct cursor_position {
	int rows, cols;
};

// template<typename _CharT, typename _Traits>
// 	inline basic_ostream<_CharT, _Traits>&
// 		operator<<(basic_ostream<_CharT, _Traits>& __os, __ansiio_streamObject __c)
// 		{
// 			__os << __c.content;
// 			return __os;
// 		}

// template<typename _CharT, typename _Traits>
// 	inline basic_ostringstream<_CharT, _Traits>&
// 		operator<<(basic_ostringstream<_CharT, _Traits>& __os, __ansiio_streamObject __c)
// 		{
// 			__os << __c.content;
// 			return __os;
// 		}

// template<typename _CharT, typename _Traits>
// 	inline basic_stringstream<_CharT, _Traits>&
// 		operator<<(basic_stringstream<_CharT, _Traits>& __os, __ansiio_streamObject __c)
// 		{
// 			__os << __c.content;
// 			return __os;
// 		}

// inline void ansiio(__ansiio_streamObject o) {
// 	std::cout << o;
// }


/// @brief Predefined Color Patterns.
/// @attention t means Text, and b means Background. Do not mix them!
enum colors {
	tBlack = 30, tRed, tGreen, tYellow, tBlue, tPurple, tCyan, tWhite,
	bBlack = 40, bRed, bGreen, bYellow, bBlue, bPurple, bCyan, bWhite,
	tLightBlack = 90, tLightRed, tLightGreen, tLightYellow, tLightBlue, tLightPurple, tLightCyan, tLightWhite,
};
// 黑、红、绿、黄、蓝、洋红、青、白

/** @brief Outputs Ansi Color Control Pattern (Text (and background)).
 * @param text Color of the Text, in colors::tXXXX
 * @param background Color of the background, in colors::bXXXX
 * 
 * @attention This function does not check the range of the
 * parameters, so make sure you use the right enum value.
 * 
 * To change background color only, use bgcolor() instead
*/
inline string color(int text, int background = 0)
{
	string _text = itos(text), _background = background?(itos(background)):"";
	return string("\033[1;" + _text + (background?";":"") + _background + "m");
}

/** @brief Outputs Ansi Color Control Pattern (Background).
 * @param background Color of the background, in colors::bXXXX
 * 
 * @attention This function does not check the range of the
 * parameters, so make sure you use the right enum value.
*/ 
inline string bgcolor(int background)
{
	return string("\033[1;" + itos(background) + "m");
}

inline string coloredtext(string content, int text, int background = 0)
{
	string _text = itos(text), _background = background?(itos(background)):"";
	return string("\033[1;" + _text + (background?";":"") + _background + "m" + content + "\033[0m");
}

inline string bcoloredtext(bool enabled, string content, int text, int textDisabled = tWhite, int background = 0, int backgroundDisabled = 0)
{
	string _text = enabled?itos(text):itos(textDisabled),
		   _background = enabled?(background?(itos(background)):""):(backgroundDisabled?(itos(backgroundDisabled)):"");
	return string("\033[1;" + _text + (background?";":"") + _background + "m" + content + "\033[0m");
}

/// @brief The Reset Ansi Color Control Pattern.
const string clearcolor { "\033[0m" };


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


void progress_bar_colored(double progress, int length = 140, int color = colors::tGreen) {
    int pos = static_cast<int>(length * progress); // 计算进度条的填充长度
    std::cout << std::color(color); // 填充绿色
    for (int i = 0; i < pos; ++i) std::cout << " ";
	std::cout << std::clearcolor; // 清除颜色
	for (int i = pos; i < length; ++i) std::cout << " ";
    // 输出右侧百分比
    std::cout << "] " << std::fixed << std::setprecision(1) << (progress * 100) << "%";
    std::flush(std::cout); // 刷新输出
}

void progress_bar_texted(double progress, int length = 140) {
    int pos = static_cast<int>(length * progress); // 计算进度条的填充长度
    std::cout << "\r[";
    for (int i = 0; i < pos; ++i) std::cout << "=";
	for (int i = pos; i < length; ++i) std::cout << " ";
    // 输出右侧百分比
    std::cout << "] " << std::fixed << std::setprecision(1) << (progress * 100) << "%";
    std::flush(std::cout); // 刷新输出
}

/**
 * @brief Show a progress bar using ansi colors.
 * @param current the current count
 * @param all the overall count
 */
void progress_bar_colored(int current, int all, int length = 140, int color = colors::tGreen) {
    double progress = static_cast<double>(current) / all;
    int pos = static_cast<int>(length * progress); // 计算进度条的填充长度
    std::cout << std::color(color); // 填充绿色
    for (int i = 0; i < pos; ++i) std::cout << " ";
	std::cout << std::clearcolor; // 清除颜色
	for (int i = pos; i < length; ++i) std::cout << " ";
    // 输出右侧百分比
    std::cout << "] " << current << "/" << all << " " << std::fixed << std::setprecision(1) << (progress * 100) << "%";
    std::flush(std::cout); // 刷新输出
}

void progress_bar_texted(int current, int all, int length = 140) {
    double progress = static_cast<double>(current) / all;
    int pos = static_cast<int>(length * progress); // 计算进度条的填充长度
    std::cout << "\r[";
    for (int i = 0; i < pos; ++i) std::cout << "=";
	for (int i = pos; i < length; ++i) std::cout << " ";
    // 输出右侧百分比
    std::cout << "] " << current << "/" << all << " " << std::fixed << std::setprecision(1) << (progress * 100) << "%";
    std::flush(std::cout); // 刷新输出
}

/** @brief Move the cursor to a specified position.
 * @param row The row number to move to (1-based).
 * @param col The column number to move to (1-based).
 * 
 * This function uses ANSI escape codes to move the cursor
 * to the specified position in the terminal.
 */
inline string movecursor(int row, int col)
{
    return string("\033[" + itos(row) + ";" + itos(col) + "H");
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
 */
inline cursor_position get_cursor_position() {
    std::cout << "\033[6n";

    std::string response;
    std::getline(std::cin, response);

    int rows, cols;
    sscanf(response.c_str(), "\033[%d;%dR", &rows, &cols);

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
