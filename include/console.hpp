/*
    Console control module for SharedCppLib2.

    This module is here because ansiio's cursor control functions are
    just not working. They are now marked as deprecated.

    These, however, use platform-specific methods. They are typically faster
    than ansiio features, and is actually more compatible on supported platforms.

    Like on Windows 7, when ansi features are not fully supported, these
    functions can still work.

    It's suggested to use:
        namespace c = Console;
    to simplify the writing. But I would not include it in the file, since it's
    dangerous to have such short naming.

*/

#pragma once

#include "ansiio.hpp"
#include "platform.hpp"

#include "basics.hpp"

#include "scltypes.hpp"

namespace Console {

// Cursor related

bool setCursorPosition(scl2::Point pos);
bool setCursorPosition(int row, int col);

scl2::Point getCursorPosition();

bool showCursor(bool show = true);
inline bool hideCursor() { return showCursor(false); }


// Color related
// These are sometimes faster on some platforms.

// Console related
scl2::Size getConsoleSize();




// Easy output-to-pos.
// This one works like `std::cout << posout(10, 20) << "Hello World!" << std::endl;`
class posout {
public:
    posout(int row, int col) : pos(row, col) {}
    posout(scl2::Point pos) : pos(pos) {}

    friend std::ostream& operator<<(std::ostream& os, const posout& po) {
        Console::setCursorPosition(po.pos);
        return os;
    }

private:
    scl2::Point pos;
};

// Easy output-to-pos.
// This one works like `pout(10, 20) << "Hello World!" << std::endl;`
class pout {
public:
    inline pout(int row, int col) { setCursorPosition(row, col); }
    inline pout(scl2::Point pos) { setCursorPosition(pos); }

    template <typename _Any>
    std::ostream& operator<<(const _Any& val) {
        return std::cout << val;
    }
};


// Output to a single line
// actually is using `\r` to return to the beginning.

// Usage example:
/*
    for (int i = 0; i < 100; i++) {
        Console::lout() << "Progress: " << i << "%";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    Console::lout::finalize();



*/

class lout {
public:
    lout();
    ~lout();

    disable_copy_move(lout) // Does not allow user to keep the instance.

    template <typename _Any>
    requires requires (const _Any &a) { std::to_string(a); }
    std::ostream& operator<<(const _Any &val) {
        return this->operator<<(std::to_string(val));
    }

    std::ostream& operator<<(const std::string& str);

    static void finalize();

private:
    static size_t lastLen;
    std::stringstream sstr;
};



} // namespace Console