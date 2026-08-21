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

#include <algorithm>
#include <cstdint>
#include <stdexcept>

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

    constexpr bool is_null() const { return type == ColorType::Null; }
    constexpr bool is_rgb() const { return type == ColorType::RGB; }
    constexpr bool is_rgba() const { return type == ColorType::RGBA; }
    constexpr bool is_cmyk() const { return type == ColorType::CMYK; }
    constexpr bool is_builtin() const { return type == ColorType::Builtin; }
    constexpr bool is_system() const { return type == ColorType::System; }
    constexpr bool is_style() const { return type == ColorType::Style; }
    constexpr explicit operator bool() const { return type != ColorType::Null; }

    // System and Style colors cannot be converted to regular color resources,
    // since they are dependent on the things outside the program.
    constexpr color to_rgb() const;
    constexpr color to_rgba() const;
    constexpr color to_cmyk() const;

    // Note: not simple one-to-one mapping, but do actual conversion (except System / Style colors)
    constexpr bool operator==(const color &other) const;

    // Blend toward another color (linear interpolation in RGBA space).
    // `t` is the weight (0..255) of `other`; 255 = fully `other`. Result is RGBA.
    constexpr color blend(const color& other, uint8_t t = 128) const;
    // a + b blends the two colors 50/50.
    constexpr color operator+(const color& other) const { return blend(other); }

private:
    // initialize placeholder, to prevent uninitialized values in constexpr constructor.
    constexpr color(ColorType type, uint8_t v1, uint8_t v2 = 0, uint8_t v3 = 0, uint8_t v4 = 0)
        : type(type), v1(v1), v2(v2), v3(v3), v4(v4) {}
};

// Lightweight packed RGBA pixel.
//
// `color` is 5 bytes (a type tag + 4 channels) and carries semantic color
// information (RGB/RGBA/CMYK/builtin/system/style). That makes it a poor
// per-pixel storage type for bitmaps: a dense 4-byte RGBA array is 20% smaller
// and avoids padding/alignment surprises. `rgba8` is that storage type — a
// plain packed pixel with no tag. Convert via rgba8(color) / rgba8::to_color().
struct rgba8 {
    uint8_t r, g, b, a;

    constexpr rgba8() : r(0), g(0), b(0), a(255) {}
    constexpr rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}
    constexpr rgba8(const color& c) {
        const color cc = c.to_rgba();
        r = cc.v1; g = cc.v2; b = cc.v3; a = cc.v4;
    }

    constexpr color to_color() const { return color::rgba(r, g, b, a); }
    constexpr operator color() const { return to_color(); }

    constexpr bool operator==(const rgba8&) const = default;

    // Linear interpolation toward `other` (t: 0..255 weight of `other`).
    constexpr rgba8 blend(const rgba8& other, uint8_t t = 128) const {
        const int wt = static_cast<int>(t);
        const int wn = 255 - wt;
        return rgba8(
            static_cast<uint8_t>((static_cast<int>(r) * wn + static_cast<int>(other.r) * wt) / 255),
            static_cast<uint8_t>((static_cast<int>(g) * wn + static_cast<int>(other.g) * wt) / 255),
            static_cast<uint8_t>((static_cast<int>(b) * wn + static_cast<int>(other.b) * wt) / 255),
            static_cast<uint8_t>((static_cast<int>(a) * wn + static_cast<int>(other.a) * wt) / 255));
    }
    constexpr rgba8 operator+(const rgba8& other) const { return blend(other); }
};

// Builtin color lookup table (RGB), indexed by builtin_color.
// Defined at namespace scope unconditionally because to_rgb()/to_rgba()/
// to_cmyk() reference it, even when SCL2_NO_BUILTIN_COLORS is defined.
inline constexpr color builtin_colors_rgb[] = {
    color(255, 255, 255), // White
    color(255, 0, 0),     // Red
    color(255, 165, 0),   // Orange
    color(255, 255, 0),   // Yellow
    color(0, 128, 0),     // Green
    color(0, 255, 255),   // Cyan
    color(0, 0, 255),     // Blue
    color(128, 0, 128),   // Purple
    color(0, 0, 0)        // Black
};

// Conversion implementations are inline constexpr here (not in a .cpp) so
// they are usable in constant expressions from any translation unit.
inline constexpr color color::to_rgb() const
{
    switch (type) {
        case ColorType::RGB:
            return *this; // Already RGB
        case ColorType::RGBA:
            return color(ColorType::RGB, v1, v2, v3); // Drop alpha channel
        case ColorType::CMYK: {
            // Convert CMYK to RGB
            uint8_t r = static_cast<uint8_t>(255 * (1 - v1 / 255.0) * (1 - v4 / 255.0));
            uint8_t g = static_cast<uint8_t>(255 * (1 - v2 / 255.0) * (1 - v4 / 255.0));
            uint8_t b = static_cast<uint8_t>(255 * (1 - v3 / 255.0) * (1 - v4 / 255.0));
            return color(ColorType::RGB, r, g, b);
        }
        case ColorType::Builtin:
            return builtin_colors_rgb[v1]; // Map builtin color to RGB
        case ColorType::System:
        case ColorType::Style:
            throw std::runtime_error("Cannot convert System/Style color to RGB");
        default:
            throw std::runtime_error("Invalid color type for conversion to RGB");
    }
}

inline constexpr color color::to_rgba() const
{
    switch (type) {
        case ColorType::RGB:
            return color(ColorType::RGBA, v1, v2, v3, 255); // Add alpha channel, consider as fully opaque
        case ColorType::RGBA:
            return *this; // Already RGBA
        case ColorType::CMYK: {
            // Convert CMYK to RGBA
            uint8_t r = static_cast<uint8_t>(255 * (1 - v1 / 255.0) * (1 - v4 / 255.0));
            uint8_t g = static_cast<uint8_t>(255 * (1 - v2 / 255.0) * (1 - v4 / 255.0));
            uint8_t b = static_cast<uint8_t>(255 * (1 - v3 / 255.0) * (1 - v4 / 255.0));
            return color(ColorType::RGBA, r, g, b, 255);
        }
        case ColorType::Builtin:
            return builtin_colors_rgb[v1].to_rgba(); // Map builtin color to RGBA with alpha = 255
        case ColorType::System:
        case ColorType::Style:
            throw std::runtime_error("Cannot convert System/Style color to RGBA");
        default:
            throw std::runtime_error("Invalid color type for conversion to RGBA");
    }
}

inline constexpr color color::to_cmyk() const
{
    switch (type) {
        case ColorType::RGB: {
            // Convert RGB to CMYK
            double r = v1 / 255.0;
            double g = v2 / 255.0;
            double b = v3 / 255.0;
            double k = 1 - std::max({r, g, b});
            if (k == 1) {
                return color(ColorType::CMYK, 0, 0, 0, 255);
            }
            double c = (1 - r - k) / (1 - k);
            double m = (1 - g - k) / (1 - k);
            double y = (1 - b - k) / (1 - k);
            return color(ColorType::CMYK, static_cast<uint8_t>(c * 255), static_cast<uint8_t>(m * 255), static_cast<uint8_t>(y * 255), static_cast<uint8_t>(k * 255));
        }
        case ColorType::RGBA: {
            // Convert RGBA to CMYK
            double r = v1 / 255.0;
            double g = v2 / 255.0;
            double b = v3 / 255.0;
            double k = 1 - std::max({r, g, b});
            if (k == 1) {
                return color(ColorType::CMYK, 0, 0, 0, 255);
            }
            double c = (1 - r - k) / (1 - k);
            double m = (1 - g - k) / (1 - k);
            double y = (1 - b - k) / (1 - k);
            return color(ColorType::CMYK, static_cast<uint8_t>(c * 255), static_cast<uint8_t>(m * 255), static_cast<uint8_t>(y * 255), static_cast<uint8_t>(k * 255));
        }
        case ColorType::CMYK:
            return *this; // Already CMYK
        case ColorType::Builtin:
            return builtin_colors_rgb[v1].to_cmyk(); // Map builtin color to CMYK
        case ColorType::System:
        case ColorType::Style:
            throw std::runtime_error("Cannot convert System/Style color to CMYK");
        default:
            throw std::runtime_error("Invalid color type for conversion to CMYK");
    }
}

inline constexpr bool color::operator==(const color &other) const
{
    if (type != other.type) {
        switch (type) {
            case ColorType::RGB:
            case ColorType::RGBA:
                return to_rgba() == other.to_rgba();
            case ColorType::CMYK:
                return to_rgb() == other.to_rgb();
            case ColorType::Builtin:
                return to_rgba() == other.to_rgba();
            case ColorType::System:
            case ColorType::Style:
                return false; // Cannot compare System/Style colors
            default:
                return false;
        }
    } else {
        switch (type) {
            case ColorType::RGB:
            case ColorType::RGBA:
            case ColorType::CMYK:
            case ColorType::Builtin:
                return v1 == other.v1 && v2 == other.v2 && v3 == other.v3 && v4 == other.v4;
            case ColorType::System:
            case ColorType::Style:
                return v1 == other.v1; // Compare only the identifier for System/Style colors
            default:
                return false;
        }
    }
}

// color::blend — linear interpolation in RGBA space.
inline constexpr color color::blend(const color& other, uint8_t t) const
{
    const color a = to_rgba();
    const color b = other.to_rgba();
    const int wt = static_cast<int>(t);
    const int wn = 255 - wt;
    return color::rgba(
        static_cast<uint8_t>((static_cast<int>(a.v1) * wn + static_cast<int>(b.v1) * wt) / 255),
        static_cast<uint8_t>((static_cast<int>(a.v2) * wn + static_cast<int>(b.v2) * wt) / 255),
        static_cast<uint8_t>((static_cast<int>(a.v3) * wn + static_cast<int>(b.v3) * wt) / 255),
        static_cast<uint8_t>((static_cast<int>(a.v4) * wn + static_cast<int>(b.v4) * wt) / 255));
}

/// Blend two colors. `t` (0..255) is the weight of `b`; result is RGBA.
inline constexpr color blend(const color& a, const color& b, uint8_t t = 128)
{
    return a.blend(b, t);
}


#ifndef SCL2_NO_BUILTIN_COLORS

// Pre-compiled builtin colors in RGB format.
// constexpr now that to_rgb() is defined inline in this header.
namespace colors {

    inline constexpr color white = color(builtin_color::White).to_rgb();
    inline constexpr color red = color(builtin_color::Red).to_rgb();
    inline constexpr color orange = color(builtin_color::Orange).to_rgb();
    inline constexpr color yellow = color(builtin_color::Yellow).to_rgb();
    inline constexpr color green = color(builtin_color::Green).to_rgb();
    inline constexpr color cyan = color(builtin_color::Cyan).to_rgb();
    inline constexpr color blue = color(builtin_color::Blue).to_rgb();
    inline constexpr color purple = color(builtin_color::Purple).to_rgb();
    inline constexpr color black = color(builtin_color::Black).to_rgb();

    constexpr color null = color();

} // namespace colors

#endif // SCL2_NO_BUILTIN_COLORS



} // namespace scl2