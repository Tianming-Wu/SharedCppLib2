#pragma once

#include "logt.hpp"
#include "ansiio.hpp"
#include "color.hpp"

namespace logc {

using scl2::color;

struct color_standard {
    color debug, info, warn, error, fatal;
    inline color get(LogLevel level) {
        switch(level) {
            case LogLevel::Debug: return debug;
            case LogLevel::Info: return info;
            case LogLevel::Warn: return warn;
            case LogLevel::Error: return error;
            case LogLevel::Fatal: return fatal;

            case LogLevel::Quiet: // invalid
            default: return color();
        }
    }
};

extern const color_standard clrstd_buildin;
extern const color_standard clrstd_vscode;

bool logPreprocessor(logt_message& message);
void setColorStandard(const color_standard &s);

} // namespace logc