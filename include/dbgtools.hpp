/*
    This is a header-only debugging tools library, used for testing and debugging this library itself.

    It does not contain production logic, and is not for development debugging.
*/

#pragma once

#include "stringlist.hpp"
#include "ansiio.hpp"

namespace dbgtools {

inline std::string strlist_dbgjoin_colored(const std::stringlist &sl) {
    std::string result; bool st = false;
    for(const std::string& str : sl) {
        result += scl2::textColor(st?scl2::colors::white:scl2::colors::green) + str + std::string(scl2::reset_color);
        st = !st;
    }
    return result;
}

} // namespace dbgtools