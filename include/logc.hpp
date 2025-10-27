#pragma once

#include "logt.hpp"
#include "ansiio.hpp"

using std::colorctl;

namespace logc {

struct color_standard {
    colorctl debug, info, warn, error, fatal;
    inline colorctl get(LogLevel level) {
        switch(level) {
            case LogLevel::l_DEBUG: return debug;
            case LogLevel::l_INFO: return info;
            case LogLevel::l_WARN: return warn;
            case LogLevel::l_ERROR: return error;
            case LogLevel::l_FATAL: return fatal;

            case LogLevel::l_QUIET: // invalid
            default: return colorctl();
        }
    }
};

extern const color_standard clrstd_buildin;
extern const color_standard clrstd_vscode;

bool logPreprocessor(logt_message& message);
void setColorStandard(const color_standard &s);

} // namespace logc