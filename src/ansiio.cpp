#include "ansiio.hpp"
#include <unordered_map>

namespace std
{

string colorctl::to_ansi_code() const {
    switch(type) {
        case cctlt_console:
            return "\033[1;" + std::to_string(v1) + "m";
        case cctlt_rgba:
            return "\033[38;2;" + std::to_string(v1) + ";" 
                    + std::to_string(v2) + ";" + std::to_string(v3) + "m";
        case cctlt_builtin:
            // 将内置颜色映射到具体 ANSI 值
            return map_builtin_to_ansi();
        default:
            return "\033[0m";
    }
}

std::string colorctl::map_builtin_to_ansi() const {
    static const std::unordered_map<int, int> builtin_map = {
        {biWhite, tWhite}, {biRed, tRed}, {biOrange, tOrange}, // 208 是橙色
        {biYellow, tYellow}, {biGreen, tGreen}, {biCyan, tCyan},
        {biBlue, tBlue}, {biPurple, tPurple}, {biBlack, tBlack}
    };
    auto it = builtin_map.find(v1);
    return it != builtin_map.end() ? 
        "\033[1;" + std::to_string(it->second) + "m" : "\033[0m";
}

std::vector<colorctl> decompress(const std::string& input) {
    // 语法：
    // <t><l>[d, r, g, y, b, p, c, w]%d<b>...
    // text light [ black(dark) red green yellow blue purple cyan white ] number background

    std::vector<colorctl> result;
    for(size_t i = 0; i < input.size();) {
        char c = input[i];
        bool isbg = false, islight = false;
        if(c == 't') c = input[++i];
        else if(c == 'b') { isbg = true; c = input[++i]; }
        if(c == 'l') { islight = true; c = input[++i]; }

        colorctl cc;

        switch(c) {
        case 'd': cc = colorctl(isbg?bBlack:(islight?tLightBlack:tBlack)); break;
        case 'r': cc = colorctl(isbg?bRed:(islight?tLightRed:tRed)); break;
        case 'g': cc = colorctl(isbg?bGreen:(islight?tLightGreen:tGreen)); break;
        case 'y': cc = colorctl(isbg?bYellow:(islight?tLightYellow:tYellow)); break;
        case 'b': cc = colorctl(isbg?bBlue:(islight?tLightBlue:tBlue)); break;
        case 'p': cc = colorctl(isbg?bPurple:(islight?tLightPurple:tPurple)); break;
        case 'c': cc = colorctl(isbg?bCyan:(islight?tLightCyan:tCyan)); break;
        case 'w':
        default : cc = colorctl(isbg?bWhite:(islight?tLightWhite:tWhite)); break;
        }

        int rep = [&] {
            int res = 0;

            char ci = input[++i];
            while(!(ci >= '0' && ci <= '9')) {
                res = res * 10 + (ci - '0');
                ci = input[++i];
            }
            
            return res?res:1;
        }();

        for(int j = 0; j < rep; j++) result.push_back(cc);
    }

    return result;
}

void progress_bar_colored(double progress, int length, int color) {
    int pos = static_cast<int>(length * progress); // 计算进度条的填充长度
    std::cout << std::textcolor(color); // 填充绿色
    for (int i = 0; i < pos; ++i) std::cout << " ";
	std::cout << std::clearcolor; // 清除颜色
	for (int i = pos; i < length; ++i) std::cout << " ";
    // 输出右侧百分比
    std::cout << "] " << std::fixed << std::setprecision(1) << (progress * 100) << "%";
    std::flush(std::cout); // 刷新输出
}

void progress_bar_texted(double progress, int length) {
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
void progress_bar_colored(int current, int all, int length, int color) {
    double progress = static_cast<double>(current) / all;
    int pos = static_cast<int>(length * progress); // 计算进度条的填充长度
    std::cout << std::textcolor(color); // 填充绿色
    for (int i = 0; i < pos; ++i) std::cout << " ";
	std::cout << std::clearcolor; // 清除颜色
	for (int i = pos; i < length; ++i) std::cout << " ";
    // 输出右侧百分比
    std::cout << "] " << current << "/" << all << " " << std::fixed << std::setprecision(1) << (progress * 100) << "%";
    std::flush(std::cout); // 刷新输出
}

void progress_bar_texted(int current, int all, int length) {
    double progress = static_cast<double>(current) / all;
    int pos = static_cast<int>(length * progress); // 计算进度条的填充长度
    std::cout << "\r[";
    for (int i = 0; i < pos; ++i) std::cout << "=";
	for (int i = pos; i < length; ++i) std::cout << " ";
    // 输出右侧百分比
    std::cout << "] " << current << "/" << all << " " << std::fixed << std::setprecision(1) << (progress * 100) << "%";
    std::flush(std::cout); // 刷新输出
}

} // namespace std
