#include "color.hpp"

#include <stdexcept>

namespace scl2 {


static constexpr color builtin_colors_rgb[] = {
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

constexpr color color::to_rgb() const
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

constexpr color color::to_rgba() const
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

constexpr color color::to_cmyk() const
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

constexpr bool color::operator==(const color &other) const
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

} // namespace scl2