/*
    PNG codec module for SharedCppLib2.

    Decodes PNG (ISO/IEC 15948, a.k.a. RFC 2083) into an RGBA8 bitmap, and
    encodes an RGBA8 bitmap back into a PNG stream. Built entirely on modules
    that already exist in the library:

      - `scl2::zlib`  — an IDAT chunk is exactly a zlib stream (RFC 1950), which
                        is the container `zlib::compress` emits and
                        `zlib::decompress` accepts. The decompressor implements
                        stored / fixed / dynamic blocks, so any stream produced
                        by a real PNG encoder decodes.
      - `scl2::crc32` — PNG chunk CRCs are CRC-32/PKZip (poly 0xEDB88320), which
                        is what `crc32` implements; its digest is already
                        big-endian, the byte order PNG wants.
      - `scl2::bitmap<rgba8>` — the pixel container. `png` derives from it, so a
                        decoded image *is* a bitmap: it can be drawn on, passed
                        to anything taking `bitmap<rgba8>&`, or sliced down with
                        `to_bitmap()`. `color.hpp` documents `rgba8` as the
                        dense per-pixel storage type (4 bytes, no type tag).

    Decoding covers what a conforming decoder must handle: bit depths
    1/2/4/8/16, colour types 0 (grey) / 2 (RGB) / 3 (palette) / 4 (grey+alpha)
    / 6 (RGBA), `tRNS` transparency (palette alpha or colour key), and Adam7
    interlacing. 16-bit samples are reduced to 8 bits by taking the high byte.

    Encoding writes 8-bit images only (the container is RGBA8), choosing
    between greyscale, greyscale+alpha, RGB, RGBA and palette. With the default
    `mode::auto_preserve` a decoded palette or greyscale image keeps its shape
    as long as the pixels still fit; otherwise it falls back to truecolour.

    Example:

        scl2::png img = scl2::png::decode(scl2::bytearray::fromFile("x.png"));
        for (scl2::rgba8& px : img.row(0)) px.a = 128;   // tap the bitmap API
        scl2::bytearray out = img.dump();                // re-encode

    `dump()`/`load()` are aliases of `encode()`/`decode()` following the
    `dump()`/`load()` convention documented in `api.hpp`, so a `png` also works
    with `scl2::gdump()` / `scl2::gload<png>()`.

    Note: encoding does not attempt to reproduce the source byte-for-byte. The
    zlib module uses a fixed-Huffman encoder, so the IDAT of a re-encoded file
    will differ from the original even when every pixel matches.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "basics.hpp"
#include "bitmap.hpp"
#include "bytearray.hpp"
#include "color.hpp"

namespace scl2 {

/*
    PNG image.

    Derives from `bitmap<rgba8>` (which itself implements `draw_target<rgba8>`),
    so all bitmap operations are available directly, and additionally carries
    the PNG-specific information gathered while decoding.
*/
class png : public bitmap<rgba8> {
public:
    /// PNG colour types (the IHDR colour-type byte).
    enum class color_type : uint8_t {
        grayscale       = 0,
        rgb             = 2,
        palette         = 3,
        grayscale_alpha = 4,
        rgba            = 6,
    };

    /// The IHDR of the file this image came from.
    struct info {
        size_t     width      = 0;
        size_t     height     = 0;
        uint8_t    bit_depth  = 8;
        color_type color      = color_type::rgba;
        bool       interlaced = false;
    };

    /// How `encode()` should lay the image out.
    enum class mode : uint8_t {
        auto_preserve,  ///< keep the source's palette / greyscale shape when it still fits
        rgb,            ///< 8-bit truecolour without alpha
        rgba,           ///< 8-bit truecolour with alpha
        grayscale,      ///< 8-bit greyscale; rejected if any pixel has R != G != B
        palette,        ///< palette built from the image itself (fails above 256 colours)
    };

    struct options {
        mode format    = mode::auto_preserve;
        int  filter    = -1;     ///< -1 = per-row heuristic, 0..4 = force that filter type
        bool interlace = false;  ///< write Adam7 (valid, but rarely worth the size)
    };

    /// An ancillary chunk carried over from the source file.
    struct chunk {
        std::string     type;
        scl2::bytearray data;
    };

    // ── format identification ────────────────────────────────────────

    static constexpr size_t  magic_size = 8;
    static constexpr uint8_t magic[magic_size] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
    };

    /// @brief Does a PNG stream start at the current read cursor?
    ///
    /// Sniffs without side effects: the cursor is saved and restored, so this is
    /// safe to call while another reader owns the cursor. Returns false for
    /// anything that is not PNG rather than throwing, which makes it usable for
    /// format auto-detection.
    static bool matches(const scl2::bytearray& data);

    // ── decoding ─────────────────────────────────────────────────────

    /// @brief Decode a complete PNG stream.
    /// @throws std::runtime_error if the stream is malformed or uses an
    ///         unsupported feature.
    static png decode(const scl2::bytearray& data);

    /// @brief Alias of decode(), following the library's `load()` naming.
    static png load(const scl2::bytearray& data) { return decode(data); }

    // ── encoding ─────────────────────────────────────────────────────

    /// @brief Encode this image into a PNG stream.
    /// @throws std::runtime_error if the requested format cannot represent it.
    scl2::bytearray encode(const options& opt = {}) const;

    /// @brief Alias of encode(), following the library's `dump()` naming.
    scl2::bytearray dump() const { return encode(); }

    // ── PNG-specific information ─────────────────────────────────────

    /// @brief IHDR of the source file (defaults for a freshly built image).
    const info& source_info() const { return m_info; }

    /// @brief Palette of the source file; empty for non-palette images.
    /// @note Alpha from a `tRNS` chunk is already folded in.
    const std::vector<rgba8>& palette() const { return m_palette; }

    /// @brief Ancillary chunks kept while decoding (see `encode()`: they are
    ///        exposed for inspection but not written back automatically).
    const std::vector<chunk>& ancillary() const { return m_ancillary; }

    // ── downgrading to a plain bitmap ────────────────────────────────

    /// @brief The pixels viewed as a plain bitmap (an implicit upcast also works).
    const bitmap<rgba8>& as_bitmap() const { return *this; }

    /// @brief Copy the pixels out, dropping every PNG-specific field.
    bitmap<rgba8> to_bitmap() const { return static_cast<const bitmap<rgba8>&>(*this); }

private:
    info               m_info;
    std::vector<rgba8> m_palette;
    std::vector<chunk> m_ancillary;
};

} // namespace scl2
