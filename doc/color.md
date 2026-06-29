# color - Color Types and Constants

+ Name: color
+ Namespace: `scl2`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `basic` (contains color) |

Include usage:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::basic)
```

```cpp
#include <SharedCppLib2/color.hpp>
```

For the extended named color library:
```cpp
#include <SharedCppLib2/named_colors.hpp>
```

## Description

The `color` module defines the fundamental color types used across SharedCppLib2. It provides a unified `color` struct that can represent colors in multiple formats (RGB, RGBA, CMYK, system, builtin), along with pre-defined color constants and an extensive named color collection.

## Color Types

### ColorType

```cpp
enum class ColorType : uint8_t {
    Null,     // No color / reset
    RGB,      // 24-bit RGB color
    RGBA,     // RGB with alpha channel
    CMYK,     // CMYK color
    Builtin,  // Pre-defined builtin color (mapped to RGB)
    System,   // Terminal system color (16 standard colors)
    Style     // Platform-specific style color (for GUI)
};
```

### Enumerations

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

## The color Struct

```cpp
struct color {
    ColorType type;
    uint8_t v1, v2, v3, v4;
};
```

### Constructors

```cpp
constexpr color();                                    // Null color (reset)
constexpr color(builtin_color bcid);                   // From builtin preset
constexpr color(system_color scid);                    // From system color
constexpr color(int r, int g, int b);                  // RGB (alpha = 255)
constexpr color(int r, int g, int b, int a);           // RGBA
```

### Static factory methods

```cpp
static constexpr inline color rgb(int r, int g, int b);
static constexpr inline color rgba(int r, int g, int b, int a);
static constexpr inline color cmyk(int c, int m, int y, int k);
```

### Type checks

```cpp
bool is_null() const;
bool is_rgb() const;
bool is_rgba() const;
bool is_cmyk() const;
bool is_builtin() const;
bool is_system() const;
bool is_style() const;
constexpr explicit operator bool() const;  // true if not Null
```

### Color space conversion

```cpp
constexpr color to_rgb() const;    // Convert to RGB (Builtin → mapped, RGBA → drop alpha)
constexpr color to_rgba() const;   // Convert to RGBA
constexpr color to_cmyk() const;   // Convert to CMYK
```

### Comparison

```cpp
constexpr bool operator==(const color& other) const;
```

Cross-type comparison converts to the more expressive format first (e.g., RGB and RGBA compare after unifying to RGBA).

## Built-in Color Constants

The `colors` namespace provides pre-compiled builtin colors as RGB values:

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
    constexpr color null;     // Null color (reset)
}
```

These can be disabled by defining `SCL2_NO_CONSTEXPR_BUILTIN_COLORS` before including the header.

## Named Colors Extension

The `named_colors` header extends `color` with a comprehensive set of CSS/HTML named colors:

```cpp
#include <SharedCppLib2/named_colors.hpp>

// Usage
scl2::named_colors::Orange
scl2::named_colors::Tomato
scl2::named_colors::SteelBlue
```

This collection includes over 200 named colors from the standard CSS color palette, including:
- Basic colors: `White`, `Black`, `Red`, `Blue`, `Green`, etc.
- Extended palette: `Coral`, `Salmon`, `Plum`, `Orchid`, etc.
- Shade variants: `Snow1`–`Snow4`, `Seashell1`–`Seashell4`, `Azure1`–`Azure4`, etc.

## tri_state

Defined in `scltypes.hpp`:

```cpp
struct tri_state {
    enum Value : uint8_t { no, yes, not_set };
    // Implicit conversions from bool and Value
    explicit operator bool() const;  // true iff value == yes
};
```

A three-state boolean used by the ANSI style system (`text_style`). `not_set` indicates the attribute was not specified and should inherit or default.

## Quick Start

```cpp
#include <SharedCppLib2/color.hpp>
#include <SharedCppLib2/named_colors.hpp>
#include <SharedCppLib2/ansiio.hpp>

// From builtin (auto-converted to RGB)
scl2::color c = scl2::colors::blue;
std::cout << scl2::textColor(c) << "Blue text" << scl2::reset_color << std::endl;

// From system color
scl2::color sys = scl2::system_color::LightCyan;
std::cout << scl2::textColor(sys) << "Light cyan" << scl2::reset_color << std::endl;

// RGB
scl2::color rgb(255, 165, 0);  // Orange
std::cout << scl2::textColor(rgb) << "RGB orange" << scl2::reset_color << std::endl;

// Named color
auto nc = scl2::named_colors::Tomato;
std::cout << scl2::textColor(nc) << "Tomato" << scl2::reset_color << std::endl;

// Null check
if (scl2::color()) {
    // Won't reach here — null color is falsy
}
```
