/*
    QRCode encoder/decoder module for SharedCppLib2.

    This module only generates / parses 1-bit matrices (bitmap_1c). Use
    other modules to render them into actual images (PNG/BMP etc.).

    Framework status:
      - Compile-time functional patterns (finder / alignment)  : done
      - Function pattern placement (finder/timing/dark)        : done
      - Data encoding / Reed-Solomon / masking / format info   : TODO
      - Decoder                                                : TODO

    Rendering pipeline (encoder::make_matrix):
        data -> encode_data() -> reed_solomon() -> build_codewords()
             -> place_function_patterns() -> place_data()
             -> apply_mask() -> place_format_info() / place_version_info()
*/

#pragma once

#include <cstdint>
#include <string>
#include <algorithm>

#include "bytearray.hpp"
#include "bitmap.hpp"
#include "xmath-reedsolomon.hpp"

namespace scl2::qrcode {

enum class Mode : uint8_t {
    automatic = 0x0, // pick the most compact mode automatically
    numeric = 0x1,
    alphanumeric = 0x2,
    byte = 0x4,
    kanji = 0x8,
};

enum class ErrorCorrectionLevel : uint8_t {
    L = 0x1,
    M = 0x2,
    Q = 0x3,
    H = 0x4,
};

enum class Version : uint8_t {
    vauto = 0,
     v1,  v2,  v3,  v4,  v5,  v6,  v7,  v8,  v9, v10,
    v11, v12, v13, v14, v15, v16, v17, v18, v19, v20,
    v21, v22, v23, v24, v25, v26, v27, v28, v29, v30,
    v31, v32, v33, v34, v35, v36, v37, v38, v39, v40,
};

/// @brief Size of a version in modules: 17 + 4 * version.
constexpr size_t sizeForVersion(Version v) {
    return 17 + 4 * static_cast<size_t>(v);
}

size_t capacityForVersion(Version v, ErrorCorrectionLevel l);

/// @brief Pick the most compact mode that can represent `data`.
/// Character-by-character check: as soon as a character is outside a mode's
/// alphabet that mode is ruled out, so one bad char can drop several modes.
/// Preference order: numeric > alphanumeric > byte. Kanji is not auto-picked.
Mode detect_mode(const std::string& data);

// ---- Compile-time functional patterns (constexpr) ----

/// 7x7 finder pattern: outer 7x7 ring + center 3x3 (Chebyshev distance d = max(|x-3|,|y-3|)),
/// white ring in between. Dark when d == 3 (outer) or d <= 1 (center 3x3).
inline constexpr auto finder_pattern_7 = [] {
    scl2::bitmap_pattern<7, 7> p;
    for (size_t y = 0; y < 7; ++y)
        for (size_t x = 0; x < 7; ++x) {
            size_t d = std::max(x < 3 ? 3 - x : x - 3, y < 3 ? 3 - y : y - 3);
            p.set(x, y, d == 3 || d <= 1);
        }
    return p;
}();

/// 5x5 alignment pattern: outer 5x5 ring + center pixel.
inline constexpr auto alignment_pattern_5 = [] {
    scl2::bitmap_pattern<5, 5> p;
    for (size_t y = 0; y < 5; ++y)
        for (size_t x = 0; x < 5; ++x) {
            size_t d = std::max(x < 2 ? 2 - x : x - 2, y < 2 ? 2 - y : y - 2);
            p.set(x, y, d == 2 || d == 0);
        }
    return p;
}();

class encoder {
public:
    struct options {
        ErrorCorrectionLevel ec_level = ErrorCorrectionLevel::M;
        Version version = Version::vauto; // vauto = auto-select smallest
        Mode mode = Mode::automatic;      // automatic = detect from data
        int mask = -1;                    // -1 = auto-select best mask
        int quiet_zone = 4;               // white border in modules
    };

    /// @brief Encode data and produce the final QR image (incl. quiet zone).
    static scl2::bitmap_1c generate(const std::string& data, const options& opt = {});

    /// @brief Encode data and produce the bare N x N matrix (no quiet zone).
    static scl2::bitmap_1c make_matrix(const std::string& data, const options& opt = {});

    /// @brief Choose the smallest version that fits the data.
    static Version select_version(const std::string& data, Mode m, ErrorCorrectionLevel l);

    // ---- Phase A: data -> codewords (pure functions, public for testing) ----
    /// @brief Pack `data` into data codewords: mode indicator + char count +
    /// payload bits + terminator + padding (0xEC/0x11) up to the capacity.
    static scl2::bytearray encode_data(const std::string& data, Mode mode,
                                       ErrorCorrectionLevel ec_level, Version version);
    /// @brief Split data codewords into blocks, append Reed-Solomon ECC per
    /// block, then interleave -> the final codeword sequence for the matrix.
    static scl2::bytearray build_codewords(const scl2::bytearray& data_codewords,
                                           Version version, ErrorCorrectionLevel ec_level);

private:
    static scl2::bytearray reed_solomon(const scl2::bytearray& data, size_t ec_count);
    /// @brief Width of the character-count field for a mode at a version.
    static int char_count_bits(Mode mode, Version version);

    // ---- Phase B: codewords -> matrix ----
    static void place_function_patterns(scl2::bitmap_1c& m, Version version);
    static void place_finder(scl2::bitmap_1c& m, size_t x, size_t y);
    static void place_alignment(scl2::bitmap_1c& m, size_t cx, size_t cy);
    static void place_timing(scl2::bitmap_1c& m);
    static void place_dark_module(scl2::bitmap_1c& m, Version version);
    static void place_format_info(scl2::bitmap_1c& m, ErrorCorrectionLevel l, int mask);
    static void place_version_info(scl2::bitmap_1c& m, Version version);
    static void place_data(scl2::bitmap_1c& m, const scl2::bytearray& codewords);
    static void apply_mask(scl2::bitmap_1c& m, int mask);
    static int select_mask(scl2::bitmap_1c& m);

    /// @brief Whether (x, y) belongs to a fixed function module (finder,
    /// separator, timing, format/version info, dark module, alignment).
    static bool is_function_module(size_t x, size_t y, size_t n);
    /// @brief Whether (x, y) is inside a 5x5 alignment pattern.
    static bool is_alignment_module(size_t x, size_t y, size_t n);
    /// @brief Whether mask `mask` flips the module at (x, y).
    static bool mask_condition(int mask, size_t x, size_t y);
    /// @brief N1..N4 penalty score of a (already masked) matrix; lower is better.
    static int mask_penalty(const scl2::bitmap_1c& m);

    // ---- capacity tables ----
    static size_t data_capacity_bits(Version v, ErrorCorrectionLevel l);
};

class decoder {
public:
    /// @brief Decode a QR matrix back into a string. (Not yet implemented.)
    static std::string decode(const scl2::bitmap_1c& matrix);
};

} // namespace scl2::qrcode