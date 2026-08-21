#include "bitmap.hpp"

namespace scl2 {

// ---- bitmap<bool> : 1-bit monochrome ----
// (constructor is defined inline in bitmap.hpp)

bool bitmap<bool>::setPixel(size_t x, size_t y, bool value)
{
    __access_check(x, y);
    auto [byte_index, pindex] = getIndex(x, y);

    bits::bits b{pixel_data[byte_index]};
    bool original_value = b.get(pindex);
    b.put(pindex, value);
    pixel_data[byte_index] = b.value();
    return original_value;
}

bool bitmap<bool>::getPixel(size_t x, size_t y) const
{
    __access_check(x, y);
    auto [byte_index, pindex] = getIndex(x, y);
    bits::bits b{pixel_data[byte_index]};
    return b.get(pindex);
}

void bitmap<bool>::resize(size_t width, size_t height)
{
    if (width == 0 || height == 0) {
        header = bitmap_header_1c{};
        m_row_bytes = 0;
        pixel_data.clear();
        return;
    }

    size_t row_bytes = (width + 7) / 8;
    if (m_row_align > 1)
        row_bytes = ((row_bytes + m_row_align - 1) / m_row_align) * m_row_align;
    m_row_bytes = row_bytes;

    header.biWidth    = static_cast<int32_t>(width);
    header.biHeight   = static_cast<int32_t>(height);
    header.biSize     = 40;                       // BITMAPINFOHEADER
    header.bfOffBits  = 14 + 40 + 8;              // file + info header + 2-entry palette
    header.biSizeImage = static_cast<uint32_t>(row_bytes * height);
    header.bfSize     = header.bfOffBits + header.biSizeImage;

    pixel_data.clear(); // discard existing data (see resize() docs)
    pixel_data.resize(header.biSizeImage, std::byte{0});
}

bitmap<bool> bitmap<bool>::resize(size_t width, size_t height, scl2::Alignment align) const
{
    bitmap result(width, height);
    const size_t sw = this->width(), sh = this->height();
    if (sw == 0 || sh == 0) return result;

    size_t ox = 0, oy = 0;
    if (align & Alignment::Right)        ox = width > sw ? width - sw : 0;
    else if (align & Alignment::HCenter) ox = width > sw ? (width - sw) / 2 : 0;
    if (align & Alignment::Bottom)       oy = height > sh ? height - sh : 0;
    else if (align & Alignment::VCenter) oy = height > sh ? (height - sh) / 2 : 0;

    for (size_t y = 0; y < sh && oy + y < height; ++y)
        for (size_t x = 0; x < sw && ox + x < width; ++x)
            result.setPixel(ox + x, oy + y, getPixel(x, y));
    return result;
}

void bitmap<bool>::extend(size_t width, size_t height, scl2::Alignment align)
{
    if (width < this->width() || height < this->height()) return; // only extend
    *this = resize(width, height, align);
}

void bitmap<bool>::shrink(size_t width, size_t height, scl2::Alignment align)
{
    if (width > this->width() || height > this->height()) return; // only shrink
    *this = resize(width, height, align);
}

std::pair<size_t, size_t> bitmap<bool>::getSize() const
{
    return { width(), height() };
}

size_t bitmap<bool>::pixelCount() const { return width() * height(); }
size_t bitmap<bool>::sizeInBytes() const { return pixel_data.size(); }

void bitmap<bool>::set_row_align(size_t bytes)
{
    if (bytes == 0) bytes = 1;
    m_row_align = bytes;
    if (width() > 0 || height() > 0) resize(width(), height()); // reallocate
}

scl2::bytearray bitmap<bool>::toByteArray() const
{
    return pixel_data;
}

scl2::bytearray bitmap<bool>::toBoolArray() const
{
    const size_t n = pixelCount();
    scl2::bytearray arr(n, std::byte{0});
    for (size_t i = 0; i < n; ++i)
        arr[i] = getPixel(i % width(), i / width()) ? std::byte{1} : std::byte{0};
    return arr;
}

scl2::bytearray bitmap<bool>::toByteArrayPadded() const
{
    const size_t row_bytes = (width() + 7) / 8;
    const size_t padded_row = ((row_bytes + 3) / 4) * 4; // round up to multiple of 4

    scl2::bytearray result(padded_row * height(), std::byte{0});
    for (size_t y = 0; y < height(); ++y) {
        for (size_t x = 0; x < width(); ++x) {
            if (getPixel(x, y)) {
                size_t bi = y * padded_row + x / 8;
                int bit = 7 - static_cast<int>(x % 8); // MSB first (BMP layout)
                result[bi] |= std::byte{1} << bit;
            }
        }
    }
    return result;
}

scl2::bytearray bitmap<bool>::toBmp() const
{
    const size_t row_bytes = (width() + 7) / 8;
    const size_t padded_row = ((row_bytes + 3) / 4) * 4;
    const size_t image_size = padded_row * height();
    const uint32_t off_bits = 14 + 40 + 8;
    const uint32_t file_size = off_bits + static_cast<uint32_t>(image_size);

    scl2::bytearray out(file_size, std::byte{0});

    auto put_u16 = [&](size_t off, uint16_t v) {
        out[off]     = static_cast<std::byte>(v & 0xFF);
        out[off + 1] = static_cast<std::byte>((v >> 8) & 0xFF);
    };
    auto put_u32 = [&](size_t off, uint32_t v) {
        for (int i = 0; i < 4; ++i) out[off + i] = static_cast<std::byte>((v >> (8 * i)) & 0xFF);
    };

    put_u16(0, 0x4D42);                             // "BM"
    put_u32(2, file_size);                          // file size
    put_u32(10, off_bits);                          // pixel data offset
    put_u32(14, 40);                                // BITMAPINFOHEADER size
    put_u32(18, static_cast<uint32_t>(width()));
    put_u32(22, static_cast<uint32_t>(height()));
    put_u16(26, 1);                                 // planes
    put_u16(28, 1);                                 // bits per pixel
    put_u32(30, 0);                                 // BI_RGB
    put_u32(34, static_cast<uint32_t>(image_size)); // image size

    // 2-entry palette: index 0 = white, index 1 = black
    // (a "1" bit in the pixel data means dark / foreground)
    out[54] = out[55] = out[56] = out[57] = std::byte{255};
    out[58] = out[59] = out[60] = std::byte{0};
    out[61] = std::byte{0};

    // pixel data: bottom-up rows, MSB first, 1 = dark (black)
    for (size_t y = 0; y < height(); ++y) {
        const size_t src_y = height() - 1 - y;
        for (size_t x = 0; x < width(); ++x) {
            if (getPixel(x, src_y)) {
                size_t bi = off_bits + y * padded_row + x / 8;
                int bit = 7 - static_cast<int>(x % 8);
                out[bi] |= std::byte{1} << bit;
            }
        }
    }
    return out;
}

bitmap<bool> bitmap<bool>::fromBmp(const scl2::bytearray& data)
{
    // --- BITMAPFILEHEADER (14) + BITMAPINFOHEADER (>= 40) + palette ---
    if (data.size() < 14 + 40) {
        throw std::invalid_argument("fromBmp: data too small for a BMP header");
    }
    const auto rd_u16 = [&](size_t off) -> uint16_t {
        return static_cast<uint16_t>(static_cast<uint8_t>(data[off]))
             | static_cast<uint16_t>(static_cast<uint8_t>(data[off + 1])) << 8;
    };
    const auto rd_u32 = [&](size_t off) -> uint32_t {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
            v |= static_cast<uint32_t>(static_cast<uint8_t>(data[off + i])) << (8 * i);
        return v;
    };

    if (rd_u16(0) != 0x4D42) {
        throw std::invalid_argument("fromBmp: not a BMP file (bad signature)");
    }
    const uint32_t off_bits = rd_u32(10);
    const uint32_t info_size = rd_u32(14);
    const int32_t bi_width  = static_cast<int32_t>(rd_u32(18));
    const int32_t bi_height = static_cast<int32_t>(rd_u32(22));
    const uint16_t planes = rd_u16(26);
    const uint16_t bpp = rd_u16(28);
    const uint32_t compression = rd_u32(30);

    if (info_size < 40)         throw std::invalid_argument("fromBmp: unsupported info header");
    if (bi_width <= 0)          throw std::invalid_argument("fromBmp: invalid width");
    if (bi_height == 0)         throw std::invalid_argument("fromBmp: invalid height");
    if (planes != 1)            throw std::invalid_argument("fromBmp: only 1-plane BMP supported");
    if (bpp != 1)               throw std::invalid_argument("fromBmp: only 1-bit BMP supported");
    if (compression != 0)       throw std::invalid_argument("fromBmp: only uncompressed BMP supported");

    const size_t w = static_cast<size_t>(bi_width);
    const size_t h = static_cast<size_t>(bi_height < 0
                        ? -static_cast<int64_t>(bi_height)
                        : static_cast<int64_t>(bi_height));
    const bool top_down = bi_height < 0;

    const size_t row_bytes = (w + 7) / 8;
    const size_t padded_row = ((row_bytes + 3) / 4) * 4; // DWORD-aligned rows
    if (off_bits + padded_row * h > data.size()) {
        throw std::invalid_argument("fromBmp: pixel data truncated");
    }

    // Map the 1-bit palette to bitmap<bool>'s dark/light convention.
    // toBmp() writes palette[0] = white, palette[1] = black (1 = dark).
    // Follow the actual palette when present, otherwise assume 1 = dark.
    bool bit1_is_dark = true;
    const size_t pal = 14 + info_size;
    if (data.size() >= pal + 8) {
        const auto lum = [&](size_t off) -> unsigned {
            const unsigned r = static_cast<uint8_t>(data[off]);
            const unsigned g = static_cast<uint8_t>(data[off + 1]);
            const unsigned b = static_cast<uint8_t>(data[off + 2]);
            return (r + g + b) / 3;
        };
        bit1_is_dark = lum(pal) > lum(pal + 4);
    }

    bitmap result(w, h);
    for (size_t y = 0; y < h; ++y) {
        const size_t src_y = top_down ? y : (h - 1 - y);
        for (size_t x = 0; x < w; ++x) {
            const size_t bi = off_bits + src_y * padded_row + x / 8;
            const int bit = 7 - static_cast<int>(x % 8); // MSB first
            const bool bitval = (static_cast<uint8_t>(data[bi]) >> bit) & 1u;
            if (bit1_is_dark ? bitval : !bitval) result.setPixel(x, y, true);
        }
    }
    return result;
}

void bitmap<bool>::pixelAnd(size_t x, size_t y, const bitmap& other)
{
    overlap(x, y, other, [](bool a, bool b) -> bool { return a && b; });
}

void bitmap<bool>::pixelOr(size_t x, size_t y, const bitmap& other)
{
    overlap(x, y, other, [](bool a, bool b) -> bool { return a || b; });
}

void bitmap<bool>::pixelXor(size_t x, size_t y, const bitmap& other)
{
    overlap(x, y, other, [](bool a, bool b) -> bool { return a != b; });
}

void bitmap<bool>::pixelOverride(size_t x, size_t y, const bitmap& other)
{
    overlap(x, y, other, [](bool a, bool b) -> bool { return b; });
}

std::pair<size_t, size_t> bitmap<bool>::getIndex(size_t x, size_t y) const
{
    size_t byte_index = y * m_row_bytes + x / 8;
    size_t pindex = x % 8;
    return std::pair<size_t, size_t>(byte_index, pindex);
}

void bitmap<bool>::overlap(size_t x, size_t y, const bitmap& other, std::function<bool(bool, bool)> op)
{
    for (size_t j = 0; j < other.height(); ++j) {
        for (size_t i = 0; i < other.width(); ++i) {
            if (x + i >= width() || y + j >= height()) continue; // out of bounds
            bool pixel = getPixel(x + i, y + j), other_pixel = other.getPixel(i, j);
            bool result = op(pixel, other_pixel);
            if (result != pixel) setPixel(x + i, y + j, result);
        }
    }
}

void bitmap<bool>::__access_check(size_t x, size_t y) const
{
    if (x >= width() || y >= height()) {
        throw std::out_of_range("bitmap_1c: pixel coordinates out of bounds");
    }
}

} // namespace scl2