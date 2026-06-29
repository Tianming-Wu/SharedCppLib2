# ansiio - ANSI 终端输出与格式化

+ 名称: ansiio
+ 命名空间: `scl2::ansi` (inline)
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `basic` (包含 ansiio) |

包含方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::basic)
```

```cpp
#include <SharedCppLib2/ansiio.hpp>
```

## 描述

`ansiio` 提供用于终端输出的 ANSI 转义序列生成功能。支持：

- **颜色**：系统颜色（16 种标准色）、RGB（24 位真彩色）和命名颜色
- **文本样式**：粗体、斜体、下划线，支持继承
- **超链接**：在支持的终端中生成可点击的 OSC 8 超链接
- **进度条**：彩色和纯文本进度指示器
- **样式系统**：将样式字符串解析为结构化的 `text_style` 对象，解析继承关系，并渲染为 ANSI 码

所有 ANSI 函数位于 `scl2::ansi` 内联命名空间中，可直接通过 `scl2::function_name()` 访问。

## 快速开始

### 基本颜色输出

```cpp
#include <SharedCppLib2/ansiio.hpp>
#include <SharedCppLib2/named_colors.hpp>

// 简单文字颜色
std::cout << scl2::textColor(scl2::colors::red) << "红色文字" << scl2::reset_color << std::endl;

// RGB 颜色
std::cout << scl2::textColor(scl2::color(255, 165, 0)) << "橙色文字" << scl2::reset_color << std::endl;

// 文字 + 背景
std::cout << scl2::to_ansi_code(scl2::colors::red, scl2::colors::white)
          << "白底红字" << scl2::reset_color << std::endl;
```

### 样式系统

```cpp
// 解析样式字符串
auto styles = scl2::make_style("t:r,bold;  t:g,italic;  <,b:bl");
scl2::compile_style(styles);

// 应用样式到文本
for (size_t i = 0; i < styles.size(); ++i) {
    std::cout << scl2::apply_style("Hello", styles[i]);
}
```

### 样式转 SGR（流式友好）

```cpp
std::cout << scl2::style_to_sgr({.bold = true, .text_color = scl2::named_colors::Orange})
          << "粗体橙色文字" << scl2::reset_color << std::endl;
```

### 可点击超链接

```cpp
std::cout << scl2::hyperlink("https://example.com", "点击这里") << std::endl;
std::cout << scl2::file_link("/path/to/file.txt", "打开文件") << std::endl;
```

### 进度条

```cpp
scl2::progress_bar_colored(0.75, 40, scl2::colors::green);
// 输出: [============================                       ] 75.0%
```

## 颜色函数

### map_system_color

```cpp
int map_system_color(const color& c, bool is_background);
```

将系统颜色映射为其 ANSI 码数字。返回 `31` 表示红色前景，`41` 表示红色背景等。

### to_rgb_code

```cpp
std::string to_rgb_code(const color& c, bool is_background);
```

将 RGB 颜色转换为 ANSI SGR 参数字符串。前景返回 `"38;2;R;G;B"`，背景返回 `"48;2;R;G;B"`。

### to_ansi_code

```cpp
std::string to_ansi_code(const color& c, const color& b = colors::null);
std::wstring wto_ansi_code(const color& c, const color& b = colors::null);
```

将颜色对（前景 + 背景）转换为完整的 ANSI 转义序列。例如返回 `"\033[31;44m"` 表示红字蓝底。

### reset_color / wreset_color

```cpp
constexpr std::string_view reset_color = "\033[0m";
constexpr std::wstring_view wreset_color = L"\033[0m";
```

将所有 ANSI 属性重置为默认值。可直接与 `std::cout` 一起使用。

## 便捷函数

```cpp
std::string textColor(const color& text, const color& background = colors::null);
std::wstring wtextColor(const color& text, const color& background = colors::null);

std::string bgcolor(const color& background);
std::wstring wbgcolor(const color& background);

std::string coloredText(std::string content, const color& text, const color& background = colors::null);

std::string conditionalText(bool enabled, std::string content,
    const color& text,
    const color& textDisabled = colors::white,
    const color& background = colors::null,
    const color& backgroundDisabled = colors::null);
```

`to_ansi_code` 的简单包装，用于常见使用场景。

## 超链接函数（OSC 8）

```cpp
std::string hyperlink(std::string target, std::string display = "");
std::string file_link(std::string path, std::string display = "");
std::string web_link(std::string target, std::string display = "", std::string protocol = "http");
```

使用 OSC 8 转义序列格式生成可点击的终端超链接。

## 样式系统

### text_style

```cpp
struct text_style {
    tri_state bold = tri_state::not_set;
    tri_state italic = tri_state::not_set;
    tri_state underline = tri_state::not_set;
    bool is_inherit = false;
    color text_color = colors::null;
    bool text_color_inherit = false;
    color background_color = colors::null;
    bool background_color_inherit = false;
};
```

单个样式块。`tri_state` 有三个值：`yes`、`no`、`not_set`。`compile_style()` 后，所有 `not_set` 值会被解析为显式的 `yes`/`no` 或继承值。

```cpp
typedef std::vector<text_style> sentence_style;
```

### make_style

```cpp
std::vector<text_style> make_style(const std::string& input);
```

将样式字符串解析为 `text_style` 块的向量。块之间以 `;` 分隔，属性以 `,` 分隔。

**语法：**

| 属性 | 语法 | 示例 |
|-----------|--------|---------|
| 文字颜色 | `t:` + 颜色 | `t:r`, `t:#ff0000` |
| 背景颜色 | `b:` + 颜色 | `b:g`, `b:#00ff00` |
| 粗体 | `b` | `b` |
| 斜体 | `i` | `i` |
| 下划线 | `u` | `u` |
| 取反 | `!` + 属性 | `!b`（非粗体） |
| 继承 | `<` | `<`（仅限块首） |
| 分隔符 | `;` 分隔块 | `t:r;t:g` |

**颜色语法：**

| 类型 | 语法 | 示例 |
|------|--------|---------|
| 系统色（暗） | 单个小写字母 | `r`=红, `g`=绿, `y`=黄, `b`=黑, `bl`=蓝, `p`=紫, `c`=青, `w`=白 |
| 系统色（亮） | 单个大写字母 | `R`=亮红, `G`=亮绿等 |
| RGB | `#RRGGBB` | `#ff6600` |
| 重置 | `-` 或空 | `t:-` 或 `t:` |
| 继承颜色 | `<` | `t:<`, `b:<` |

**示例：**

```
t:r                     → 红色文字
t:r,b:g                 → 红色文字，绿色背景
t:#ff0000,bold          → RGB 红色文字，粗体
b:R,italic              → 亮红色背景，斜体
t:r,b:g,bold,underline  → 所有属性
<,t:g                   → 继承前一块，然后文字设为绿色
```

### compile_style

```cpp
std::vector<text_style>& compile_style(std::vector<text_style>& styles);
```

解析样式向量中的继承关系。第一个块的 `not_set` 值变为 `no`。后续块在标记为 `<` 或有按颜色继承标记时从前一块继承。

### style_to_sgr

```cpp
std::string style_to_sgr(const text_style& style);
```

将已解析的 `text_style` 转换为完整的 ANSI SGR 序列。采用全重置方法：以 `0` 开头，然后设置所需属性。

```
style_to_sgr({.bold=true})                    → "\033[0;1m"
style_to_sgr({.text_color=colors::red})       → "\033[0;31m"
style_to_sgr({.bold=true, .italic=true})       → "\033[0;1;3m"
```

### apply_style

```cpp
std::string apply_style(const std::string& input, const text_style& style);
std::stringlist apply_style(const std::stringlist& lines, const std::vector<text_style>& styles);
std::string apply_style_inline(const std::stringlist& lines, const std::vector<text_style>& styles);
```

使用 ANSI 样式码包裹文本。单文本版本返回 `style_to_sgr(style) + input + reset_color`。stringlist 版本将样式应用于每一行（如果样式少于行数则循环使用）。

## 进度条

```cpp
void progress_bar_colored(double progress, int length = 140, const color& color = colors::green);
void progress_bar_texted(double progress, int length = 140);
void progress_bar_colored(int current, int all, int length = 140, const color& color = colors::green);
void progress_bar_texted(int current, int all, int length = 140);
```

在终端中显示进度条。彩色版本使用 ANSI 颜色；纯文本版本使用普通字符。

```cpp
scl2::progress_bar_colored(42, 100, 50, scl2::colors::cyan);
// 输出: [============================                       ] 42/100 42.0%
```
