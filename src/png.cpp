#include "png.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "crc32.hpp"
#include "zlib.hpp"

namespace scl2 {

namespace {

// PNG stores 32-bit length/width/height; the spec caps them at 2^31 - 1.
constexpr uint32_t max_dimension = 0x7FFFFFFFu;

// A library that materialises the whole image as RGBA8 has to draw a line
// somewhere. 2^30 pixels is 4 GiB of pixels; beyond that we refuse rather than
// risk overflowing the size arithmetic below.
constexpr uint64_t max_pixels = 1ull << 30;

// ── big-endian scalar access ─────────────────────────────────────────────

uint8_t rd_u8(const scl2::bytearray& d, size_t o) {
    return std::to_integer<uint8_t>(d[o]);
}

uint16_t rd_u16(const scl2::bytearray& d, size_t o) {
    return static_cast<uint16_t>((static_cast<uint32_t>(rd_u8(d, o)) << 8) | rd_u8(d, o + 1));
}

uint32_t rd_u32(const scl2::bytearray& d, size_t o) {
    return (static_cast<uint32_t>(rd_u8(d, o)) << 24)
         | (static_cast<uint32_t>(rd_u8(d, o + 1)) << 16)
         | (static_cast<uint32_t>(rd_u8(d, o + 2)) << 8)
         |  static_cast<uint32_t>(rd_u8(d, o + 3));
}

void push_u8(scl2::bytearray& out, uint8_t v) {
    out.push_back(static_cast<std::byte>(v));
}

void push_u32(scl2::bytearray& out, uint32_t v) {
    push_u8(out, static_cast<uint8_t>(v >> 24));
    push_u8(out, static_cast<uint8_t>(v >> 16));
    push_u8(out, static_cast<uint8_t>(v >> 8));
    push_u8(out, static_cast<uint8_t>(v));
}

void push_tag(scl2::bytearray& out, const char* tag) {
    for (int i = 0; i < 4; ++i) push_u8(out, static_cast<uint8_t>(tag[i]));
}

// ── chunks ───────────────────────────────────────────────────────────────

/// Append one complete chunk: length, type, payload, CRC32(type + payload).
void append_chunk(scl2::bytearray& out, const char* tag, const scl2::bytearray& payload) {
    push_u32(out, static_cast<uint32_t>(payload.size()));
    push_tag(out, tag);

    scl2::bytearray crc_input;
    push_tag(crc_input, tag);
    if (!payload.empty()) {
        out.append(payload.data(), payload.size());
        crc_input.append(payload.data(), payload.size());
    }

    const scl2::bytearray digest = scl2::crc32::hash(crc_input);
    for (size_t i = 0; i < 4; ++i) out.push_back(digest[i]);
}

/// Verify the CRC of the chunk starting at `pos` (whose length field is `len`).
bool chunk_crc_ok(const scl2::bytearray& d, size_t pos, uint32_t len) {
    // CRC covers the type tag plus the payload.
    const scl2::bytearray calc(d.data() + pos + 4, static_cast<size_t>(len) + 4);
    const scl2::bytearray digest = scl2::crc32::hash(calc);
    for (size_t i = 0; i < 4; ++i) {
        if (std::to_integer<uint8_t>(digest[i]) != rd_u8(d, pos + 8 + len + i)) return false;
    }
    return true;
}

/// Ancillary chunks worth keeping around for inspection.
bool keep_as_ancillary(const char* tag) {
    static const char* const kept[] = {
        "gAMA", "cHRM", "sRGB", "iCCP", "sBIT", "bKGD", "pHYs",
        "tEXt", "zTXt", "iTXt", "tIME", "eXIf", "hIST", "sPLT",
    };
    for (const char* k : kept)
        if (std::strncmp(tag, k, 4) == 0) return true;
    return false;
}

// ── sample layout ────────────────────────────────────────────────────────

size_t channel_count(uint8_t color_type) {
    switch (color_type) {
    case 0: return 1;   // greyscale
    case 2: return 3;   // RGB
    case 3: return 1;   // palette index
    case 4: return 2;   // greyscale + alpha
    case 6: return 4;   // RGBA
    default: throw std::runtime_error("png: invalid colour type");
    }
}

bool depth_allowed(uint8_t color_type, uint8_t depth) {
    switch (color_type) {
    case 0: return depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16;
    case 2:
    case 4:
    case 6: return depth == 8 || depth == 16;
    case 3: return depth == 1 || depth == 2 || depth == 4 || depth == 8;
    default: return false;
    }
}

size_t row_byte_count(size_t width, size_t channels, uint8_t depth) {
    return (width * channels * depth + 7) / 8;
}

/// Bytes per pixel used by the scanline filters (at least 1).
size_t filter_bpp(size_t channels, uint8_t depth) {
    return std::max<size_t>(1, (channels * depth + 7) / 8);
}

/// Extract sample `index` of a row. For 16-bit samples only the high byte is
/// returned, because the pixel container is RGBA8.
uint32_t packed_sample(const uint8_t* row, uint8_t depth, size_t index) {
    switch (depth) {
    case 8:  return row[index];
    case 16: return row[index * 2];
    case 1:  return static_cast<uint32_t>((row[index >> 3] >> (7 - (index & 7))) & 0x01);
    case 2:  return static_cast<uint32_t>((row[index >> 2] >> (6 - 2 * (index & 3))) & 0x03);
    case 4:  return static_cast<uint32_t>((row[index >> 1] >> ((index & 1) ? 0 : 4)) & 0x0F);
    default: throw std::runtime_error("png: unsupported bit depth");
    }
}

/// Expand a low-bit-depth greyscale sample to the full 0..255 range.
uint8_t scale_sample(uint32_t v, uint8_t depth) {
    switch (depth) {
    case 1:  return static_cast<uint8_t>(v * 255u);
    case 2:  return static_cast<uint8_t>(v * 85u);
    case 4:  return static_cast<uint8_t>(v * 17u);
    default: return static_cast<uint8_t>(v);
    }
}

// ── scanline filters ─────────────────────────────────────────────────────

uint8_t paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = std::abs(p - a);
    const int pb = std::abs(p - b);
    const int pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return static_cast<uint8_t>(a);
    if (pb <= pc)             return static_cast<uint8_t>(b);
    return static_cast<uint8_t>(c);
}

/// Reverse one filter, in place, turning filtered bytes back into samples.
void unfilter_row(uint8_t ft, uint8_t* cur, const uint8_t* prior, size_t len, size_t bpp) {
    switch (ft) {
    case 0:  // None
        break;
    case 1:  // Sub
        for (size_t i = bpp; i < len; ++i) cur[i] = static_cast<uint8_t>(cur[i] + cur[i - bpp]);
        break;
    case 2:  // Up
        for (size_t i = 0; i < len; ++i) cur[i] = static_cast<uint8_t>(cur[i] + prior[i]);
        break;
    case 3:  // Average
        for (size_t i = 0; i < len; ++i) {
            const int left = i >= bpp ? cur[i - bpp] : 0;
            cur[i] = static_cast<uint8_t>(cur[i] + ((left + prior[i]) >> 1));
        }
        break;
    case 4:  // Paeth
        for (size_t i = 0; i < len; ++i) {
            const int a = i >= bpp ? cur[i - bpp] : 0;
            const int b = prior[i];
            const int c = i >= bpp ? prior[i - bpp] : 0;
            cur[i] = static_cast<uint8_t>(cur[i] + paeth(a, b, c));
        }
        break;
    default:
        throw std::runtime_error("png::decode: invalid scanline filter type");
    }
}

/// Apply one filter, producing the bytes to store in the stream.
void apply_filter(uint8_t ft, const uint8_t* cur, const uint8_t* prior,
                  size_t len, size_t bpp, uint8_t* out) {
    for (size_t i = 0; i < len; ++i) {
        const int a = i >= bpp ? cur[i - bpp] : 0;
        const int b = prior[i];
        const int c = i >= bpp ? prior[i - bpp] : 0;
        int v = 0;
        switch (ft) {
        case 0:  v = cur[i];                            break;
        case 1:  v = cur[i] - a;                        break;
        case 2:  v = cur[i] - b;                        break;
        case 3:  v = cur[i] - ((a + b) >> 1);           break;
        default: v = cur[i] - paeth(a, b, c);           break;
        }
        out[i] = static_cast<uint8_t>(v & 0xFF);
    }
}

/// Minimum sum of absolute differences: the classic heuristic for choosing
/// which filter to use for a row.
size_t filter_cost(const uint8_t* p, size_t len) {
    size_t sum = 0;
    for (size_t i = 0; i < len; ++i) sum += (p[i] < 128 ? p[i] : 256 - p[i]);
    return sum;
}

// ── Adam7 ────────────────────────────────────────────────────────────────

struct adam7_pass {
    size_t x0, y0, dx, dy;
};

constexpr adam7_pass adam7_passes[7] = {
    { 0, 0, 8, 8 }, { 4, 0, 8, 8 }, { 0, 4, 4, 8 }, { 2, 0, 4, 4 },
    { 0, 2, 2, 4 }, { 1, 0, 2, 2 }, { 0, 1, 1, 2 },
};

/// Exact number of bytes the inflated IDAT stream must contain: every scanline
/// of every pass, each preceded by its filter-type byte.
size_t expected_raw_size(size_t w, size_t h, size_t channels, uint8_t depth, bool interlaced) {
    auto pass_size = [&](size_t x0, size_t y0, size_t dx, size_t dy) -> size_t {
        if (x0 >= w || y0 >= h) return 0;
        const size_t pw = (w - x0 + dx - 1) / dx;
        const size_t ph = (h - y0 + dy - 1) / dy;
        if (pw == 0 || ph == 0) return 0;
        return ph * (1 + row_byte_count(pw, channels, depth));
    };

    if (!interlaced) return pass_size(0, 0, 1, 1);

    size_t total = 0;
    for (const adam7_pass& p : adam7_passes) total += pass_size(p.x0, p.y0, p.dx, p.dy);
    return total;
}

// ── palette helpers (encoding) ───────────────────────────────────────────

uint32_t pack_pixel(const rgba8& p) {
    return (static_cast<uint32_t>(p.r) << 24) | (static_cast<uint32_t>(p.g) << 16)
         | (static_cast<uint32_t>(p.b) << 8)  |  static_cast<uint32_t>(p.a);
}

/// Bit depth needed to address `colors` palette entries.
uint8_t palette_bit_depth(size_t colors) {
    if (colors <= 2)  return 1;
    if (colors <= 4)  return 2;
    if (colors <= 16) return 4;
    return 8;
}

/// Try to express every pixel with `palette`; fills `indices` on success.
bool map_to_palette(const bitmap<rgba8>& img, const std::vector<rgba8>& palette,
                    std::vector<uint8_t>& indices) {
    if (palette.empty() || palette.size() > 256) return false;

    std::unordered_map<uint32_t, uint8_t> lookup;
    lookup.reserve(palette.size() * 2);
    for (size_t i = 0; i < palette.size(); ++i) lookup.emplace(pack_pixel(palette[i]), static_cast<uint8_t>(i));

    indices.resize(img.pixelCount());
    for (size_t i = 0; i < indices.size(); ++i) {
        auto it = lookup.find(pack_pixel(img.data()[i]));
        if (it == lookup.end()) {
            indices.clear();
            return false;
        }
        indices[i] = it->second;
    }
    return true;
}

/// Build a palette from the image itself (first-seen order).
void build_palette(const bitmap<rgba8>& img, std::vector<rgba8>& palette,
                   std::vector<uint8_t>& indices) {
    std::unordered_map<uint32_t, uint8_t> lookup;
    palette.clear();
    indices.resize(img.pixelCount());
    for (size_t i = 0; i < indices.size(); ++i) {
        const rgba8 px = img.data()[i];
        const uint32_t key = pack_pixel(px);
        auto it = lookup.find(key);
        if (it != lookup.end()) {
            indices[i] = it->second;
            continue;
        }
        if (palette.size() >= 256)
            throw std::runtime_error("png::encode: more than 256 colours, palette output impossible");
        const uint8_t idx = static_cast<uint8_t>(palette.size());
        palette.push_back(px);
        lookup.emplace(key, idx);
        indices[i] = idx;
    }
}

/// Write one unfiltered scanline of a pass into `row`.
void build_row(const bitmap<rgba8>& img, const adam7_pass& p, size_t py, size_t pw,
               uint8_t color_type, uint8_t depth,
               const std::vector<uint8_t>& indices, std::vector<uint8_t>& row) {
    std::fill(row.begin(), row.end(), uint8_t{ 0 });

    const size_t per_byte = depth >= 8 ? 1 : (8 / depth);
    const uint8_t mask = depth >= 8 ? 0xFF : static_cast<uint8_t>((1u << depth) - 1);

    size_t sample = 0;
    auto put = [&](uint8_t v) {
        if (depth == 8) { row[sample++] = v; return; }
        const size_t byte_i = sample / per_byte;
        const size_t shift = 8 - depth * ((sample % per_byte) + 1);
        row[byte_i] = static_cast<uint8_t>(row[byte_i] | ((v & mask) << shift));
        ++sample;
    };

    const size_t oy = p.y0 + py * p.dy;
    const rgba8* src = img.row(oy).data();

    for (size_t px = 0; px < pw; ++px) {
        const size_t ox = p.x0 + px * p.dx;
        const rgba8 c = src[ox];
        switch (color_type) {
        case 0: put(c.r); break;
        case 2: put(c.r); put(c.g); put(c.b); break;
        case 3: put(indices[oy * img.width() + ox]); break;
        case 4: put(c.r); put(c.a); break;
        default: put(c.r); put(c.g); put(c.b); put(c.a); break;
        }
    }
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════
//  identification
// ═════════════════════════════════════════════════════════════════════════

bool png::matches(const scl2::bytearray& data) {
    // Never throws and never disturbs the caller's read cursor: the guard
    // restores it on every return path.
    auto guard = data.rp_guard();
    if (data.remaining() < magic_size) return false;
    const size_t p = data.tellr();
    for (size_t i = 0; i < magic_size; ++i) {
        if (std::to_integer<uint8_t>(data[p + i]) != magic[i]) return false;
    }
    return true;
}

// ═════════════════════════════════════════════════════════════════════════
//  decoding
// ═════════════════════════════════════════════════════════════════════════

png png::decode(const scl2::bytearray& data) {
    if (!matches(data))
        throw std::runtime_error("png::decode: not a PNG stream (bad signature)");

    png out;
    bool have_ihdr = false;
    bool idat_started = false;
    bool idat_closed = false;

    std::vector<uint8_t> plte;
    std::vector<uint8_t> trns;
    scl2::bytearray idat;

    size_t pos = magic_size;
    while (pos + 8 <= data.size()) {
        const uint32_t len = rd_u32(data, pos);
        if (pos + 12 + static_cast<uint64_t>(len) > data.size())
            throw std::runtime_error("png::decode: chunk extends past the end of the stream");

        char tag[4];
        for (int i = 0; i < 4; ++i) tag[i] = static_cast<char>(rd_u8(data, pos + 4 + i));
        const size_t body = pos + 8;

        if (!chunk_crc_ok(data, pos, len))
            throw std::runtime_error(std::string("png::decode: CRC mismatch in chunk ")
                                     + std::string(tag, 4));

        if (std::strncmp(tag, "IHDR", 4) == 0) {
            if (have_ihdr) throw std::runtime_error("png::decode: duplicate IHDR");
            if (len != 13) throw std::runtime_error("png::decode: IHDR must be 13 bytes");

            const uint32_t w = rd_u32(data, body);
            const uint32_t h = rd_u32(data, body + 4);
            const uint8_t depth = rd_u8(data, body + 8);
            const uint8_t color = rd_u8(data, body + 9);
            const uint8_t compression = rd_u8(data, body + 10);
            const uint8_t filter_method = rd_u8(data, body + 11);
            const uint8_t interlace = rd_u8(data, body + 12);

            if (w == 0 || h == 0)              throw std::runtime_error("png::decode: zero width or height");
            if (w > max_dimension || h > max_dimension)
                throw std::runtime_error("png::decode: dimensions exceed the PNG limit");
            if (compression != 0)              throw std::runtime_error("png::decode: unsupported compression method");
            if (filter_method != 0)            throw std::runtime_error("png::decode: unsupported filter method");
            if (interlace > 1)                 throw std::runtime_error("png::decode: unsupported interlace method");
            if (!depth_allowed(color, depth))  throw std::runtime_error("png::decode: illegal bit depth for colour type");

            out.m_info.width      = w;
            out.m_info.height     = h;
            out.m_info.bit_depth  = depth;
            out.m_info.color      = static_cast<color_type>(color);
            out.m_info.interlaced = (interlace == 1);
            have_ihdr = true;
        }
        else if (std::strncmp(tag, "PLTE", 4) == 0) {
            if (len == 0 || len % 3 != 0 || len > 768)
                throw std::runtime_error("png::decode: malformed PLTE chunk");
            plte.resize(len);
            std::memcpy(plte.data(), data.data() + body, len);
        }
        else if (std::strncmp(tag, "tRNS", 4) == 0) {
            trns.resize(len);
            if (len) std::memcpy(trns.data(), data.data() + body, len);
        }
        else if (std::strncmp(tag, "IDAT", 4) == 0) {
            if (!have_ihdr)  throw std::runtime_error("png::decode: IDAT before IHDR");
            if (idat_closed) throw std::runtime_error("png::decode: IDAT chunks are not consecutive");
            if (len) idat.append(data.data() + body, len);
            idat_started = true;
        }
        else if (std::strncmp(tag, "IEND", 4) == 0) {
            if (len != 0) throw std::runtime_error("png::decode: IEND must be empty");
            break;
        }
        else {
            // Any non-IDAT chunk closes the IDAT run.
            if (idat_started) idat_closed = true;
            if (keep_as_ancillary(tag)) {
                out.m_ancillary.push_back(chunk{
                    std::string(tag, 4),
                    len ? scl2::bytearray(data.data() + body, len) : scl2::bytearray{}
                });
            }
            else if ((static_cast<uint8_t>(tag[0]) & 0x20) == 0) {
                // Bit 5 of the first letter is set for ancillary chunks.
                throw std::runtime_error(std::string("png::decode: unknown critical chunk ")
                                         + std::string(tag, 4));
            }
        }

        pos += 12 + static_cast<size_t>(len);
    }

    if (!have_ihdr)       throw std::runtime_error("png::decode: no IHDR chunk");
    if (!idat_started)    throw std::runtime_error("png::decode: no IDAT chunk");

    const size_t   w        = out.m_info.width;
    const size_t   h        = out.m_info.height;
    const uint8_t  ct       = static_cast<uint8_t>(out.m_info.color);
    const uint8_t  depth    = out.m_info.bit_depth;
    const size_t   channels = channel_count(ct);

    // ── palette ──────────────────────────────────────────────────────
    if (ct == 3) {
        if (plte.empty())
            throw std::runtime_error("png::decode: palette image without a PLTE chunk");
        const size_t n = plte.size() / 3;
        out.m_palette.resize(n);
        for (size_t i = 0; i < n; ++i) {
            out.m_palette[i] = rgba8(plte[i * 3], plte[i * 3 + 1], plte[i * 3 + 2], 255);
        }
        // Fold palette transparency into the stored palette, so the entries are
        // exactly the pixels and a re-encode can match against them directly.
        for (size_t i = 0; i < n && i < trns.size(); ++i) out.m_palette[i].a = trns[i];
    }

    // ── transparency ─────────────────────────────────────────────────
    bool     grey_keyed = false;
    uint16_t grey_key   = 0;
    bool     rgb_keyed  = false;
    uint16_t rgb_key[3] = { 0, 0, 0 };

    if (!trns.empty()) {
        if (ct == 0 && trns.size() >= 2) {
            grey_keyed = true;
            grey_key = static_cast<uint16_t>((trns[0] << 8) | trns[1]);
        }
        else if (ct == 2 && trns.size() >= 6) {
            rgb_keyed = true;
            for (int i = 0; i < 3; ++i)
                rgb_key[i] = static_cast<uint16_t>((trns[i * 2] << 8) | trns[i * 2 + 1]);
        }
    }

    // ── reject absurd geometry, then inflate ─────────────────────────
    // A corrupt IHDR can claim huge dimensions; refuse before anything is
    // allocated. The size check below additionally requires the inflated
    // stream to really be that large, so a bogus header fails fast.
    if (static_cast<uint64_t>(w) * static_cast<uint64_t>(h) > max_pixels)
        throw std::runtime_error("png::decode: image too large to materialise (over 2^30 pixels)");

    const scl2::bytearray raw = scl2::zlib::decompress(idat);

    if (raw.size() < expected_raw_size(w, h, channels, depth, out.m_info.interlaced))
        throw std::runtime_error("png::decode: pixel data is truncated");

    // `resize` clears the buffer and arms the geometry; the spans returned by
    // bitmap::row() then let us fill it without a per-pixel access check.
    out.bitmap<rgba8>::resize(w, h);

    // ── scanline processing, per interlace pass ──────────────────────
    std::vector<uint8_t> cur, prior;
    size_t consumed = 0;

    auto process_pass = [&](const adam7_pass& p) {
        if (p.x0 >= w || p.y0 >= h) return;
        const size_t pw = (w - p.x0 + p.dx - 1) / p.dx;
        const size_t ph = (h - p.y0 + p.dy - 1) / p.dy;
        if (pw == 0 || ph == 0) return;

        const size_t rb  = row_byte_count(pw, channels, depth);
        const size_t bpp = filter_bpp(channels, depth);

        cur.assign(rb, 0);
        prior.assign(rb, 0);

        for (size_t py = 0; py < ph; ++py) {
            if (consumed + 1 + rb > raw.size())
                throw std::runtime_error("png::decode: pixel data is truncated");

            const uint8_t ft = rd_u8(raw, consumed);
            ++consumed;
            std::memcpy(cur.data(), raw.data() + consumed, rb);
            consumed += rb;

            unfilter_row(ft, cur.data(), prior.data(), rb, bpp);

            const size_t oy = p.y0 + py * p.dy;
            rgba8* dst = out.row(oy).data();

            for (size_t px = 0; px < pw; ++px) {
                const size_t ox = p.x0 + px * p.dx;
                switch (ct) {
                case 0: {
                    const uint32_t g = packed_sample(cur.data(), depth, px);
                    const uint8_t g8 = scale_sample(g, depth);
                    const uint8_t a = (grey_keyed && g == grey_key) ? 0 : 255;
                    dst[ox] = rgba8(g8, g8, g8, a);
                    break;
                }
                case 2: {
                    const uint32_t r = packed_sample(cur.data(), depth, px * 3 + 0);
                    const uint32_t g = packed_sample(cur.data(), depth, px * 3 + 1);
                    const uint32_t b = packed_sample(cur.data(), depth, px * 3 + 2);
                    const uint8_t a = (rgb_keyed && r == rgb_key[0] && g == rgb_key[1] && b == rgb_key[2])
                                          ? 0 : 255;
                    dst[ox] = rgba8(scale_sample(r, depth), scale_sample(g, depth),
                                    scale_sample(b, depth), a);
                    break;
                }
                case 3: {
                    const uint32_t idx = packed_sample(cur.data(), depth, px);
                    if (idx >= out.m_palette.size())
                        throw std::runtime_error("png::decode: palette index out of range");
                    dst[ox] = out.m_palette[idx];   // tRNS alpha already folded in
                    break;
                }
                case 4: {
                    const uint32_t g = packed_sample(cur.data(), depth, px * 2 + 0);
                    const uint32_t a = packed_sample(cur.data(), depth, px * 2 + 1);
                    const uint8_t g8 = scale_sample(g, depth);
                    dst[ox] = rgba8(g8, g8, g8, scale_sample(a, depth));
                    break;
                }
                default: {  // 6
                    dst[ox] = rgba8(packed_sample(cur.data(), depth, px * 4 + 0),
                                    packed_sample(cur.data(), depth, px * 4 + 1),
                                    packed_sample(cur.data(), depth, px * 4 + 2),
                                    packed_sample(cur.data(), depth, px * 4 + 3));
                    break;
                }
                }
            }

            prior.swap(cur);
        }
    };

    if (out.m_info.interlaced) {
        for (const adam7_pass& p : adam7_passes) process_pass(p);
    }
    else {
        process_pass(adam7_pass{ 0, 0, 1, 1 });
    }

    return out;
}

// ═════════════════════════════════════════════════════════════════════════
//  encoding
// ═════════════════════════════════════════════════════════════════════════

scl2::bytearray png::encode(const options& opt) const {
    const size_t w = width();
    const size_t h = height();
    if (w == 0 || h == 0)
        throw std::runtime_error("png::encode: refusing to write an empty image");
    if (w > max_dimension || h > max_dimension)
        throw std::runtime_error("png::encode: image exceeds the PNG dimension limit");

    auto all_opaque = [this] {
        for (const rgba8& p : data()) if (p.a != 255) return false;
        return true;
    };
    auto all_gray = [this] {
        for (const rgba8& p : data()) if (p.r != p.g || p.g != p.b) return false;
        return true;
    };

    uint8_t            out_color = 6;
    uint8_t            out_depth = 8;
    std::vector<rgba8> out_palette;
    std::vector<uint8_t> indices;

    switch (opt.format) {
    case mode::rgb:
        out_color = 2;
        break;
    case mode::rgba:
        out_color = 6;
        break;
    case mode::grayscale:
        if (!all_gray())
            throw std::runtime_error("png::encode: grayscale output requires R == G == B in every pixel");
        out_color = all_opaque() ? 0 : 4;
        break;
    case mode::palette:
        build_palette(*this, out_palette, indices);
        out_color = 3;
        out_depth = palette_bit_depth(out_palette.size());
        break;

    case mode::auto_preserve:
    default:
        if (m_info.color == color_type::palette && !m_palette.empty()
            && map_to_palette(*this, m_palette, indices)) {
            // Still representable by the palette we decoded: keep it, which
            // also preserves the original bit depth.
            out_palette = m_palette;
            out_color = 3;
            out_depth = palette_bit_depth(out_palette.size());
        }
        else if ((m_info.color == color_type::grayscale
               || m_info.color == color_type::grayscale_alpha) && all_gray()) {
            out_color = all_opaque() ? 0 : 4;
        }
        else if (all_opaque()) {
            out_color = 2;
        }
        else {
            out_color = 6;
        }
        break;
    }

    const size_t channels = channel_count(out_color);

    // ── raw scanlines, filtered ──────────────────────────────────────
    std::vector<uint8_t> raw;
    raw.reserve((row_byte_count(w, channels, out_depth) + 1) * h);

    std::vector<uint8_t> row, prior, filtered, best;
    const adam7_pass single{ 0, 0, 1, 1 };
    const size_t pass_count = opt.interlace ? 7 : 1;

    for (size_t pi = 0; pi < pass_count; ++pi) {
        const adam7_pass p = opt.interlace ? adam7_passes[pi] : single;
        if (p.x0 >= w || p.y0 >= h) continue;
        const size_t pw = (w - p.x0 + p.dx - 1) / p.dx;
        const size_t ph = (h - p.y0 + p.dy - 1) / p.dy;
        if (pw == 0 || ph == 0) continue;

        const size_t rb  = row_byte_count(pw, channels, out_depth);
        const size_t bpp = filter_bpp(channels, out_depth);

        row.assign(rb, 0);
        prior.assign(rb, 0);
        filtered.resize(rb);
        best.resize(rb);

        for (size_t py = 0; py < ph; ++py) {
            build_row(*this, p, py, pw, out_color, out_depth, indices, row);

            uint8_t chosen;
            if (opt.filter >= 0) {
                chosen = static_cast<uint8_t>(std::min(opt.filter, 4));
                apply_filter(chosen, row.data(), prior.data(), rb, bpp, best.data());
            }
            else {
                size_t best_cost = static_cast<size_t>(-1);
                chosen = 0;
                for (uint8_t ft = 0; ft < 5; ++ft) {
                    apply_filter(ft, row.data(), prior.data(), rb, bpp, filtered.data());
                    const size_t cost = filter_cost(filtered.data(), rb);
                    if (cost < best_cost) {
                        best_cost = cost;
                        chosen = ft;
                        std::memcpy(best.data(), filtered.data(), rb);
                    }
                }
            }

            raw.push_back(chosen);
            raw.insert(raw.end(), best.begin(), best.end());

            prior.swap(row);
        }
    }

    // ── chunks ───────────────────────────────────────────────────────
    const scl2::bytearray compressed = scl2::zlib::compress(
        raw.empty() ? scl2::bytearray{} : scl2::bytearray(raw.data(), raw.size()));

    scl2::bytearray ihdr;
    push_u32(ihdr, static_cast<uint32_t>(w));
    push_u32(ihdr, static_cast<uint32_t>(h));
    push_u8(ihdr, out_depth);
    push_u8(ihdr, out_color);
    push_u8(ihdr, 0);   // compression method: deflate
    push_u8(ihdr, 0);   // filter method: adaptive
    push_u8(ihdr, opt.interlace ? 1 : 0);

    scl2::bytearray file;
    for (size_t i = 0; i < magic_size; ++i) push_u8(file, magic[i]);
    append_chunk(file, "IHDR", ihdr);

    if (out_color == 3) {
        scl2::bytearray plte;
        for (const rgba8& c : out_palette) {
            push_u8(plte, c.r);
            push_u8(plte, c.g);
            push_u8(plte, c.b);
        }
        append_chunk(file, "PLTE", plte);

        // tRNS only has to reach as far as the last entry that is not opaque.
        size_t last_transparent = 0;
        for (size_t i = 0; i < out_palette.size(); ++i)
            if (out_palette[i].a != 255) last_transparent = i + 1;
        if (last_transparent > 0) {
            scl2::bytearray trns;
            for (size_t i = 0; i < last_transparent; ++i) push_u8(trns, out_palette[i].a);
            append_chunk(file, "tRNS", trns);
        }
    }

    append_chunk(file, "IDAT", compressed);
    append_chunk(file, "IEND", scl2::bytearray{});
    return file;
}

} // namespace scl2
