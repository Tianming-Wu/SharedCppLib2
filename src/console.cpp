#include "console.hpp"

namespace Console {

bool setCursorPosition(scl2::Point pos)
{
#ifdef OS_WINDOWS
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) {
        return false;
    }
    COORD cpos = { static_cast<SHORT>(pos.x), static_cast<SHORT>(pos.y) };
    return SetConsoleCursorPosition(hConsole, cpos) != 0;
#else
    return false;
#endif
}

bool setCursorPosition(int row, int col)
{
#ifdef OS_WINDOWS
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) {
        return false;
    }
    COORD pos = { static_cast<SHORT>(col), static_cast<SHORT>(row) };
    return SetConsoleCursorPosition(hConsole, pos) != 0;
#else
    return false;
#endif
}

scl2::Point getCursorPosition()
{
#ifdef OS_WINDOWS
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return scl2::Point(0, 0);
    }
    return scl2::Point(csbi.dwCursorPosition.X, csbi.dwCursorPosition.Y);
#else
    return scl2::Point();
#endif
}

bool showCursor(bool show)
{
#ifdef OS_WINDOWS
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) {
        return false;
    }
    CONSOLE_CURSOR_INFO cursorInfo;
    if (!GetConsoleCursorInfo(hConsole, &cursorInfo)) {
        return false;
    }
    cursorInfo.bVisible = show ? TRUE : FALSE;
    return SetConsoleCursorInfo(hConsole, &cursorInfo) != 0;
#else
    return false;
#endif
}

scl2::Size getConsoleSize()
{
#ifdef OS_WINDOWS
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return scl2::Size(0, 0);
    }
    int width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return scl2::Size(width, height);
#else
    return scl2::Size();
#endif
}

// The output length of the last line
size_t lout::lastLen = 0;

lout::lout()
{
    sstr.clear();
}

lout::~lout()
{
    std::string content = sstr.str();
    sstr.clear();

    if(content.length() < lastLen) {
        // Wipe the remaining characters with spaces to avoid leftover characters from previous output
        std::cout << '\r' << content << std::string(lastLen - content.length(), ' ') << '\r';
    } else {
        std::cout << '\r' << content;
    }

    lastLen = content.length();
}

std::ostream &lout::operator<<(const std::string &str)
{
    return std::cout << str;
}

void lout::finalize()
{
    std::cout << std::endl;
    lastLen = 0; // reset last length
}


};