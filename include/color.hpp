/*
    Color Interface for SharedCppLib2


    Builtin colors are a simplified way of getting colors.

    System colors are for console output, so they are not guaranteed
    to be the same across different platforms. Actually, they are
    defined by the console itself. So it is not possible to get the
    exact RGB values of them.

    Style colors are for graphical interfaces, and they are defined
    by the operating system. For example, color_button and
    color_window are defined by the OS, and they may be different on
    different platforms. So it is not possible to get the exact RGB
    values of them either.

    These two types of colors are served as placeholders. The actual
    handling is done in other modules or projects.


    For more varied predefined colors, check named_colors.hpp.
    It provides a series of predefined colors under the CSS standard.

*/

#pragma once

#include "basics.hpp"

#include <cstdint>

namespace scl2 {

// Predefined standard colors
enum class builtin_color : uint8_t {
    White, Red, Orange, Yellow, Green, Cyan, Blue, Purple, Black
};

enum class system_color : uint8_t {
    Null,
    Black, Red, Green, Yellow, Blue, Purple, Cyan, White,
	LightBlack, LightRed, LightGreen, LightYellow, LightBlue, LightPurple, LightCyan, LightWhite,

    // 8-bit extension colors (may not be supported on all terminals)
    // Currently not supported, will be implemented in the future.
    Orange,
};

// Graphical interface style colors
// Just a placeholder for now, will be implemented in the future when developing relative
// libraries or projects.
enum class style_color : uint8_t {

};

// Placed outside for simplicity.
enum class ColorType : uint8_t {
    Null,
    RGB, RGBA,
    CMYK,
    Builtin, System,
    Style // For graphical interfaces.
};

struct color {
    ColorType type;
    uint8_t v1, v2, v3, v4;

    constexpr color() : color(ColorType::Null, 0, 0, 0, 0) {}
    constexpr color(builtin_color bcid) : color(ColorType::Builtin, static_cast<uint8_t>(bcid)) {}
    constexpr color(system_color scid) : color(ColorType::System, static_cast<uint8_t>(scid)) {}
    constexpr color(int r, int g, int b) : type(ColorType::RGB), v1(static_cast<uint8_t>(r)), v2(static_cast<uint8_t>(g)), v3(static_cast<uint8_t>(b)), v4(255) {}
    constexpr color(int r, int g, int b, int a) : type(ColorType::RGBA), v1(static_cast<uint8_t>(r)), v2(static_cast<uint8_t>(g)), v3(static_cast<uint8_t>(b)), v4(static_cast<uint8_t>(a)) {}

    constexpr color(const color &other) = default;
    constexpr color& operator=(const color &other) = default;
    constexpr color(color &&other) = default;
    constexpr color& operator=(color &&other) = default;

    static constexpr inline color rgb(int r, int g, int b) {
        return color(r, g, b);
    }

    static constexpr inline color rgba(int r, int g, int b, int a) {
        return color(r, g, b, a);
    }

    static constexpr inline color cmyk(int c, int m, int y, int k) {
        return color(ColorType::CMYK, static_cast<uint8_t>(c), static_cast<uint8_t>(m), static_cast<uint8_t>(y), static_cast<uint8_t>(k));
    }

    bool is_null() const { return type == ColorType::Null; }
    bool is_rgb() const { return type == ColorType::RGB; }
    bool is_rgba() const { return type == ColorType::RGBA; }
    bool is_cmyk() const { return type == ColorType::CMYK; }
    bool is_builtin() const { return type == ColorType::Builtin; }
    bool is_system() const { return type == ColorType::System; }
    bool is_style() const { return type == ColorType::Style; }
    constexpr explicit operator bool() const { return type != ColorType::Null; }

    // System and Style colors cannot be converted to regular color resources,
    // since they are dependent on the things outside the program.
    constexpr color to_rgb() const;
    constexpr color to_rgba() const;
    constexpr color to_cmyk() const;

    // Note: not simple one-to-one mapping, but do actual conversion (except System / Style colors)
    constexpr bool operator==(const color &other) const;

private:
    // initialize placeholder, to prevent uninitialized values in constexpr constructor.
    constexpr color(ColorType type, uint8_t v1, uint8_t v2 = 0, uint8_t v3 = 0, uint8_t v4 = 0)
        : type(type), v1(v1), v2(v2), v3(v3), v4(v4) {}
};


#ifndef SCL2_NO_BUILTIN_COLORS

// Pre-compiled builtin colors in RGB format.
// Uses inline const (not constexpr) because to_rgb() is defined in color.cpp
// and is not visible as constexpr from other translation units.
namespace colors {

    inline const color white = color(builtin_color::White).to_rgb();
    inline const color red = color(builtin_color::Red).to_rgb();
    inline const color orange = color(builtin_color::Orange).to_rgb();
    inline const color yellow = color(builtin_color::Yellow).to_rgb();
    inline const color green = color(builtin_color::Green).to_rgb();
    inline const color cyan = color(builtin_color::Cyan).to_rgb();
    inline const color blue = color(builtin_color::Blue).to_rgb();
    inline const color purple = color(builtin_color::Purple).to_rgb();
    inline const color black = color(builtin_color::Black).to_rgb();

    constexpr color null = color();

} // namespace colors

#endif // SCL2_NO_CONSTEXPR_BUILTIN_COLORS



} // namespace scl2