#pragma once

#include "logt.hpp"
#include "ansiio.hpp"

using std::colorctl;

namespace logc {

struct color_standard {
    colorctl debug, info, warn, error, fatal;
    inline colorctl get(LogLevel level) {
        switch(level) {
            case LogLevel::DEBUG: return debug;
            case LogLevel::INFO: return info;
            case LogLevel::WARN: return warn;
            case LogLevel::ERROR: return error;
            case LogLevel::FATAL: return fatal;

            case LogLevel::QUIET: // invalid
            default: return colorctl();
        }
    }
};

extern const color_standard clrstd_buildin;
extern const color_standard clrstd_vscode;

bool logPreprocessor(logt_message& message);
void setColorStandard(const color_standard &s);

} // namespace logc