#include "logc.hpp"
#include <string>


namespace logc {

const color_standard clrstd_buildin {
    .debug = colorctl(144, 238, 144),
    .info = colorctl(255, 255, 255),
    .warn = colorctl(255, 165, 0),
    .error = colorctl(255, 99, 71),
    .fatal = colorctl(178, 34, 34)
};

const color_standard clrstd_vscode {
    .debug = colorctl(106, 185, 112),
    .info = colorctl(255, 255, 255),
    .warn = colorctl(255, 203, 107),
    .error = colorctl(255, 107, 107),
    .fatal = colorctl(204, 62, 68)
};


color_standard clrstd = clrstd_buildin;

bool logPreprocessor(logt_message& message) {
    std::string colortag = clrstd.get(message.level).to_ansi_code();
    message.content = colortag + message.content + std::clearcolor;

    return true;
}

void setColorStandard(const color_standard &s) {
    clrstd = s;
}

} // namespace logc