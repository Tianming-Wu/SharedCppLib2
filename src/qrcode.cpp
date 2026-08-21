#include "qrcode.hpp"

#include <cmath>
#include <limits>

namespace scl2::qrcode {

size_t capacityForVersion(Version v, ErrorCorrectionLevel l)
{
    // [1-40] [{L = 0, M = 1, Q = 2, H = 3}]
    // So the table is indexed by [version-1][ec_level-1].
    static constexpr size_t capacity_table[40][4] = {
        {   19,   16,   13,    9 }, {   34,   28,   22,   16 }, {   55,   44,   34,   26 }, {   80,   64,   48,   36 },
        {  108,   86,   62,   46 }, {  136,  108,   76,   60 }, {  156,  124,   88,   66 }, {  194,  154,  110,   86 },
        {  232,  182,  132,  100 }, {  274,  216,  154,  122 }, {  324,  254,  180,  140 }, {  370,  290,  206,  158 },
        {  428,  334,  244,  180 }, {  461,  365,  261,  197 }, {  523,  415,  295,  223 }, {  589,  453,  325,  253 },
        {  647,  507,  367,  283 }, {  721,  563,  397,  313 }, {  795,  627,  445,  341 }, {  861,  669,  485,  385 },
        {  932,  714,  512,  406 }, { 1006,  782,  568,  442 }, { 1094,  860,  614,  464 }, { 1174,  914,  664,  514 },
        { 1276, 1000,  718,  538 }, { 1370, 1062,  754,  596 }, { 1468, 1128,  808,  628 }, { 1531, 1193,  871,  661 },
        { 1631, 1267,  911,  701 }, { 1735, 1373,  985,  745 }, { 1843, 1455, 1033,  793 }, { 1955, 1541, 1115,  845 },
        { 2071, 1631, 1171,  901 }, { 2191, 1725, 1231,  961 }, { 2306, 1812, 1286,  986 }, { 2434, 1914, 1354, 1054 },
        { 2566, 1992, 1426, 1096 }, { 2702, 2102, 1502, 1142 }, { 2812, 2216, 1582, 1222 }, { 2956, 2334, 1666, 1276 }
    };
    return capacity_table[static_cast<size_t>(v) - 1][static_cast<size_t>(l) - 1];
}

namespace {

// QR alphanumeric alphabet value: 0-9 -> 0-9, A-Z -> 10-35,
// then space $ % * + - . / : -> 36..44.
int alnum_value(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    switch (c) {
        case ' ': return 36; case '$': return 37; case '%': return 38;
        case '*': return 39; case '+': return 40; case '-': return 41;
        case '.': return 42; case '/': return 43; case ':': return 44;
        default: return -1;
    }
}

bool is_alphanumeric_char(unsigned char c) { return alnum_value(c) >= 0; }

// MSB-first bit writer used while assembling the data bit stream.
struct bit_writer {
    std::vector<uint8_t> bytes;
    size_t bit_count = 0;

    void write_bits(uint32_t value, int n) {
        for (int i = n - 1; i >= 0; --i) {
            if (bit_count % 8 == 0) bytes.push_back(0);
            if ((value >> i) & 1U)
                bytes.back() |= static_cast<uint8_t>(1U << (7 - (bit_count % 8)));
            ++bit_count;
        }
    }
    void pad_to_byte() {
        while (bit_count % 8 != 0) write_bits(0, 1);
    }
};

void write_numeric(bit_writer& bw, const std::string& data) {
    const size_t n = data.size();
    size_t i = 0;
    while (i + 3 <= n) {
        const uint32_t v = static_cast<uint32_t>(data[i] - '0') * 100
                         + static_cast<uint32_t>(data[i + 1] - '0') * 10
                         + static_cast<uint32_t>(data[i + 2] - '0');
        bw.write_bits(v, 10); // 3 digits -> 10 bits
        i += 3;
    }
    const size_t rem = n - i;
    if (rem == 2)
        bw.write_bits(static_cast<uint32_t>(data[i] - '0') * 10 + static_cast<uint32_t>(data[i + 1] - '0'), 7);
    else if (rem == 1)
        bw.write_bits(static_cast<uint32_t>(data[i] - '0'), 4);
}

void write_alphanumeric(bit_writer& bw, const std::string& data) {
    const size_t n = data.size();
    size_t i = 0;
    while (i + 2 <= n) {
        const uint32_t v = static_cast<uint32_t>(alnum_value(static_cast<unsigned char>(data[i])) * 45)
                         + static_cast<uint32_t>(alnum_value(static_cast<unsigned char>(data[i + 1])));
        bw.write_bits(v, 11); // 2 chars -> 11 bits
        i += 2;
    }
    if (i < n)
        bw.write_bits(static_cast<uint32_t>(alnum_value(static_cast<unsigned char>(data[i]))), 6);
}

void write_byte(bit_writer& bw, const std::string& data) {
    for (unsigned char c : data) bw.write_bits(c, 8);
}

// Raw payload bit length of `data` in a mode (excludes indicator/count/terminator).
size_t raw_data_bits(const std::string& data, Mode mode) {
    const size_t n = data.size();
    switch (mode) {
        case Mode::numeric:      return (n / 3) * 10 + (n % 3 == 2 ? 7 : n % 3 == 1 ? 4 : 0);
        case Mode::alphanumeric: return (n / 2) * 11 + (n % 2 == 1 ? 6 : 0);
        case Mode::byte:         return n * 8;
        default:                 return n * 8; // kanji not supported yet
    }
}

// Alignment pattern center coordinates per version (index 0 = count, then the
// centers). ISO/IEC 18004 table. Version 1 has no alignment patterns.
static constexpr uint8_t ALIGN_POSITIONS[41][8] = {
    { 0,  0,  0,  0,  0,  0,  0,  0 },                          // v1
    { 2,  6, 18,  0,  0,  0,  0,  0 },                          // v2
    { 2,  6, 22,  0,  0,  0,  0,  0 },                          // v3
    { 2,  6, 26,  0,  0,  0,  0,  0 },                          // v4
    { 2,  6, 30,  0,  0,  0,  0,  0 },                          // v5
    { 2,  6, 34,  0,  0,  0,  0,  0 },                          // v6
    { 3,  6, 22, 38,  0,  0,  0,  0 },                          // v7
    { 3,  6, 24, 42,  0,  0,  0,  0 },                          // v8
    { 3,  6, 26, 46,  0,  0,  0,  0 },                          // v9
    { 3,  6, 28, 50,  0,  0,  0,  0 },                          // v10
    { 3,  6, 30, 54,  0,  0,  0,  0 },                          // v11
    { 3,  6, 32, 58,  0,  0,  0,  0 },                          // v12
    { 3,  6, 34, 62,  0,  0,  0,  0 },                          // v13
    { 4,  6, 26, 46, 66,  0,  0,  0 },                          // v14
    { 4,  6, 26, 48, 70,  0,  0,  0 },                          // v15
    { 4,  6, 26, 50, 74,  0,  0,  0 },                          // v16
    { 4,  6, 30, 54, 78,  0,  0,  0 },                          // v17
    { 4,  6, 30, 56, 82,  0,  0,  0 },                          // v18
    { 4,  6, 30, 58, 86,  0,  0,  0 },                          // v19
    { 4,  6, 34, 62, 90,  0,  0,  0 },                          // v20
    { 5,  6, 28, 50, 72, 94,  0,  0 },                          // v21
    { 5,  6, 26, 50, 74, 98,  0,  0 },                          // v22
    { 5,  6, 30, 54, 78,102,  0,  0 },                          // v23
    { 5,  6, 28, 54, 80,106,  0,  0 },                          // v24
    { 5,  6, 32, 58, 84,110,  0,  0 },                          // v25
    { 5,  6, 30, 58, 86,114,  0,  0 },                          // v26
    { 5,  6, 34, 62, 90,118,  0,  0 },                          // v27
    { 6,  6, 26, 50, 74, 98,122,  0 },                          // v28
    { 6,  6, 30, 54, 78,102,126,  0 },                          // v29
    { 6,  6, 26, 52, 78,104,130,  0 },                          // v30
    { 6,  6, 30, 56, 82,108,134,  0 },                          // v31
    { 6,  6, 34, 60, 86,112,138,  0 },                          // v32
    { 6,  6, 30, 58, 86,114,142,  0 },                          // v33
    { 6,  6, 34, 62, 90,118,146,  0 },                          // v34
    { 7,  6, 30, 54, 78,102,126,150 },                          // v35
    { 7,  6, 24, 50, 76,102,128,154 },                          // v36
    { 7,  6, 28, 54, 80,106,132,158 },                          // v37
    { 7,  6, 32, 58, 84,110,136,162 },                          // v38
    { 7,  6, 26, 54, 82,110,138,166 },                          // v39
    { 7,  6, 30, 58, 86,114,142,170 },                          // v40
};

} // namespace

Mode detect_mode(const std::string& data)
{
    if (data.empty()) return Mode::byte;
    bool numeric_ok = true;
    bool alnum_ok = true;
    for (char c : data) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc < '0' || uc > '9') numeric_ok = false;
        if (!is_alphanumeric_char(uc)) alnum_ok = false;
        if (!numeric_ok && !alnum_ok) break; // one bad char rules out both modes
    }
    if (numeric_ok) return Mode::numeric;
    if (alnum_ok) return Mode::alphanumeric;
    return Mode::byte;
}

// ======================== public API ========================

scl2::bitmap_1c encoder::generate(const std::string& data, const options& opt)
{
    auto m = make_matrix(data, opt);
    const int qz = opt.quiet_zone;
    if (qz <= 0) return m;

    const size_t n = m.width();
    const size_t size = n + 2 * static_cast<size_t>(qz);
    scl2::bitmap_1c out(size, size); // default all-white
    for (size_t y = 0; y < n; ++y)
        for (size_t x = 0; x < n; ++x)
            out.setPixel(qz + x, qz + y, m.getPixel(x, y));
    return out;
}

scl2::bitmap_1c encoder::make_matrix(const std::string& data, const options& opt)
{
    const Mode mode = opt.mode == Mode::automatic ? detect_mode(data) : opt.mode;
    Version v = opt.version == Version::vauto
        ? select_version(data, mode, opt.ec_level)
        : opt.version;
    const size_t n = sizeForVersion(v);
    scl2::bitmap_1c m(n, n);

    place_function_patterns(m, v);

    // data codewords -> final (interleaved) codeword sequence
    const auto data_codewords = encode_data(data, mode, opt.ec_level, v);
    const auto codewords = build_codewords(data_codewords, v, opt.ec_level);
    place_data(m, codewords);

    // pick and apply a mask, then write the format info that records it
    const int mask = opt.mask >= 0 ? opt.mask : select_mask(m);
    apply_mask(m, mask);
    place_format_info(m, opt.ec_level, mask);

    // version info for v >= 7
    if (v >= Version::v7) place_version_info(m, v);

    return m;
}

Version encoder::select_version(const std::string& data, Mode m, ErrorCorrectionLevel l)
{
    const size_t data_bits = raw_data_bits(data, m);
    for (int v = 1; v <= 40; ++v) {
        const size_t cap_bits = capacityForVersion(static_cast<Version>(v), l) * 8;
        const size_t needed = 4 + static_cast<size_t>(char_count_bits(m, static_cast<Version>(v))) + data_bits;
        if (needed <= cap_bits) return static_cast<Version>(v);
    }
    throw std::runtime_error("qrcode: data too long for any version");
}

// ======================== Phase B: function patterns ========================

void encoder::place_function_patterns(scl2::bitmap_1c& m, Version version)
{
    const size_t n = sizeForVersion(version);
    m.clear(); // all white; separators stay white naturally

    // three finder patterns at the corners
    place_finder(m, 0, 0);
    place_finder(m, n - 7, 0);
    place_finder(m, 0, n - 7);

    // timing patterns on row/column 6
    place_timing(m);

    // dark module
    place_dark_module(m, version);

    // alignment patterns for v >= 2 (table is indexed by version - 1)
    const size_t v = static_cast<size_t>(version);
    if (v >= 2) {
        const size_t count = ALIGN_POSITIONS[v - 1][0];
        for (size_t i = 0; i < count; ++i) {
            const size_t r = ALIGN_POSITIONS[v - 1][i + 1];
            for (size_t j = 0; j < count; ++j) {
                const size_t c = ALIGN_POSITIONS[v - 1][j + 1];
                // skip the three finder corners
                if (r == 6 && c == 6) continue;
                if (r == 6 && c == n - 7) continue;
                if (r == n - 7 && c == 6) continue;
                place_alignment(m, c, r);
            }
        }
    }
}

void encoder::place_finder(scl2::bitmap_1c& m, size_t x, size_t y)
{
    for (size_t j = 0; j < 7; ++j)
        for (size_t i = 0; i < 7; ++i)
            m.setPixel(x + i, y + j, finder_pattern_7.get(i, j));
}

void encoder::place_alignment(scl2::bitmap_1c& m, size_t cx, size_t cy)
{
    for (size_t j = 0; j < 5; ++j)
        for (size_t i = 0; i < 5; ++i)
            m.setPixel(cx - 2 + i, cy - 2 + j, alignment_pattern_5.get(i, j));
}

void encoder::place_timing(scl2::bitmap_1c& m)
{
    const size_t n = m.width();
    for (size_t i = 8; i <= n - 9; ++i) {
        const bool v = (i % 2 == 0); // alternating, dark first
        m.setPixel(i, 6, v);
        m.setPixel(6, i, v);
    }
}

void encoder::place_dark_module(scl2::bitmap_1c& m, Version version)
{
    m.setPixel(8, sizeForVersion(version) - 8, true);
}

// ======================== Phase B: TODO ========================

void encoder::place_format_info(scl2::bitmap_1c& m, ErrorCorrectionLevel l, int mask)
{
    const size_t n = m.width();

    // 2-bit ECC format indicator: L=01 M=00 Q=11 H=10 (note the odd mapping).
    int ec_bits = 0;
    switch (l) {
        case ErrorCorrectionLevel::L: ec_bits = 0b01; break;
        case ErrorCorrectionLevel::M: ec_bits = 0b00; break;
        case ErrorCorrectionLevel::Q: ec_bits = 0b11; break;
        case ErrorCorrectionLevel::H: ec_bits = 0b10; break;
    }

    // 5 data bits + 10-bit BCH (generator 0x537), then XOR the fixed mask 0x5412.
    const uint32_t data = (static_cast<uint32_t>(ec_bits) << 3) | (static_cast<uint32_t>(mask) & 0x7u);
    uint32_t rem = data;
    for (int i = 0; i < 10; ++i)
        rem = (rem << 1) ^ (((rem >> 9) & 1u) ? 0x537u : 0u);
    const uint32_t bits = ((data << 10) | rem) ^ 0x5412u; // 15 bits

    auto set_bit = [&](size_t x, size_t y, int i) {
        m.setPixel(x, y, ((bits >> i) & 1u) != 0);
    };

    // copy 1: around the top-left finder
    for (int i = 0; i <= 5; i++) set_bit(8, static_cast<size_t>(i), i);
    set_bit(8, 7, 6);
    set_bit(8, 8, 7);
    set_bit(7, 8, 8);
    for (int i = 9; i < 15; i++) set_bit(static_cast<size_t>(14 - i), 8, i);

    // copy 2: beside the top-right / above the bottom-left finders
    for (int i = 0; i < 8; i++) set_bit(n - 1 - static_cast<size_t>(i), 8, i);
    for (int i = 8; i < 15; i++) set_bit(8, n - 15 + static_cast<size_t>(i), i);
}

void encoder::place_version_info(scl2::bitmap_1c& m, Version version)
{
    const size_t n = m.width();

    // 6-bit version + 12-bit BCH (generator 0x1F25) = 18 bits.
    const uint32_t data = static_cast<uint32_t>(version);
    uint32_t rem = data;
    for (int i = 0; i < 12; ++i)
        rem = (rem << 1) ^ (((rem >> 11) & 1u) ? 0x1F25u : 0u);
    const uint32_t bits = (data << 12) | rem;

    // Written as a 3x6 block, twice: near the top-left and top-right finders.
    for (int i = 0; i < 18; ++i) {
        const bool bit = ((bits >> i) & 1u) != 0;
        const size_t a = n - 11 + static_cast<size_t>(i % 3);
        const size_t b = static_cast<size_t>(i / 3);
        m.setPixel(a, b, bit); // copy 1 (below the top-left finder)
        m.setPixel(b, a, bit); // copy 2 (left of the top-right finder)
    }
}

void encoder::place_data(scl2::bitmap_1c& m, const scl2::bytearray& codewords)
{
    const size_t n = m.width();
    const size_t total_bits = codewords.size() * 8;

    // Zig-zag scan: start at the bottom-right corner, walk two columns at a
    // time vertically (up then down), skipping function modules. Column 6 is
    // the timing column, so when we reach it we drop the left column of the
    // pair.
    size_t bit_index = 0;
    int inc = -1;                       // vertical direction (up first)
    int row = static_cast<int>(n) - 1;  // start at the bottom
    for (int col = static_cast<int>(n) - 1; col > 0; col -= 2) {
        if (col == 6) col--;            // skip the timing column
        while (true) {
            for (int c = 0; c < 2; ++c) {
                const size_t x = static_cast<size_t>(col - c);
                const size_t y = static_cast<size_t>(row);
                if (!is_function_module(x, y, n)) {
                    const bool bit = bit_index < total_bits
                        && (((static_cast<unsigned char>(codewords[bit_index >> 3]) >> (7 - (bit_index & 7))) & 1u) != 0);
                    m.setPixel(x, y, bit);
                    ++bit_index;
                }
            }
            row += inc;
            if (row < 0 || row >= static_cast<int>(n)) {
                row -= inc;
                inc = -inc;             // reverse vertical direction
                break;
            }
        }
    }
}

void encoder::apply_mask(scl2::bitmap_1c& m, int mask)
{
    const size_t n = m.width();
    for (size_t y = 0; y < n; ++y)
        for (size_t x = 0; x < n; ++x) {
            if (is_function_module(x, y, n)) continue;
            if (mask_condition(mask, x, y))
                m.setPixel(x, y, !m.getPixel(x, y));
        }
}

int encoder::select_mask(scl2::bitmap_1c& m)
{
    int best = 0;
    int best_penalty = std::numeric_limits<int>::max();
    for (int mask = 0; mask < 8; ++mask) {
        auto copy = m;
        apply_mask(copy, mask);
        const int p = mask_penalty(copy);
        if (p < best_penalty) { best_penalty = p; best = mask; }
    }
    return best;
}

bool encoder::is_function_module(size_t x, size_t y, size_t n)
{
    // timing patterns (row/col 6)
    if (x == 6 || y == 6) return true;
    // finder + separator (8x8 zones at three corners)
    if (x < 8 && y < 8) return true;
    if (x >= n - 8 && y < 8) return true;
    if (x < 8 && y >= n - 8) return true;
    // format info copy 1 (beside the top-left finder)
    if (x == 8 && (y <= 5 || y == 7 || y == 8)) return true;
    if (y == 8 && x <= 7) return true;
    // format info copy 2 (beside the top-right / above the bottom-left finder)
    if (y == 8 && x >= n - 8) return true;
    if (x == 8 && y >= n - 7) return true;
    // dark module at (8, n-8)
    if (x == 8 && y == n - 8) return true;
    // alignment patterns (5x5, v >= 2)
    if (is_alignment_module(x, y, n)) return true;
    // version info reserved area (v >= 7, n >= 45): 3x6 at two corners
    if (n >= 45 && ((x < 6 && y >= n - 11) || (y < 6 && x >= n - 11))) return true;
    return false;
}

bool encoder::is_alignment_module(size_t x, size_t y, size_t n)
{
    const size_t v = (n - 17) / 4;
    if (v < 2) return false;
    const size_t count = ALIGN_POSITIONS[v - 1][0];
    for (size_t i = 0; i < count; ++i) {
        const size_t r = ALIGN_POSITIONS[v - 1][i + 1];
        for (size_t j = 0; j < count; ++j) {
            const size_t c = ALIGN_POSITIONS[v - 1][j + 1];
            if (r == 6 && c == 6) continue;
            if (r == 6 && c == n - 7) continue;
            if (r == n - 7 && c == 6) continue;
            if (x + 2 >= c && x <= c + 2 && y + 2 >= r && y <= r + 2)
                return true;
        }
    }
    return false;
}

bool encoder::mask_condition(int mask, size_t x, size_t y)
{
    switch (mask) {
        case 0: return (x + y) % 2 == 0;
        case 1: return y % 2 == 0;
        case 2: return x % 3 == 0;
        case 3: return (x + y) % 3 == 0;
        case 4: return (x / 3 + y / 2) % 2 == 0;
        case 5: return (x * y) % 2 + (x * y) % 3 == 0;
        case 6: return ((x * y) % 2 + (x * y) % 3) % 2 == 0;
        case 7: return ((x + y) % 2 + (x * y) % 3) % 2 == 0;
        default: return false;
    }
}

int encoder::mask_penalty(const scl2::bitmap_1c& m)
{
    const size_t n = m.width();
    int penalty = 0;
    auto px = [&](size_t x, size_t y) -> bool { return m.getPixel(x, y); };

    // N1: runs of >= 5 same-colored modules in each row and column
    for (size_t y = 0; y < n; ++y) {
        size_t run = 1;
        for (size_t x = 1; x < n; ++x) {
            if (px(x, y) == px(x - 1, y)) ++run;
            else {
                if (run >= 5) penalty += 3 + static_cast<int>(run - 5);
                run = 1;
            }
        }
        if (run >= 5) penalty += 3 + static_cast<int>(run - 5);
    }
    for (size_t x = 0; x < n; ++x) {
        size_t run = 1;
        for (size_t y = 1; y < n; ++y) {
            if (px(x, y) == px(x, y - 1)) ++run;
            else {
                if (run >= 5) penalty += 3 + static_cast<int>(run - 5);
                run = 1;
            }
        }
        if (run >= 5) penalty += 3 + static_cast<int>(run - 5);
    }

    // N2: 2x2 blocks of the same color (overlapping)
    for (size_t y = 0; y + 1 < n; ++y)
        for (size_t x = 0; x + 1 < n; ++x) {
            const bool c = px(x, y);
            if (px(x + 1, y) == c && px(x, y + 1) == c && px(x + 1, y + 1) == c)
                penalty += 3;
        }

    // N3: finder-like pattern 1:1:3:1:1 (1011101) with 0000 before or after
    for (size_t y = 0; y < n; ++y)
        for (size_t x = 0; x + 7 < n; ++x) {
            if (px(x, y) && !px(x + 1, y) && px(x + 2, y) && px(x + 3, y)
                && px(x + 4, y) && !px(x + 5, y) && px(x + 6, y)) {
                const bool before = x >= 4 && !px(x - 1, y) && !px(x - 2, y) && !px(x - 3, y) && !px(x - 4, y);
                const bool after  = x + 10 < n && !px(x + 7, y) && !px(x + 8, y) && !px(x + 9, y) && !px(x + 10, y);
                if (before || after) penalty += 40;
            }
        }
    for (size_t x = 0; x < n; ++x)
        for (size_t y = 0; y + 7 < n; ++y) {
            if (px(x, y) && !px(x, y + 1) && px(x, y + 2) && px(x, y + 3)
                && px(x, y + 4) && !px(x, y + 5) && px(x, y + 6)) {
                const bool before = y >= 4 && !px(x, y - 1) && !px(x, y - 2) && !px(x, y - 3) && !px(x, y - 4);
                const bool after  = y + 10 < n && !px(x, y + 7) && !px(x, y + 8) && !px(x, y + 9) && !px(x, y + 10);
                if (before || after) penalty += 40;
            }
        }

    // N4: proportion of dark modules deviating from 50%, every 5% = 10 points
    size_t dark = 0;
    for (size_t y = 0; y < n; ++y)
        for (size_t x = 0; x < n; ++x)
            if (px(x, y)) ++dark;
    const double proportion = static_cast<double>(dark) / static_cast<double>(n * n);
    penalty += static_cast<int>(std::abs(proportion - 0.5) / 0.05) * 10;

    return penalty;
}

// ======================== Phase A: data encoding (TODO) ========================

scl2::bytearray encoder::encode_data(const std::string& data, Mode mode,
                                     ErrorCorrectionLevel ec_level, Version version)
{
    const size_t capacity_bytes = capacityForVersion(version, ec_level);
    const size_t capacity_bits = capacity_bytes * 8;

    bit_writer bw;
    bw.write_bits(static_cast<uint32_t>(mode), 4); // mode indicator
    bw.write_bits(static_cast<uint32_t>(data.size()), char_count_bits(mode, version));

    switch (mode) {
        case Mode::numeric:      write_numeric(bw, data);      break;
        case Mode::alphanumeric: write_alphanumeric(bw, data); break;
        case Mode::byte:         write_byte(bw, data);         break;
        default:
            throw std::runtime_error("qrcode: unsupported encoding mode");
    }

    // terminator: up to 4 zero bits, or fewer if the capacity is reached
    const size_t remaining = capacity_bits - bw.bit_count;
    const size_t term = std::min<size_t>(4, remaining);
    for (size_t i = 0; i < term; ++i) bw.write_bits(0, 1);

    bw.pad_to_byte(); // zero-pad the last byte

    // alternating 0xEC / 0x11 padding up to the data capacity
    while (bw.bytes.size() < capacity_bytes)
        bw.bytes.push_back((bw.bytes.size() % 2 == 0) ? 0xEC : 0x11);

    scl2::bytearray out(capacity_bytes, std::byte{0});
    for (size_t i = 0; i < capacity_bytes; ++i) out[i] = std::byte{bw.bytes[i]};
    return out;
}

scl2::bytearray encoder::reed_solomon(const scl2::bytearray& data, size_t ec_count)
{
    // Reed-Solomon over GF(2^8) — see xmath-reedsolomon.hpp for the math.
    return scl2::xmath::reedsolomon::encode(data, ec_count);
}

scl2::bytearray encoder::build_codewords(const scl2::bytearray& data_codewords,
                                         Version version, ErrorCorrectionLevel ec_level)
{
    const size_t ver = static_cast<size_t>(version);      // 1..40
    const size_t ec  = static_cast<size_t>(ec_level) - 1; // L=0 M=1 Q=2 H=3

    // ECC codewords per block, indexed [ecc][version-1] (ISO/IEC 18004).
    static constexpr uint8_t ECC_PER_BLOCK[4][40] = {
        {  7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28,
          28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 },
        { 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26,
          26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28 },
        { 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30,
          28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 },
        { 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28,
          30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 },
    };

    // Number of blocks, indexed [ecc][version-1].
    static constexpr uint8_t NUM_BLOCKS[4][40] = {
        {  1,  1,  1,  1,  1,  2,  2,  2,  2,  4,  4,  4,  4,  4,  6,  6,  6,  6,  7,  8,
           8,  9,  9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25 },
        {  1,  1,  1,  2,  2,  4,  4,  4,  5,  5,  5,  8,  9,  9, 10, 10, 11, 13, 14, 16,
          17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49 },
        {  1,  1,  2,  2,  4,  4,  6,  6,  8,  8,  8, 10, 12, 16, 12, 17, 16, 18, 21, 20,
          23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68 },
        {  1,  1,  2,  4,  4,  4,  5,  6,  8,  8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25,
          25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81 },
    };

    const size_t numBlocks   = NUM_BLOCKS[ec][ver - 1];
    const size_t eccPerBlock = ECC_PER_BLOCK[ec][ver - 1];
    const size_t totalData   = data_codewords.size();

    // Short blocks hold shortBlockTotal codewords (data + ECC); long blocks one
    // more. totalRaw = totalData + numBlocks * eccPerBlock.
    const size_t totalRaw      = totalData + numBlocks * eccPerBlock;
    const size_t shortDataLen  = totalRaw / numBlocks - eccPerBlock;
    const size_t numLongBlocks = totalRaw % numBlocks;
    const size_t numShortBlocks = numBlocks - numLongBlocks;
    const size_t longDataLen   = shortDataLen + 1;

    // 1) split the data codewords into blocks
    std::vector<scl2::bytearray> blocks(numBlocks);
    size_t pos = 0;
    for (size_t b = 0; b < numBlocks; ++b) {
        const size_t len = (b < numShortBlocks) ? shortDataLen : longDataLen;
        blocks[b].resize(len, std::byte{0});
        for (size_t k = 0; k < len; ++k) blocks[b][k] = data_codewords[pos++];
    }

    // 2) Reed-Solomon ECC for each block
    std::vector<scl2::bytearray> eccs(numBlocks);
    for (size_t b = 0; b < numBlocks; ++b)
        eccs[b] = reed_solomon(blocks[b], eccPerBlock);

    // 3) interleave: data codewords column-by-column, then ECC codewords
    std::vector<uint8_t> result;
    result.reserve(totalRaw);
    for (size_t k = 0; k < longDataLen; ++k)
        for (size_t b = 0; b < numBlocks; ++b)
            if (k < blocks[b].size())
                result.push_back(static_cast<uint8_t>(blocks[b][k]));
    for (size_t k = 0; k < eccPerBlock; ++k)
        for (size_t b = 0; b < numBlocks; ++b)
            result.push_back(static_cast<uint8_t>(eccs[b][k]));

    scl2::bytearray out(result.size(), std::byte{0});
    for (size_t i = 0; i < result.size(); ++i) out[i] = std::byte{result[i]};
    return out;
}

int encoder::char_count_bits(Mode mode, Version version)
{
    const int v = static_cast<int>(version);
    switch (mode) {
        case Mode::numeric:      return v <= 9 ? 10 : (v <= 26 ? 12 : 14);
        case Mode::alphanumeric: return v <= 9 ? 9  : (v <= 26 ? 11 : 13);
        case Mode::byte:         return v <= 9 ? 8  : 16;
        case Mode::kanji:        return v <= 9 ? 8  : (v <= 26 ? 10 : 12);
        default:                 return 0;
    }
}

// ======================== capacity tables (TODO) ========================

size_t encoder::data_capacity_bits(Version v, ErrorCorrectionLevel l)
{
    (void)v; (void)l;
    // TODO: the 40 x 4 capacity table (total codewords, EC codewords, blocks).
    return 0;
}

// ======================== decoder (TODO) ========================

std::string decoder::decode(const scl2::bitmap_1c& matrix)
{
    (void)matrix;
    // TODO: locate + unmask + read format info + extract codewords +
    // Reed-Solomon decode + data decode.
    return {};
}

} // namespace scl2::qrcode