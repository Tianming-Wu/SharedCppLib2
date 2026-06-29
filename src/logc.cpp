#include "logc.hpp"
#include <string>


namespace logc {

const color_standard clrstd_buildin {
    .debug = color(144, 238, 144),
    .info = color(255, 255, 255),
    .warn = color(255, 165, 0),
    .error = color(255, 99, 71),
    .fatal = color(178, 34, 34)
};

const color_standard clrstd_vscode {
    .debug = color(106, 185, 112),
    .info = color(255, 255, 255),
    .warn = color(255, 203, 107),
    .error = color(255, 107, 107),
    .fatal = color(204, 62, 68)
};


color_standard clrstd = clrstd_buildin;

bool logPreprocessor(logt_message& message) {
    std::string colortag = scl2::to_ansi_code(clrstd.get(message.level));
    message.content = colortag + message.content + std::string(scl2::reset_color);

    return true;
}

void setColorStandard(const color_standard &s) {
    clrstd = s;
}

} // namespace logc