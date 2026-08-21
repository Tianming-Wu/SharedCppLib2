/*
    Extensive math module (xmath) for SharedCppLib2.

    SubModule: Reed-Solomon error correction over GF(2^8).

    This is the error-correction algorithm used by QR Code (ISO/IEC 18004).
    It relies on a Galois field (有限域 / 伽罗瓦域):

      GF(2^8)  — a finite field of 256 elements, each being one byte.
        * Addition:        XOR of two bytes.
        * Multiplication:  NOT plain integer multiply. Every non-zero element
                           is a power of the primitive element alpha = 2:
                             alpha^k  =  exp_table[k]
                           so  a*b  =  alpha^(log_table[a] + log_table[b]).
        * The field is defined by the primitive polynomial (本原多项式)
              x^8 + x^4 + x^3 + x^2 + 1   (0x11D)
          which tells us how to "wrap around" when alpha^k grows past 8 bits.

    Encoding (what this module does):
      Treat the data bytes as coefficients of a polynomial D(x). Multiply by
      x^e (e = number of ECC codewords) and divide by the generator
      polynomial (生成多项式)
          G(x) = (x - alpha^0)(x - alpha^1)...(x - alpha^(e-1))
      The remainder R(x) is the ECC. Appending R(x)'s e coefficients to the
      data produces a codeword that can survive up to e/2 symbol errors.

    Header-only.
*/

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "bytearray.hpp"

namespace scl2::xmath {

/// Reed-Solomon error-correction encoder over GF(2^8), primitive polynomial 0x11D.
class reedsolomon {
public:
    /// @brief Compute `ec_count` ECC codewords for `data`.
    /// @param data     The message bytes (the final data codewords).
    /// @param ec_count Number of ECC (parity) codewords to produce.
    static scl2::bytearray encode(const scl2::bytearray& data, size_t ec_count);

private:
    // ---- GF(2^8) arithmetic ----
    // alpha^i == exp_table[i]; the table wraps so alpha^255 == alpha^0 == 1.
    static const std::array<uint8_t, 256>& exp_table();
    // log_table[alpha^i] == i. log_table[0] is undefined (0 has no log).
    static const std::array<uint8_t, 256>& log_table();

    static uint8_t mul(uint8_t a, uint8_t b);      // GF multiplication
    static uint8_t pow_alpha(int k);               // alpha^k

    // Coefficients of G(x) of the given degree (leading 1 implicit).
    // Memoized, built incrementally.
    static const std::vector<uint8_t>& generator(size_t degree);

    // Polynomial long division: remainder of data * x^degree / generator.
    static std::vector<uint8_t> compute_remainder(const scl2::bytearray& data,
                                                  const std::vector<uint8_t>& generator);
};

// ============================ implementation ============================

inline const std::array<uint8_t, 256>& reedsolomon::exp_table()
{
    // Repeatedly multiply by alpha = 2, reducing with the primitive
    // polynomial 0x11D whenever the value overflows 8 bits.
    static const std::array<uint8_t, 256> table = [] {
        std::array<uint8_t, 256> t{};
        int x = 1;
        for (int i = 0; i < 255; ++i) {
            t[static_cast<size_t>(i)] = static_cast<uint8_t>(x);
            x <<= 1;
            if (x & 0x100) x ^= 0x11D;
        }
        t[255] = t[0]; // alpha^255 == alpha^0 == 1
        return t;
    }();
    return table;
}

inline const std::array<uint8_t, 256>& reedsolomon::log_table()
{
    static const std::array<uint8_t, 256> table = [] {
        std::array<uint8_t, 256> t{};
        const auto& e = exp_table();
        for (int i = 0; i < 255; ++i)
            t[static_cast<size_t>(e[static_cast<size_t>(i)])] = static_cast<uint8_t>(i);
        return t;
    }();
    return table;
}

inline uint8_t reedsolomon::mul(uint8_t a, uint8_t b)
{
    if (a == 0 || b == 0) return 0;
    const auto& e = exp_table();
    const auto& l = log_table();
    int sum = static_cast<int>(l[a]) + static_cast<int>(l[b]);
    if (sum >= 255) sum -= 255; // mod 255 (alpha^255 == 1)
    return e[static_cast<size_t>(sum)];
}

inline uint8_t reedsolomon::pow_alpha(int k)
{
    k %= 255;
    if (k < 0) k += 255;
    return exp_table()[static_cast<size_t>(k)];
}

inline const std::vector<uint8_t>& reedsolomon::generator(size_t degree)
{
    // G(x) = product_{i=0..degree-1} (x + alpha^i), stored with the leading 1
    // implicit, so cache[d] holds the d lower-degree coefficients.
    //
    // Incremental construction:  G_d = (x + alpha^(d-1)) * G_{d-1}
    // with prev = cache[d-1] (length d-1), root = alpha^(d-1):
    //   result[0]   = prev[0] ^ root
    //   result[k]   = prev[k] ^ mul(prev[k-1], root)      (1 <= k <= d-2)
    //   result[d-1] = mul(prev[d-2], root)
    static std::vector<std::vector<uint8_t>> cache;
    if (cache.empty()) cache.push_back({}); // degree 0: G = 1, no coefficients
    while (cache.size() <= degree) {
        const size_t d = cache.size();      // new degree being built
        const auto& prev = cache.back();    // degree d-1, length d-1
        const uint8_t root = pow_alpha(static_cast<int>(d - 1));
        std::vector<uint8_t> next(d, 0);
        for (size_t j = 0; j < d; ++j) {
            uint8_t v = 0;
            if (j == 0) v = root;           // implicit leading 1 * root
            else        v = mul(prev[j - 1], root);
            if (j + 1 < d) v ^= prev[j];    // carry-over coefficient of G_{d-1}
            next[j] = v;
        }
        cache.push_back(std::move(next));   // after the loop, so prev stays valid
    }
    return cache[degree];
}

inline std::vector<uint8_t> reedsolomon::compute_remainder(const scl2::bytearray& data,
                                                           const std::vector<uint8_t>& gen)
{
    const size_t degree = gen.size();
    std::vector<uint8_t> rem(degree, 0);
    for (size_t i = 0; i < data.size(); ++i) {
        const uint8_t factor = static_cast<uint8_t>(data[i]) ^ rem[0];
        for (size_t j = 0; j + 1 < degree; ++j) rem[j] = rem[j + 1]; // shift left
        rem[degree - 1] = 0;
        if (factor != 0) {
            for (size_t j = 0; j < degree; ++j)
                rem[j] ^= mul(gen[j], factor);
        }
    }
    return rem;
}

inline scl2::bytearray reedsolomon::encode(const scl2::bytearray& data, size_t ec_count)
{
    if (ec_count == 0) return {};
    const std::vector<uint8_t> rem = compute_remainder(data, generator(ec_count));
    scl2::bytearray out(ec_count, std::byte{0});
    for (size_t i = 0; i < ec_count; ++i)
        out[i] = static_cast<std::byte>(rem[i]);
    return out;
}

} // namespace scl2::xmath
