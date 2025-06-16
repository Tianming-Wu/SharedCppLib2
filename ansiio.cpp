#include "ansiio.hpp"

namespace std
{

void progress_bar_colored(double progress, int length, int color) {
    int pos = static_cast<int>(length * progress); // 计算进度条的填充长度
    std::cout << std::color(color); // 填充绿色
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
    std::cout << std::color(color); // 填充绿色
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
