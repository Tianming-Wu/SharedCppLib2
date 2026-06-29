# ansiio - ANSI Terminal Output and Formatting

+ Name: ansiio
+ Namespace: `scl2::ansi` (inline)
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `basic` (contains ansiio) |

Include usage:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::basic)
```

```cpp
#include <SharedCppLib2/ansiio.hpp>
```

## Description

`ansiio` provides ANSI escape sequence generation for terminal output. It supports:

- **Colors**: System colors (16 standard), RGB (24-bit true color), and named colors
- **Text styles**: Bold, italic, underline, with inheritance support
- **Hyperlinks**: Clickable OSC 8 hyperlinks in supported terminals
- **Progress bars**: Colored and plain progress indicators
- **Style system**: Parse style strings into structured `text_style` objects, resolve inheritance, and render to ANSI codes

All ANSI functions are in the `scl2::ansi` inline namespace, making them accessible as `scl2::function_name()` directly.

## Quick Start

### Basic color output

```cpp
#include <SharedCppLib2/ansiio.hpp>
#include <SharedCppLib2/named_colors.hpp>

// Simple text color
std::cout << scl2::textColor(scl2::colors::red) << "Red text" << scl2::reset_color << std::endl;

// RGB color
std::cout << scl2::textColor(scl2::color(255, 165, 0)) << "Orange text" << scl2::reset_color << std::endl;

// Text + background
std::cout << scl2::to_ansi_code(scl2::colors::red, scl2::colors::white)
          << "Red on white" << scl2::reset_color << std::endl;
```

### Style system

```cpp
// Parse a style string
auto styles = scl2::make_style("t:r,bold;  t:g,italic;  <,b:bl");
scl2::compile_style(styles);

// Apply styles to text
for (size_t i = 0; i < styles.size(); ++i) {
    std::cout << scl2::apply_style("Hello", styles[i]);
}
```

### Style to SGR (stream-friendly)

```cpp
std::cout << scl2::style_to_sgr({.bold = true, .text_color = scl2::named_colors::Orange})
          << "Bold orange text" << scl2::reset_color << std::endl;
```

### Clickable hyperlinks

```cpp
std::cout << scl2::hyperlink("https://example.com", "Click here") << std::endl;
std::cout << scl2::file_link("/path/to/file.txt", "Open file") << std::endl;
```

### Progress bar

```cpp
scl2::progress_bar_colored(0.75, 40, scl2::colors::green);
// Output: [============================                       ] 75.0%
```

## Color Functions

### map_system_color

```cpp
int map_system_color(const color& c, bool is_background);
```

Maps a system color to its ANSI code number. Returns `31` for red text, `41` for red background, etc.

### to_rgb_code

```cpp
std::string to_rgb_code(const color& c, bool is_background);
```

Converts an RGB color to the ANSI SGR parameter string. Returns `"38;2;R;G;B"` for foreground or `"48;2;R;G;B"` for background.

### to_ansi_code

```cpp
std::string to_ansi_code(const color& c, const color& b = colors::null);
std::wstring wto_ansi_code(const color& c, const color& b = colors::null);
```

Converts a color pair (foreground + background) to a complete ANSI escape sequence. Returns `"\033[31;44m"` for red text on blue background, for example.

### reset_color / wreset_color

```cpp
constexpr std::string_view reset_color = "\033[0m";
constexpr std::wstring_view wreset_color = L"\033[0m";
```

Reset all ANSI attributes to default. Usable directly with `std::cout`.

## Convenience Functions

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

Simple wrappers around `to_ansi_code` for common use cases.

## Hyperlink Functions (OSC 8)

```cpp
std::string hyperlink(std::string target, std::string display = "");
std::string file_link(std::string path, std::string display = "");
std::string web_link(std::string target, std::string display = "", std::string protocol = "http");
```

Generate clickable terminal hyperlinks using the OSC 8 escape sequence format.

## Style System

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

A single style block. `tri_state` has three values: `yes`, `no`, `not_set`. After `compile_style()`, all `not_set` values are resolved to explicit `yes`/`no` or inherited values.

```cpp
typedef std::vector<text_style> sentence_style;
```

### make_style

```cpp
std::vector<text_style> make_style(const std::string& input);
```

Parse a style string into a vector of `text_style` blocks. Blocks are separated by `;`, attributes by `,`.

**Grammar:**

| Attribute | Syntax | Example |
|-----------|--------|---------|
| Text color | `t:` + color | `t:r`, `t:#ff0000` |
| Background color | `b:` + color | `b:g`, `b:#00ff00` |
| Bold | `b` | `b` |
| Italic | `i` | `i` |
| Underline | `u` | `u` |
| Negate | `!` + attribute | `!b` (not bold) |
| Inherit | `<` | `<` (first attribute only) |
| Separator | `;` between blocks | `t:r;t:g` |

**Color syntax:**

| Type | Syntax | Example |
|------|--------|---------|
| System (dark) | Single lowercase letter | `r`=red, `g`=green, `y`=yellow, `b`=black, `bl`=blue, `p`=purple, `c`=cyan, `w`=white |
| System (light) | Single uppercase letter | `R`=light red, `G`=light green, etc. |
| RGB | `#RRGGBB` | `#ff6600` |
| Reset | `-` or empty | `t:-` or `t:` |
| Inherit color | `<` | `t:<`, `b:<` |

**Examples:**

```
t:r                    → red text
t:r,b:g                → red text, green background
t:#ff0000,bold         → RGB red text, bold
b:R,italic             → light red background, italic
t:r,b:g,bold,underline → all attributes
<,t:g                  → inherit previous, then set text to green
```

### compile_style

```cpp
std::vector<text_style>& compile_style(std::vector<text_style>& styles);
```

Resolve inheritance in a style vector. The first block's `not_set` values become `no`. Subsequent blocks inherit from the previous block when marked with `<` or per-color inherit flags.

### style_to_sgr

```cpp
std::string style_to_sgr(const text_style& style);
```

Convert a resolved `text_style` to a complete ANSI SGR sequence. Uses the full-reset approach: starts with `0`, then sets desired attributes.

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

Wrap text with ANSI style codes. The single-text version returns `style_to_sgr(style) + input + reset_color`. The stringlist versions apply styles to each line (cycling if more lines than styles).

## Progress Bars

```cpp
void progress_bar_colored(double progress, int length = 140, const color& color = colors::green);
void progress_bar_texted(double progress, int length = 140);
void progress_bar_colored(int current, int all, int length = 140, const color& color = colors::green);
void progress_bar_texted(int current, int all, int length = 140);
```

Display progress bars in the terminal. The colored version uses ANSI colors; the texted version uses plain characters.

```cpp
scl2::progress_bar_colored(42, 100, 50, scl2::colors::cyan);
// Output: [============================                       ] 42/100 42.0%
```
