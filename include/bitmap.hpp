/*
    Bitmap standard module for SharedCppLib2.

    Provides a pixel-templated bitmap container:

      bitmap<Pixel>  generic color bitmap (any pixel type, e.g. scl2::color
                     for RGB / RGBA / CMYK).
      bitmap<bool>   (alias bitmap_1c) 1-bit packed monochrome with BMP I/O,
                     and configurable row alignment (byte / 32-bit rows) for
                     MCU / framebuffer use.

    bitmap_pattern<W,H> is a constexpr-friendly monochrome raster for
    hard-coding fixed patterns (QR markers, glyphs, ...), convertible to a
    runtime bitmap<bool>.

    The abstract draw_target<Pixel> interface lets rasterization primitives
    (see drawer.hpp) render onto any of these surfaces without knowing the
    concrete pixel storage.

    File support: BMP (1-bit). png, jpg, jpeg, and gif are not supported yet.
*/

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
#include <functional>
#include <stdexcept>

#include "basics.hpp"
#include "scldefs.hpp"
#include "bytearray.hpp"
#include "bits.hpp"

namespace scl2 {

/*
    Standard BMP header for 1-bit bitmaps.
    This header struct can be directly read/write to/from a file.
*/
struct bitmap_header_1c {
    uint16_t bfType = 0x4D42; // "BM" for bitmap files
    uint32_t bfSize = 0;      // Size of the file in bytes
    uint16_t bfReserved1 = 0; // Reserved, must be 0
    uint16_t bfReserved2 = 0; // Reserved, must be 0
    uint32_t bfOffBits = 0;   // Offset to the pixel data
    uint32_t biSize = 0;      // Size of the info header
    int32_t  biWidth = 0;     // Width of the image in pixels
    int32_t  biHeight = 0;    // Height of the image in pixels
    uint16_t biPlanes = 1;    // Number of color planes, must be 1
    uint16_t biBitCount = 1;  // Bits per pixel (1 for monochrome)
    uint32_t biCompression = 0; // Compression type (0 for no compression)
    uint32_t biSizeImage = 0;   // Size of the pixel data in bytes
};

/* Abstract interface that drawers render onto. */
template <typename Pixel>
class draw_target {
public:
    virtual ~draw_target() = default;

    virtual size_t width() const = 0;
    virtual size_t height() const = 0;

    /// @brief Set the pixel at (x, y).
    virtual void set_pixel(size_t x, size_t y, const Pixel& value) = 0;
    /// @brief Get the pixel at (x, y).
    virtual Pixel get_pixel(size_t x, size_t y) const = 0;
    /// @brief Reset all pixels to the type's default value.
    virtual void clear() = 0;
};

/*
    Generic color bitmap.

    Pixel is any color type with a default constructor, copy semantics and
    operator==. For 1-bit monochrome use bitmap<bool> (alias bitmap_1c),
    which is specialized below with packed storage and BMP I/O.
*/
template <typename Pixel>
class bitmap : public draw_target<Pixel> {
public:
    bitmap() = default;
    bitmap(size_t width, size_t height, const Pixel& init = Pixel{})
        : m_width(width), m_height(height), m_data(width * height, init) {}

    // draw_target interface
    size_t width() const override { return m_width; }
    size_t height() const override { return m_height; }
    void set_pixel(size_t x, size_t y, const Pixel& value) override {
        __access_check(x, y);
        m_data[y * m_width + x] = value;
    }
    Pixel get_pixel(size_t x, size_t y) const override {
        __access_check(x, y);
        return m_data[y * m_width + x];
    }
    void clear() override { std::fill(m_data.begin(), m_data.end(), Pixel{}); }

    // camelCase compatibility
    void setPixel(size_t x, size_t y, const Pixel& value) { set_pixel(x, y, value); }
    Pixel getPixel(size_t x, size_t y) const { return get_pixel(x, y); }

    std::pair<size_t, size_t> getSize() const { return { m_width, m_height }; }
    size_t pixelCount() const { return m_width * m_height; }

    /// @brief Resize, discarding existing pixel data.
    void resize(size_t width, size_t height) {
        m_data.assign(width * height, Pixel{});
        m_width = width;
        m_height = height;
    }

    /// @brief Resize preserving content with alignment; returns a new bitmap.
    bitmap resize(size_t width, size_t height, Alignment align = Alignment::TopLeft) const {
        bitmap result(width, height);
        place_content(result, *this, align);
        return result;
    }

    /// @brief Nearest-neighbor integer upscale (scale >= 1; 1 returns a copy).
    bitmap scaled(size_t scale) const {
        if (scale == 0) scale = 1;
        bitmap result(m_width * scale, m_height * scale);
        for (size_t dy = 0; dy < m_height * scale; ++dy) {
            const size_t sy = dy / scale;
            for (size_t dx = 0; dx < m_width * scale; ++dx)
                result.m_data[dy * result.m_width + dx] = m_data[sy * m_width + dx / scale];
        }
        return result;
    }

    /// @brief Nearest-neighbor integer downscale by factor (>= 1; 1 returns a copy).
    bitmap scaled_down(size_t factor) const {
        if (factor == 0) factor = 1;
        const size_t w = (m_width + factor - 1) / factor;
        const size_t h = (m_height + factor - 1) / factor;
        bitmap result(w, h);
        for (size_t y = 0; y < h; ++y)
            for (size_t x = 0; x < w; ++x)
                result.m_data[y * w + x] = m_data[(y * factor) * m_width + (x * factor)];
        return result;
    }

    /// @brief Fit this bitmap into a target region per a Stretch mode.
    /// @param mode  Fill (stretch) / Cover (crop) / Contain (letterbox) /
    ///              Center (original size) / Tile (repeat).
    /// @param align Placement within the region (used by Cover/Contain/Center/Tile).
    bitmap fit_into(size_t width, size_t height, Stretch mode = Stretch::Contain,
                    Alignment align = Alignment::Center) const {
        bitmap result(width, height);
        const size_t sw = m_width, sh = m_height;
        if (sw == 0 || sh == 0) return result;

        // Fill: non-uniform stretch over the whole target
        if (mode == Stretch::Fill) {
            for (size_t y = 0; y < height; ++y) {
                const size_t sy = (y * sh) / height;
                for (size_t x = 0; x < width; ++x)
                    result.m_data[y * width + x] = m_data[sy * sw + (x * sw) / width];
            }
            return result;
        }

        // Tile: repeat the source at its original size
        if (mode == Stretch::Tile) {
            size_t tx = 0, ty = 0;
            if (align & Alignment::Right)        tx = width > sw ? width - sw : 0;
            else if (align & Alignment::HCenter) tx = width > sw ? (width - sw) / 2 : 0;
            if (align & Alignment::Bottom)       ty = height > sh ? height - sh : 0;
            else if (align & Alignment::VCenter) ty = height > sh ? (height - sh) / 2 : 0;
            for (size_t y = 0; y < height; ++y)
                for (size_t x = 0; x < width; ++x)
                    result.m_data[y * width + x] = m_data[((y + ty) % sh) * sw + ((x + tx) % sw)];
            return result;
        }

        // Cover / Contain / Center: uniform scale + aligned placement
        double scale = 1.0;
        if (mode == Stretch::Cover)
            scale = std::max(static_cast<double>(width) / sw, static_cast<double>(height) / sh);
        else if (mode == Stretch::Contain)
            scale = std::min(static_cast<double>(width) / sw, static_cast<double>(height) / sh);
        // Center keeps scale == 1

        const int dw = static_cast<int>(sw * scale);
        const int dh = static_cast<int>(sh * scale);

        int ox = 0, oy = 0;
        if (align & Alignment::Right)        ox = static_cast<int>(width) - dw;
        else if (align & Alignment::HCenter) ox = (static_cast<int>(width) - dw) / 2;
        if (align & Alignment::Bottom)       oy = static_cast<int>(height) - dh;
        else if (align & Alignment::VCenter) oy = (static_cast<int>(height) - dh) / 2;

        for (size_t y = 0; y < height; ++y) {
            const int dy = static_cast<int>(y) - oy;
            if (dy < 0 || dy >= dh) continue;
            const size_t sy = std::min(static_cast<size_t>(static_cast<double>(dy) / scale), sh - 1);
            for (size_t x = 0; x < width; ++x) {
                const int dx = static_cast<int>(x) - ox;
                if (dx < 0 || dx >= dw) continue;
                const size_t sx = std::min(static_cast<size_t>(static_cast<double>(dx) / scale), sw - 1);
                result.m_data[y * width + x] = m_data[sy * sw + sx];
            }
        }
        return result;
    }

protected:
    // Throws std::out_of_range if (x, y) is out of bounds.
    void __access_check(size_t x, size_t y) const {
        if (x >= m_width || y >= m_height)
            throw std::out_of_range("bitmap: pixel coordinates out of bounds");
    }

    // Copy src content into dst (dst already sized) honoring alignment.
    static void place_content(bitmap& dst, const bitmap& src, Alignment align) {
        const size_t sw = src.width(), sh = src.height();
        const size_t dw = dst.width(), dh = dst.height();
        if (sw == 0 || sh == 0) return;

        size_t ox = 0, oy = 0;
        if (align & Alignment::Right)        ox = dw > sw ? dw - sw : 0;
        else if (align & Alignment::HCenter) ox = dw > sw ? (dw - sw) / 2 : 0;
        if (align & Alignment::Bottom)       oy = dh > sh ? dh - sh : 0;
        else if (align & Alignment::VCenter) oy = dh > sh ? (dh - sh) / 2 : 0;

        for (size_t y = 0; y < sh && oy + y < dh; ++y)
            for (size_t x = 0; x < sw && ox + x < dw; ++x)
                dst.set_pixel(ox + x, oy + y, src.get_pixel(x, y));
    }

private:
    size_t m_width = 0;
    size_t m_height = 0;
    std::vector<Pixel> m_data;
};

/*
    1-bit bitmap specialization.
    Pixels are packed 8-per-byte; BMP file I/O and bitwise operations are
    only meaningful for this monochrome variant.
*/
template <>
class bitmap<bool> : public draw_target<bool> {
public:
    bitmap() = default;
    bitmap(size_t width, size_t height) { resize(width, height); }
    /// @brief Construct with explicit row alignment (1 = byte-aligned rows, 4 = 32-bit rows).
    bitmap(size_t width, size_t height, size_t row_align_bytes) {
        set_row_align(row_align_bytes);
        resize(width, height);
    }

    bitmap(const bitmap&) = default;
    bitmap& operator=(const bitmap&) = default;
    bitmap(bitmap&&) noexcept = default;
    bitmap& operator=(bitmap&&) noexcept = default;

    // draw_target interface
    size_t width() const override { return static_cast<size_t>(header.biWidth); }
    size_t height() const override { return static_cast<size_t>(header.biHeight); }
    void set_pixel(size_t x, size_t y, const bool& value) override { setPixel(x, y, value); }
    bool get_pixel(size_t x, size_t y) const override { return getPixel(x, y); }
    void clear() override { std::fill(pixel_data.begin(), pixel_data.end(), std::byte{0}); }

    /// @brief Set the pixel value at (x, y) to the specified value.
    /// @return The original pixel value at (x, y) before the change.
    bool setPixel(size_t x, size_t y, bool value);

    /// @brief Get the pixel value at (x, y) without modifying it.
    bool getPixel(size_t x, size_t y) const;

    /// @brief Resize, discarding any existing pixel data.
    void resize(size_t width, size_t height);

    /// @brief Resize preserving existing pixel data with alignment.
    /// @return A new bitmap containing the existing pixel data.
    bitmap resize(size_t width, size_t height, scl2::Alignment align = scl2::Alignment::TopLeft) const;

    /// @brief Nearest-neighbor integer upscale (each module becomes scale x scale px).
    bitmap scaled(size_t scale) const {
        if (scale == 0) scale = 1;
        bitmap result(width() * scale, height() * scale);
        for (size_t y = 0; y < height() * scale; ++y)
            for (size_t x = 0; x < width() * scale; ++x)
                result.setPixel(x, y, getPixel(x / scale, y / scale));
        return result;
    }

    /// @brief Nearest-neighbor integer downscale by factor (>= 1).
    bitmap scaled_down(size_t factor) const {
        if (factor == 0) factor = 1;
        const size_t w = (width() + factor - 1) / factor;
        const size_t h = (height() + factor - 1) / factor;
        bitmap result(w, h);
        for (size_t y = 0; y < h; ++y)
            for (size_t x = 0; x < w; ++x)
                result.setPixel(x, y, getPixel(x * factor, y * factor));
        return result;
    }

    /// @brief Return an inverted copy (every pixel flipped), same row alignment.
    bitmap reverse_color() const {
        bitmap result(width(), height(), m_row_align);
        for (size_t y = 0; y < height(); ++y)
            for (size_t x = 0; x < width(); ++x)
                result.setPixel(x, y, !getPixel(x, y));
        return result;
    }

    /// @brief Fit this bitmap into a target region per a Stretch mode.
    /// @param mode  Fill (stretch) / Cover (crop) / Contain (letterbox) /
    ///              Center (original size) / Tile (repeat).
    /// @param align Placement within the region (used by Cover/Contain/Center/Tile).
    bitmap fit_into(size_t width, size_t height, Stretch mode = Stretch::Contain,
                    Alignment align = Alignment::Center) const {
        bitmap result(width, height, m_row_align);
        const size_t sw = this->width(), sh = this->height();
        if (sw == 0 || sh == 0) return result;

        if (mode == Stretch::Fill) {
            for (size_t y = 0; y < height; ++y) {
                const size_t sy = (y * sh) / height;
                for (size_t x = 0; x < width; ++x)
                    result.setPixel(x, y, getPixel((x * sw) / width, sy));
            }
            return result;
        }

        if (mode == Stretch::Tile) {
            size_t tx = 0, ty = 0;
            if (align & Alignment::Right)        tx = width > sw ? width - sw : 0;
            else if (align & Alignment::HCenter) tx = width > sw ? (width - sw) / 2 : 0;
            if (align & Alignment::Bottom)       ty = height > sh ? height - sh : 0;
            else if (align & Alignment::VCenter) ty = height > sh ? (height - sh) / 2 : 0;
            for (size_t y = 0; y < height; ++y)
                for (size_t x = 0; x < width; ++x)
                    result.setPixel(x, y, getPixel((x + tx) % sw, (y + ty) % sh));
            return result;
        }

        double scale = 1.0;
        if (mode == Stretch::Cover)
            scale = std::max(static_cast<double>(width) / sw, static_cast<double>(height) / sh);
        else if (mode == Stretch::Contain)
            scale = std::min(static_cast<double>(width) / sw, static_cast<double>(height) / sh);
        // Center keeps scale == 1

        const int dw = static_cast<int>(sw * scale);
        const int dh = static_cast<int>(sh * scale);
        int ox = 0, oy = 0;
        if (align & Alignment::Right)        ox = static_cast<int>(width) - dw;
        else if (align & Alignment::HCenter) ox = (static_cast<int>(width) - dw) / 2;
        if (align & Alignment::Bottom)       oy = static_cast<int>(height) - dh;
        else if (align & Alignment::VCenter) oy = (static_cast<int>(height) - dh) / 2;

        for (size_t y = 0; y < height; ++y) {
            const int dy = static_cast<int>(y) - oy;
            if (dy < 0 || dy >= dh) continue;
            const size_t sy = std::min(static_cast<size_t>(static_cast<double>(dy) / scale), sh - 1);
            for (size_t x = 0; x < width; ++x) {
                const int dx = static_cast<int>(x) - ox;
                if (dx < 0 || dx >= dw) continue;
                const size_t sx = std::min(static_cast<size_t>(static_cast<double>(dx) / scale), sw - 1);
                result.setPixel(x, y, getPixel(sx, sy));
            }
        }
        return result;
    }

    /// @brief Resize (only extending), preserving pixel data with alignment.
    void extend(size_t width, size_t height, scl2::Alignment align = scl2::Alignment::TopLeft);

    /// @brief Resize (only shrinking), preserving pixel data with alignment.
    void shrink(size_t width, size_t height, scl2::Alignment align = scl2::Alignment::TopLeft);

    std::pair<size_t, size_t> getSize() const;
    size_t pixelCount() const;
    size_t sizeInBytes() const;

    /// @brief Row alignment in bytes (1 = byte-aligned rows, 4 = 32-bit rows).
    size_t row_align() const { return m_row_align; }
    /// @brief Bytes per row, including alignment padding.
    size_t row_bytes() const { return m_row_bytes; }
    /// @brief Change row alignment and reallocate (0 resets to 1).
    void set_row_align(size_t bytes);

    /// @brief The underlying packed pixel bytes (8 pixels per byte, bit 0 = LSB).
    scl2::bytearray toByteArray() const;

    /// @brief Each pixel expanded to one byte: 0x00 or 0x01.
    scl2::bytearray toBoolArray() const;

    /// @brief Rows padded to 4-byte boundaries (BMP layout, MSB first).
    scl2::bytearray toByteArrayPadded() const;

    /// @brief Export as a complete 1-bit BMP file.
    scl2::bytearray toBmp() const;

    /// @brief Parse a 1-bit BMP file (as raw bytes) into a monochrome bitmap.
    /// Supports the format written by toBmp(), plus top-down / bottom-up rows
    /// and either palette order (the darker palette entry maps to `true`).
    /// @throw std::invalid_argument if the data is not a supported 1-bit BMP.
    static bitmap fromBmp(const scl2::bytearray& data);

    /// @brief Pixel-level bitwise ops with another bitmap (result stored in this).
    void pixelAnd(size_t x, size_t y, const bitmap& other);
    void pixelOr(size_t x, size_t y, const bitmap& other);
    void pixelXor(size_t x, size_t y, const bitmap& other);
    void pixelOverride(size_t x, size_t y, const bitmap& other);

protected:
    std::pair<size_t, size_t> getIndex(size_t x, size_t y) const;

    void overlap(size_t x, size_t y, const bitmap& other, std::function<bool(bool, bool)> op);

    // Throws std::out_of_range if (x, y) is out of bounds.
    void __access_check(size_t x, size_t y) const;

private:
    bitmap_header_1c header;
    size_t m_row_align = 1;   // row alignment in bytes (1 = byte, 4 = 32-bit)
    size_t m_row_bytes = 0;   // bytes per row (incl. alignment padding)
    scl2::bytearray pixel_data; // 1-bit per pixel, packed (8 pixels per byte)
};

/// Alias kept for backward compatibility.
using bitmap_1c = bitmap<bool>;
// using bitmap_3c = bitmap<scl2::color>; // RGB

/*
    Compile-time 1-bit pattern.

    A constexpr-friendly monochrome raster stored as packed bytes in a
    std::array, usable in constant expressions (static_assert, constexpr
    function results, ...). Use it to hard-code known patterns (QR finder /
    alignment markers, glyphs, ...) and convert to a runtime bitmap<bool>
    when needed.

    Bit order matches bitmap<bool>: bit (x % 8) of a row byte is pixel (x, y).
*/
template <size_t W, size_t H>
class bitmap_pattern {
public:
    static constexpr size_t row_bytes = (W + 7) / 8;
    static constexpr size_t byte_count = row_bytes * H;

    constexpr bitmap_pattern() = default;

    constexpr void set(size_t x, size_t y, bool v) {
        std::byte& b = m_data[y * row_bytes + x / 8];
        if (v) b |= std::byte{1} << (x % 8);
        else   b &= ~(std::byte{1} << (x % 8));
    }
    constexpr bool get(size_t x, size_t y) const {
        return (m_data[y * row_bytes + x / 8] & (std::byte{1} << (x % 8))) != std::byte{0};
    }

    constexpr size_t width() const { return W; }
    constexpr size_t height() const { return H; }
    constexpr const std::array<std::byte, byte_count>& data() const { return m_data; }

    /// @brief Convert to a runtime 1-bit bitmap (default row alignment).
    bitmap<bool> to_bitmap() const {
        bitmap<bool> bm(W, H);
        for (size_t y = 0; y < H; ++y)
            for (size_t x = 0; x < W; ++x)
                if (get(x, y)) bm.setPixel(x, y, true);
        return bm;
    }

private:
    std::array<std::byte, byte_count> m_data{};
};


}