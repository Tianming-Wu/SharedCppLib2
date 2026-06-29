# color - 颜色类型与常量

+ 名称: color
+ 命名空间: `scl2`
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `basic` (包含 color) |

包含方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::basic)
```

```cpp
#include <SharedCppLib2/color.hpp>
```

扩展命名颜色库：
```cpp
#include <SharedCppLib2/named_colors.hpp>
```

## 描述

`color` 模块定义了 SharedCppLib2 中使用的核心颜色类型。它提供了一个统一的 `color` 结构体，可以以多种格式（RGB、RGBA、CMYK、系统色、内置色）表示颜色，同时提供了预定义的颜色常量和丰富的命名颜色集合。

## 颜色类型

### ColorType

```cpp
enum class ColorType : uint8_t {
    Null,     // 无颜色 / 重置
    RGB,      // 24 位 RGB 颜色
    RGBA,     // 带 Alpha 通道的 RGB
    CMYK,     // CMYK 颜色
    Builtin,  // 预定义内置颜色（映射为 RGB）
    System,   // 终端系统颜色（16 种标准色）
    Style     // 平台特定样式颜色（用于图形界面）
};
```

### 枚举

```cpp
enum class builtin_color : uint8_t {
    White, Red, Orange, Yellow, Green, Cyan, Blue, Purple, Black
};

enum class system_color : uint8_t {
    Null,
    Black, Red, Green, Yellow, Blue, Purple, Cyan, White, Orange,
    LightBlack, LightRed, LightGreen, LightYellow,
    LightBlue, LightPurple, LightCyan, LightWhite
};
```

## color 结构体

```cpp
struct color {
    ColorType type;
    uint8_t v1, v2, v3, v4;
};
```

### 构造函数

```cpp
constexpr color();                                    // 空颜色（重置）
constexpr color(builtin_color bcid);                   // 从内置预设创建
constexpr color(system_color scid);                    // 从系统颜色创建
constexpr color(int r, int g, int b);                  // RGB（alpha = 255）
constexpr color(int r, int g, int b, int a);           // RGBA
```

### 静态工厂方法

```cpp
static constexpr inline color rgb(int r, int g, int b);
static constexpr inline color rgba(int r, int g, int b, int a);
static constexpr inline color cmyk(int c, int m, int y, int k);
```

### 类型检查

```cpp
bool is_null() const;
bool is_rgb() const;
bool is_rgba() const;
bool is_cmyk() const;
bool is_builtin() const;
bool is_system() const;
bool is_style() const;
constexpr explicit operator bool() const;  // 非 Null 时为 true
```

### 色彩空间转换

```cpp
constexpr color to_rgb() const;    // 转换为 RGB（Builtin → 映射，RGBA → 丢弃 Alpha）
constexpr color to_rgba() const;   // 转换为 RGBA
constexpr color to_cmyk() const;   // 转换为 CMYK
```

### 比较

```cpp
constexpr bool operator==(const color& other) const;
```

跨类型比较会先将颜色转换为更具表现力的格式（例如 RGB 和 RGBA 统一为 RGBA 后比较）。

## 内置颜色常量

`colors` 命名空间提供预编译的内置颜色（以 RGB 值表示）：

```cpp
namespace colors {
    constexpr color white;    // (255, 255, 255)
    constexpr color red;      // (255, 0, 0)
    constexpr color orange;   // (255, 165, 0)
    constexpr color yellow;   // (255, 255, 0)
    constexpr color green;    // (0, 128, 0)
    constexpr color cyan;     // (0, 255, 255)
    constexpr color blue;     // (0, 0, 255)
    constexpr color purple;   // (128, 0, 128)
    constexpr color black;    // (0, 0, 0)
    constexpr color null;     // 空颜色（重置）
}
```

可以通过在包含头文件前定义 `SCL2_NO_CONSTEXPR_BUILTIN_COLORS` 来禁用这些常量。

## 命名颜色扩展

`named_colors` 头文件为 `color` 扩展了一套全面的 CSS/HTML 命名颜色：

```cpp
#include <SharedCppLib2/named_colors.hpp>

// 使用
scl2::named_colors::Orange
scl2::named_colors::Tomato
scl2::named_colors::SteelBlue
```

该集合包含超过 200 种来自标准 CSS 调色板的命名颜色，包括：
- 基本颜色：`White`、`Black`、`Red`、`Blue`、`Green` 等
- 扩展调色板：`Coral`、`Salmon`、`Plum`、`Orchid` 等
- 明度变体：`Snow1`–`Snow4`、`Seashell1`–`Seashell4`、`Azure1`–`Azure4` 等

## tri_state

定义于 `scltypes.hpp`：

```cpp
struct tri_state {
    enum Value : uint8_t { no, yes, not_set };
    // 从 bool 和 Value 的隐式转换
    explicit operator bool() const;  // 仅在 value == yes 时为 true
};
```

三态布尔值，由 ANSI 样式系统（`text_style`）使用。`not_set` 表示属性未指定，应继承或使用默认值。

## 快速开始

```cpp
#include <SharedCppLib2/color.hpp>
#include <SharedCppLib2/named_colors.hpp>
#include <SharedCppLib2/ansiio.hpp>

// 从内置颜色（自动转换为 RGB）
scl2::color c = scl2::colors::blue;
std::cout << scl2::textColor(c) << "蓝色文字" << scl2::reset_color << std::endl;

// 从系统颜色
scl2::color sys = scl2::system_color::LightCyan;
std::cout << scl2::textColor(sys) << "亮青色" << scl2::reset_color << std::endl;

// RGB
scl2::color rgb(255, 165, 0);  // 橙色
std::cout << scl2::textColor(rgb) << "RGB 橙色" << scl2::reset_color << std::endl;

// 命名颜色
auto nc = scl2::named_colors::Tomato;
std::cout << scl2::textColor(nc) << "番茄色" << scl2::reset_color << std::endl;

// 空值检查
if (scl2::color()) {
    // 不会执行到这里——空颜色为 falsy
}
```
