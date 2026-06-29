#include "ansiio.hpp"
#include <unordered_map>

namespace scl2::ansi {


int map_system_color(const color& c, bool is_background) {
    if (!c.is_system()) throw std::invalid_argument("Color is not a system color.");
    system_color sys_color = static_cast<system_color>(c.v1);
    switch (sys_color) {
        case system_color::Black:   return is_background ? 40 : 30;
        case system_color::Red:     return is_background ? 41 : 31;
        case system_color::Green:   return is_background ? 42 : 32;
        case system_color::Yellow:  return is_background ? 43 : 33;
        case system_color::Blue:    return is_background ? 44 : 34;
        case system_color::Purple:  return is_background ? 45 : 35;
        case system_color::Cyan:    return is_background ? 46 : 36;
        case system_color::White:   return is_background ? 47 : 37;
        case system_color::LightBlack:   return is_background ? 100 : 90;
        case system_color::LightRed:     return is_background ? 101 : 91;
        case system_color::LightGreen:   return is_background ? 102 : 92;
        case system_color::LightYellow:  return is_background ? 103 : 93;
        case system_color::LightBlue:    return is_background ? 104 : 94;
        case system_color::LightPurple:  return is_background ? 105 : 95;
        case system_color::LightCyan:    return is_background ? 106 : 96;
        case system_color::LightWhite:   return is_background ? 107 : 97;
        default: throw std::invalid_argument("Unknown system color.");
    }
}

std::string to_rgb_code(const color& c, bool is_background) {
    if (!c.is_rgb()) throw std::invalid_argument("Color is not an RGB color.");
    return (is_background ? "48;2;" : "38;2;")
        + std::to_string(c.v1) + ";" + std::to_string(c.v2) + ";" + std::to_string(c.v3);
}

std::string to_ansi_code(const color& c, const color& b) {
    std::string result = "\033[";

    // If foreground color exists
    if (c) {
        switch(c.type) {
            case ColorType::RGB:
                result += to_rgb_code(c, false); break;
            case ColorType::Builtin:
            case ColorType::RGBA:
            case ColorType::CMYK:
                result += to_rgb_code(c.to_rgb(), false); break;
            case ColorType::System:
                result += std::to_string(map_system_color(c, false));
                break;
            case ColorType::Style:
                throw std::invalid_argument("Style colors cannot be converted to ANSI codes.");
        }
    }

    if (b) {
        if (c) result += ";"; // Add a separator if both colors are present
        switch(b.type) {
            case ColorType::RGB:
                result += to_rgb_code(b, true); break;
            case ColorType::Builtin:
            case ColorType::RGBA:
            case ColorType::CMYK:
                result += to_rgb_code(b.to_rgb(), true); break;
            case ColorType::System:
                result += std::to_string(map_system_color(b, true));
                break;
            case ColorType::Style:
                throw std::invalid_argument("Style colors cannot be converted to ANSI codes.");
        }
    }

    return result + "m";
}

// helper function to parse a color code from a string
color readcolor(const std::string& input, size_t &i, bool &is_inherit) {
    // take the color code until the next ',' or ';' or end of string
    std::string color_code;
    while(i < input.size() && input[i] != ',' && input[i] != ';') {
        color_code += input[i++];
    }

    // parse it into color
    if (color_code.empty() || color_code == "-") {
        is_inherit = false;
        return colors::null; // null color
    } else if (color_code == "<") {
        is_inherit = true;
        return colors::null; // inherit color
    } else if (color_code[0] == '#') {
        // RGB color
        if (color_code.size() != 7) throw std::invalid_argument("Invalid RGB color code.");
        int r = std::stoi(color_code.substr(1, 2), nullptr, 16);
        int g = std::stoi(color_code.substr(3, 2), nullptr, 16);
        int b = std::stoi(color_code.substr(5, 2), nullptr, 16);
        is_inherit = false;
        return color(r, g, b);
    } else {
        // System color
        static const std::unordered_map<std::string, system_color> sys_color_map = {
            {"b", system_color::Black}, {"r", system_color::Red}, {"g", system_color::Green},
            {"y", system_color::Yellow}, {"bl", system_color::Blue}, {"p", system_color::Purple},
            {"c", system_color::Cyan}, {"w", system_color::White},
            {"B", system_color::LightBlack}, {"R", system_color::LightRed}, {"G", system_color::LightGreen},
            {"Y", system_color::LightYellow}, {"BL", system_color::LightBlue}, {"P", system_color::LightPurple},
            {"C", system_color::LightCyan}, {"W", system_color::LightWhite}
        };

        auto it = sys_color_map.find(color_code);
        if (it == sys_color_map.end()) throw std::invalid_argument("Unknown color code: " + color_code);
        is_inherit = false;
        return color(it->second);
    }
}

std::vector<text_style> make_style(const std::string& input) {
    /*
        Grammar info:
        The style blocks are separated by `;`.
        Inside each block, there can be multiple attributes, seperated by `,`.

        The attributes can be:
        - `t:`: text color, followed by color text (see below)
        - `b:`: background color, followed by color text (see below)
        - `b`: bold
        - `i`: italic
        - `u`: underline
            Bold, italic and underline can be combined.
            They can also be negated by prefixing with `!`, e.g., `!b` means not bold.
    
        For color text, 
        - `-` or empty: null color (reset)
        - System colors:
            - `b`: black
            - `r`: red
            - `g`: green
            - `y`: yellow
            - `bl`: blue
            - `p`: purple
            - `c`: cyan
            - `w`: white
            - Light colors are Uppercase, e.g., `B` for light black, `R` for light red, etc.
        - RGB colors:
            - `#RRGGBB` format, e.g., `#FF0000` for red, `#00FF00` for green, etc.
        - `<`: inherit from previous block.

    */

    std::vector<text_style> result;

    // If the input is empty, return a vector with a single default text_style
    if (input.empty()) return { text_style() };

    size_t i = 0;
    text_style ts;

    bool is_opposite = false, is_first_token = true;

    while(i < input.size()) {
        switch(input[i]) {
            case '!': is_first_token = false; is_opposite   = true; break;
            case 'i': is_first_token = false; ts.italic     = is_opposite ? tri_state::no : tri_state::yes; break;
            case 'u': is_first_token = false; ts.underline  = is_opposite ? tri_state::no : tri_state::yes; break;
            case 'b': {
                is_first_token = false;
                if (i + 1 >= input.size() || input[i + 1] != ':') {
                    ts.bold = is_opposite ? tri_state::no : tri_state::yes;
                    break;
                }
                
                if (is_opposite) throw std::invalid_argument("Cannot set opposite for background color.");
                i += 2; // Skip 'b:'
                ts.background_color = readcolor(input, i, ts.background_color_inherit);
                break;
            }
            case 't': {
                is_first_token = false;
                if (is_opposite) throw std::invalid_argument("Cannot set opposite for text color.");
                i += 2; // Skip 't:'
                ts.text_color = readcolor(input, i, ts.text_color_inherit);
                break;
            }
            case ' ':
                // Ignore spaces
                break;
            case '<':
                if (!is_first_token) throw std::invalid_argument("Inherit '<' can only be used as the first token in a block.");
                is_first_token = false;
                ts.is_inherit = true;
                ts.text_color_inherit = true;
                ts.background_color_inherit = true;
                break;
            case ';':
                result.push_back(ts);
                ts = text_style(); // Reset for the next block
                is_opposite = false; // Reset opposite flag
                is_first_token = true;
                break;
            case ',':
                is_opposite = false; // Reset opposite flag
                is_first_token = false;
                break;
            default:
                throw std::invalid_argument(std::string("Unknown attribute: ") + input[i]);
        }

        i++;
    }

    result.push_back(ts); // Push the last text_style
    return result;
}

std::vector<text_style> &compile_style(std::vector<text_style> &styles)
{
    // treat the first style
    styles[0].is_inherit = false;
    styles[0].text_color_inherit = false;
    styles[0].background_color_inherit = false;
    styles[0].bold = (styles[0].bold == tri_state::not_set) ? tri_state::no : styles[0].bold;
    styles[0].italic = (styles[0].italic == tri_state::not_set) ? tri_state::no : styles[0].italic;
    styles[0].underline = (styles[0].underline == tri_state::not_set) ? tri_state::no : styles[0].underline;

    for (size_t i = 1; i < styles.size(); ++i) {
        if (styles[i].is_inherit) {
            if (styles[i].bold == tri_state::not_set) styles[i].bold = styles[i - 1].bold;
            if (styles[i].italic == tri_state::not_set) styles[i].italic = styles[i - 1].italic;
            if (styles[i].underline == tri_state::not_set) styles[i].underline = styles[i - 1].underline;
        } else {
            if (styles[i].bold == tri_state::not_set) styles[i].bold = tri_state::no;
            if (styles[i].italic == tri_state::not_set) styles[i].italic = tri_state::no;
            if (styles[i].underline == tri_state::not_set) styles[i].underline = tri_state::no;
        }

        if (styles[i].text_color_inherit) {
            styles[i].text_color = styles[i - 1].text_color;
            styles[i].text_color_inherit = false;
        }

        if (styles[i].background_color_inherit) {
            styles[i].background_color = styles[i - 1].background_color;
            styles[i].background_color_inherit = false;
        }
    }
    return styles;
}

std::string style_to_sgr(const text_style& style)
{
    // Full-reset approach: start with "0", then set desired attributes.
    // "0" handles all not_set/no properties implicitly.
    std::string sgr = "0";

    auto add = [&](const std::string& p) {
        sgr += ";" + p;
    };

    if (style.bold == tri_state::yes) add("1");
    if (style.italic == tri_state::yes) add("3");
    if (style.underline == tri_state::yes) add("4");

    // Text color
    if (style.text_color) {
        switch(style.text_color.type) {
            case ColorType::RGB:
                add(to_rgb_code(style.text_color, false)); break;
            case ColorType::Builtin:
            case ColorType::RGBA:
            case ColorType::CMYK:
                add(to_rgb_code(style.text_color.to_rgb(), false)); break;
            case ColorType::System:
                add(std::to_string(map_system_color(style.text_color, false))); break;
            case ColorType::Style:
                throw std::invalid_argument("Style colors cannot be converted to ANSI codes.");
        }
    }

    // Background color
    if (style.background_color) {
        switch(style.background_color.type) {
            case ColorType::RGB:
                add(to_rgb_code(style.background_color, true)); break;
            case ColorType::Builtin:
            case ColorType::RGBA:
            case ColorType::CMYK:
                add(to_rgb_code(style.background_color.to_rgb(), true)); break;
            case ColorType::System:
                add(std::to_string(map_system_color(style.background_color, true))); break;
            case ColorType::Style:
                throw std::invalid_argument("Style colors cannot be converted to ANSI codes.");
        }
    }

    return "\033[" + sgr + "m";
}

std::stringlist apply_style(const std::stringlist &lines, const std::vector<text_style> &styles)
{
    if (styles.empty()) return lines;

    std::stringlist result;
    result.reserve(lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& style = styles[i % styles.size()];
        result.push_back(style_to_sgr(style) + lines[i] + std::string(reset_color));
    }
    return result;
}

std::string apply_style_inline(const std::stringlist &lines, const std::vector<text_style> &styles)
{
    if (styles.empty()) return lines.join();

    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& style = styles[i % styles.size()];
        result += style_to_sgr(style) + lines[i];
    }
    result += std::string(reset_color);
    return result;
}

void progress_bar_colored(double progress, int length, const color& color) {
    int pos = static_cast<int>(length * progress); // 计算进度条的填充长度
    std::cout << textColor(color); // 填充绿色
    for (int i = 0; i < pos; ++i) std::cout << " ";
	std::cout << reset_color; // 清除颜色
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
void progress_bar_colored(int current, int all, int length, const color& color) {
    double progress = static_cast<double>(current) / all;
    int pos = static_cast<int>(length * progress); // 计算进度条的填充长度
    std::cout << textColor(color); // 填充绿色
    for (int i = 0; i < pos; ++i) std::cout << " ";
	std::cout << reset_color; // 清除颜色
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

} // namespace scl2::ansi
