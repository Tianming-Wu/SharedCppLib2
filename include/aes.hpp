/*
    AES Encryption Module for SharedCppLib2

    Template-based AES with support for 128, 192, and 256-bit keys.
    Provides ECB and CBC modes with PKCS7 padding.

    Specification: FIPS PUB 197

    namespace: scl2::crypto
    link target: SharedCppLib2::aes
*/

#pragma once

#include "encryption_api.hpp"
#include "bytearray.hpp"

namespace scl2 { inline namespace crypto {

/// @brief AES-ECB mode.
/// @tparam KeyBits Key size in bits: 128, 192, or 256.
template<size_t KeyBits>
class aes_ecb {
    static_assert(KeyBits == 128 || KeyBits == 192 || KeyBits == 256,
                  "AES key size must be 128, 192, or 256 bits");
public:
    using key_type = std::bytearray;

    static constexpr size_t key_size    = KeyBits / 8;
    static constexpr size_t block_size  = 16;
    static constexpr size_t round_count = KeyBits == 128 ? 10
                                        : KeyBits == 192 ? 12 : 14;
    static constexpr size_t rk_count    = block_size * (round_count + 1); // 176/208/240

    // ─── Static API ──────────────────────────────────────────────────
    static std::bytearray encrypt(const std::bytearray& data, const std::bytearray& key);
    static std::bytearray decrypt(const std::bytearray& data, const std::bytearray& key);

    // ─── Instance API (pre-configured with key) ──────────────────────
    explicit aes_ecb(const std::bytearray& key) : stored_key_(key) {}
    std::bytearray encrypt(const std::bytearray& data) const { return encrypt(data, stored_key_); }
    std::bytearray decrypt(const std::bytearray& data) const { return decrypt(data, stored_key_); }

private:
    std::bytearray stored_key_;
};

/// @brief AES-CBC mode.
/// @tparam KeyBits Key size in bits: 128, 192, or 256.
/// @note key_type is key_size + 16 bytes (IV concatenated).
template<size_t KeyBits>
class aes_cbc {
    static_assert(KeyBits == 128 || KeyBits == 192 || KeyBits == 256,
                  "AES key size must be 128, 192, or 256 bits");
public:
    using key_type = std::bytearray;

    static constexpr size_t key_size    = KeyBits / 8;
    static constexpr size_t block_size  = 16;
    static constexpr size_t round_count = KeyBits == 128 ? 10
                                        : KeyBits == 192 ? 12 : 14;
    static constexpr size_t rk_count    = block_size * (round_count + 1);

    // ─── Static API (key = key_bytes + iv_bytes) ─────────────────────
    static std::bytearray encrypt(const std::bytearray& data, const std::bytearray& key);
    static std::bytearray decrypt(const std::bytearray& data, const std::bytearray& key);

    // ─── Instance API ────────────────────────────────────────────────
    explicit aes_cbc(const std::bytearray& key) : stored_key_(key) {}
    std::bytearray encrypt(const std::bytearray& data) const { return encrypt(data, stored_key_); }
    std::bytearray decrypt(const std::bytearray& data) const { return decrypt(data, stored_key_); }

private:
    std::bytearray stored_key_;
};

// ─── Convenience aliases ────────────────────────────────────────────────
using aes_ecb_128 = aes_ecb<128>;
using aes_ecb_192 = aes_ecb<192>;
using aes_ecb_256 = aes_ecb<256>;

using aes_cbc_128 = aes_cbc<128>;
using aes_cbc_192 = aes_cbc<192>;
using aes_cbc_256 = aes_cbc<256>;

// ─── Compile-time concept checks ────────────────────────────────────────
scl2_check_encryption_support(aes_ecb_128);
scl2_check_decryption_support(aes_ecb_128);
scl2_check_encryption_support(aes_ecb_192);
scl2_check_decryption_support(aes_ecb_192);
scl2_check_encryption_support(aes_ecb_256);
scl2_check_decryption_support(aes_ecb_256);
scl2_check_encryption_support(aes_cbc_128);
scl2_check_decryption_support(aes_cbc_128);
scl2_check_encryption_support(aes_cbc_192);
scl2_check_decryption_support(aes_cbc_192);
scl2_check_encryption_support(aes_cbc_256);
scl2_check_decryption_support(aes_cbc_256);

} } // namespace scl2::crypto
