#include "aes.hpp"

#include <stdexcept>
#include <cstring>

namespace scl2::crypto {

// ═══════════════════════════════════════════════════════════════════════
//  Shared AES core (key-size agnostic)
// ═══════════════════════════════════════════════════════════════════════

static const uint8_t SBOX[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static const uint8_t INV_SBOX[256] = {
    0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
    0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
    0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
    0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
    0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
    0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
    0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
    0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
    0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
    0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
    0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
    0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
    0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
    0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
    0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
    0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
};

static const uint8_t RCON[11] = { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36 };

static uint8_t ub(std::byte b) { return static_cast<uint8_t>(b); }
static std::byte bu(uint8_t v) { return std::byte{v}; }

static uint8_t gf_mul2(uint8_t x) { return (x << 1) ^ ((x >> 7) & 1 ? 0x1b : 0); }
static uint8_t gf_mul3(uint8_t x) { return gf_mul2(x) ^ x; }
static uint8_t gf_mul9(uint8_t x)  { return gf_mul2(gf_mul2(gf_mul2(x))) ^ x; }
static uint8_t gf_mul11(uint8_t x) { return gf_mul2(gf_mul2(gf_mul2(x))) ^ gf_mul2(x) ^ x; }
static uint8_t gf_mul13(uint8_t x) { return gf_mul2(gf_mul2(gf_mul2(x))) ^ gf_mul2(gf_mul2(x)) ^ x; }
static uint8_t gf_mul14(uint8_t x) { return gf_mul2(gf_mul2(gf_mul2(x))) ^ gf_mul2(gf_mul2(x)) ^ gf_mul2(x); }

// ─── Key expansion (parameterized by round count) ───────────────────────
static void aes_key_expand(const std::byte key[], std::byte rk[], size_t key_len, size_t nround) {
    size_t nk = key_len / 4;           // 4, 6, or 8 words
    size_t total = 4 * (nround + 1);   // 44, 52, or 60 words
    std::memcpy(rk, key, key_len);

    for (size_t i = nk; i < total; ++i) {
        uint8_t temp[4];
        std::memcpy(temp, rk + (i - 1) * 4, 4);

        if (i % nk == 0) {
            uint8_t t = temp[0]; temp[0] = temp[1]; temp[1] = temp[2];
            temp[2] = temp[3]; temp[3] = t;
            temp[0] = SBOX[temp[0]]; temp[1] = SBOX[temp[1]];
            temp[2] = SBOX[temp[2]]; temp[3] = SBOX[temp[3]];
            temp[0] ^= RCON[i / nk];
        } else if (nk > 6 && i % nk == 4) {
            // AES-256: extra SubWord on every 4th word of the second half
            temp[0] = SBOX[temp[0]]; temp[1] = SBOX[temp[1]];
            temp[2] = SBOX[temp[2]]; temp[3] = SBOX[temp[3]];
        }

        for (int j = 0; j < 4; ++j)
            rk[i * 4 + j] = bu(ub(rk[(i - nk) * 4 + j]) ^ temp[j]);
    }
}

// ─── Round functions (same for all key sizes) ───────────────────────────
static void sub_bytes(std::byte state[16]) {
    for (int i = 0; i < 16; ++i) state[i] = bu(SBOX[ub(state[i])]);
}
static void inv_sub_bytes(std::byte state[16]) {
    for (int i = 0; i < 16; ++i) state[i] = bu(INV_SBOX[ub(state[i])]);
}

static void shift_rows(std::byte state[16]) {
    std::byte t;
    t = state[1];  state[1] = state[5];  state[5] = state[9];
    state[9] = state[13]; state[13] = t;
    t = state[2];  state[2] = state[10]; state[10] = t;
    t = state[6];  state[6] = state[14]; state[14] = t;
    t = state[15]; state[15] = state[11]; state[11] = state[7];
    state[7] = state[3];  state[3] = t;
}

static void inv_shift_rows(std::byte state[16]) {
    std::byte t;
    t = state[13]; state[13] = state[9];  state[9] = state[5];
    state[5] = state[1];  state[1] = t;
    t = state[2];  state[2] = state[10]; state[10] = t;
    t = state[6];  state[6] = state[14]; state[14] = t;
    t = state[3];  state[3] = state[7];  state[7] = state[11];
    state[11] = state[15]; state[15] = t;
}

static void mix_columns(std::byte state[16]) {
    for (int c = 0; c < 4; ++c) {
        int i = c * 4;
        uint8_t a0 = ub(state[i]), a1 = ub(state[i+1]), a2 = ub(state[i+2]), a3 = ub(state[i+3]);
        state[i]   = bu(gf_mul2(a0) ^ gf_mul3(a1) ^ a2        ^ a3);
        state[i+1] = bu(a0        ^ gf_mul2(a1) ^ gf_mul3(a2) ^ a3);
        state[i+2] = bu(a0        ^ a1        ^ gf_mul2(a2) ^ gf_mul3(a3));
        state[i+3] = bu(gf_mul3(a0) ^ a1        ^ a2        ^ gf_mul2(a3));
    }
}

static void inv_mix_columns(std::byte state[16]) {
    for (int c = 0; c < 4; ++c) {
        int i = c * 4;
        uint8_t a0 = ub(state[i]), a1 = ub(state[i+1]), a2 = ub(state[i+2]), a3 = ub(state[i+3]);
        state[i]   = bu(gf_mul14(a0) ^ gf_mul11(a1) ^ gf_mul13(a2) ^ gf_mul9(a3));
        state[i+1] = bu(gf_mul9(a0)  ^ gf_mul14(a1) ^ gf_mul11(a2) ^ gf_mul13(a3));
        state[i+2] = bu(gf_mul13(a0) ^ gf_mul9(a1)  ^ gf_mul14(a2) ^ gf_mul11(a3));
        state[i+3] = bu(gf_mul11(a0) ^ gf_mul13(a1) ^ gf_mul9(a2)  ^ gf_mul14(a3));
    }
}

static void add_round_key(std::byte state[16], const std::byte rk[16]) {
    for (int i = 0; i < 16; ++i) state[i] ^= rk[i];
}

// ─── Encrypt / decrypt single block (parameterized by round count) ──────
static void aes_encrypt_blk(const std::byte pt[16], std::byte ct[16],
                            const std::byte rk[], size_t nround) {
    std::byte state[16];
    std::memcpy(state, pt, 16);

    add_round_key(state, rk);
    for (size_t r = 1; r < nround; ++r) {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, rk + r * 16);
    }
    sub_bytes(state);
    shift_rows(state);
    add_round_key(state, rk + nround * 16);

    std::memcpy(ct, state, 16);
}

static void aes_decrypt_blk(const std::byte ct[16], std::byte pt[16],
                            const std::byte rk[], size_t nround) {
    std::byte state[16];
    std::memcpy(state, ct, 16);

    add_round_key(state, rk + nround * 16);
    for (size_t r = nround; r-- > 1; ) {
        inv_shift_rows(state);
        inv_sub_bytes(state);
        add_round_key(state, rk + r * 16);
        inv_mix_columns(state);
    }
    inv_shift_rows(state);
    inv_sub_bytes(state);
    add_round_key(state, rk);

    std::memcpy(pt, state, 16);
}

// ─── PKCS7 padding ──────────────────────────────────────────────────────
static std::bytearray pad16(const std::bytearray& data) {
    uint8_t pad_len = static_cast<uint8_t>(16 - (data.size() % 16));
    std::bytearray result = data;
    for (uint8_t i = 0; i < pad_len; ++i)
        result.push_back(std::byte{pad_len});
    return result;
}

static std::bytearray unpad16(const std::bytearray& data) {
    if (data.empty() || data.size() % 16 != 0)
        throw std::invalid_argument("aes: invalid ciphertext length");
    uint8_t pad_len = static_cast<uint8_t>(data.back());
    if (pad_len < 1 || pad_len > 16)
        throw std::invalid_argument("aes: invalid padding byte value");
    for (size_t i = data.size() - pad_len; i < data.size(); ++i)
        if (data.at(i) != std::byte{pad_len})
            throw std::invalid_argument("aes: invalid PKCS7 padding");
    return data.subarr(0, data.size() - pad_len);
}

// ═══════════════════════════════════════════════════════════════════════
//  Template member definitions & explicit instantiations
// ═══════════════════════════════════════════════════════════════════════

template<size_t K>
std::bytearray aes_ecb<K>::encrypt(const std::bytearray& data, const std::bytearray& key) {
    if (key.size() != key_size)
        throw std::invalid_argument("aes_ecb::encrypt: key must be " + std::to_string(key_size) + " bytes");

    std::byte rk[rk_count];
    aes_key_expand(key.data(), rk, key_size, round_count);

    std::bytearray padded = pad16(data);
    std::bytearray result{static_cast<size_t>(padded.size()), std::byte{0}};

    for (size_t i = 0; i < padded.size(); i += 16)
        aes_encrypt_blk(padded.data() + i, result.data() + i, rk, round_count);

    return result;
}

template<size_t K>
std::bytearray aes_ecb<K>::decrypt(const std::bytearray& data, const std::bytearray& key) {
    if (key.size() != key_size)
        throw std::invalid_argument("aes_ecb::decrypt: key must be " + std::to_string(key_size) + " bytes");
    if (data.empty() || data.size() % 16 != 0)
        throw std::invalid_argument("aes_ecb::decrypt: ciphertext length must be multiple of 16");

    std::byte rk[rk_count];
    aes_key_expand(key.data(), rk, key_size, round_count);

    std::bytearray dec{static_cast<size_t>(data.size()), std::byte{0}};
    for (size_t i = 0; i < data.size(); i += 16)
        aes_decrypt_blk(data.data() + i, dec.data() + i, rk, round_count);

    return unpad16(dec);
}

template<size_t K>
std::bytearray aes_cbc<K>::encrypt(const std::bytearray& data, const std::bytearray& key) {
    if (key.size() != key_size + 16)
        throw std::invalid_argument("aes_cbc::encrypt: key must be " + std::to_string(key_size + 16) + " bytes (key+IV)");

    std::byte rk[rk_count];
    aes_key_expand(key.data(), rk, key_size, round_count);

    std::bytearray padded = pad16(data);
    std::bytearray result{static_cast<size_t>(padded.size()), std::byte{0}};

    std::byte prev[16];
    std::memcpy(prev, key.data() + key_size, 16);

    std::byte blk[16];
    for (size_t i = 0; i < padded.size(); i += 16) {
        for (int j = 0; j < 16; ++j) blk[j] = padded.data()[i + j] ^ prev[j];
        aes_encrypt_blk(blk, result.data() + i, rk, round_count);
        std::memcpy(prev, result.data() + i, 16);
    }
    return result;
}

template<size_t K>
std::bytearray aes_cbc<K>::decrypt(const std::bytearray& data, const std::bytearray& key) {
    if (key.size() != key_size + 16)
        throw std::invalid_argument("aes_cbc::decrypt: key must be " + std::to_string(key_size + 16) + " bytes (key+IV)");
    if (data.empty() || data.size() % 16 != 0)
        throw std::invalid_argument("aes_cbc::decrypt: ciphertext length must be multiple of 16");

    std::byte rk[rk_count];
    aes_key_expand(key.data(), rk, key_size, round_count);

    std::bytearray dec{static_cast<size_t>(data.size()), std::byte{0}};

    std::byte prev[16];
    std::memcpy(prev, key.data() + key_size, 16);

    std::byte curr[16];
    for (size_t i = 0; i < data.size(); i += 16) {
        std::memcpy(curr, data.data() + i, 16);
        aes_decrypt_blk(curr, dec.data() + i, rk, round_count);
        for (int j = 0; j < 16; ++j) dec.data()[i + j] ^= prev[j];
        std::memcpy(prev, curr, 16);
    }
    return unpad16(dec);
}

// ═══════════════════════════════════════════════════════════════════════
//  aes_ecb::stream_type
// ═══════════════════════════════════════════════════════════════════════

template<size_t K>
aes_ecb<K>::stream_type::stream_type(const std::bytearray& key, cipher_dir dir)
    : key_(key), dir_(dir) {
    if (key.size() != key_size)
        throw std::invalid_argument("aes_ecb::stream_type: key must be " + std::to_string(key_size) + " bytes");
    buf_.reserve(block_size);
}

template<size_t K>
std::bytearray aes_ecb<K>::stream_type::update(const std::bytearray& chunk) {
    // Flush buffer first
    if (!buf_.empty()) {
        size_t need = block_size - buf_.size();
        size_t take = std::min(need, chunk.size());
        buf_.append(chunk.subarr(0, take));
        if (buf_.size() < block_size) return {}; // still not full
        // Buffer is full, process it below as the first block
    }

    std::bytearray result;
    std::byte rk[rk_count];
    aes_key_expand(key_.data(), rk, key_size, round_count);

    size_t offset = buf_.empty() ? 0 : block_size - buf_.size();

    // Process the buffered block first if we just filled it
    if (!buf_.empty()) {
        result.append(std::bytearray(block_size, std::byte{0}));
        if (dir_ == cipher_dir::Encrypt)
            aes_encrypt_blk(buf_.data(), result.data(), rk, round_count);
        else
            aes_decrypt_blk(buf_.data(), result.data(), rk, round_count);
        buf_.clear();
    }

    // Process full blocks from chunk
    size_t remaining = chunk.size() - offset;
    size_t full_blocks = remaining / block_size;

    for (size_t i = 0; i < full_blocks; ++i) {
        size_t start = offset + i * block_size;
        result.resize(result.size() + block_size);
        if (dir_ == cipher_dir::Encrypt)
            aes_encrypt_blk(chunk.data() + start, result.data() + result.size() - block_size, rk, round_count);
        else
            aes_decrypt_blk(chunk.data() + start, result.data() + result.size() - block_size, rk, round_count);
    }

    // Buffer leftover
    size_t leftover_start = offset + full_blocks * block_size;
    if (leftover_start < chunk.size())
        buf_ = chunk.subarr(leftover_start);

    return result;
}

template<size_t K>
std::bytearray aes_ecb<K>::stream_type::end() {
    // PKCS7 padding on remaining buffer
    uint8_t pad_len = static_cast<uint8_t>(block_size - buf_.size());
    for (uint8_t i = 0; i < pad_len; ++i)
        buf_.push_back(std::byte{pad_len});

    std::bytearray result(block_size, std::byte{0});
    std::byte rk[rk_count];
    aes_key_expand(key_.data(), rk, key_size, round_count);
    aes_encrypt_blk(buf_.data(), result.data(), rk, round_count);
    buf_.clear();
    return result;
}

// ═══════════════════════════════════════════════════════════════════════
//  aes_cbc::stream_type
// ═══════════════════════════════════════════════════════════════════════

template<size_t K>
aes_cbc<K>::stream_type::stream_type(const std::bytearray& key, cipher_dir dir)
    : key_(key), dir_(dir) {
    if (key.size() != key_size + 16)
        throw std::invalid_argument("aes_cbc::stream_type: key must be " + std::to_string(key_size + 16) + " bytes");
    buf_.reserve(block_size);
}

template<size_t K>
std::bytearray aes_cbc<K>::stream_type::update(const std::bytearray& chunk) {
    std::bytearray result;
    std::byte rk[rk_count];
    aes_key_expand(key_.data(), rk, key_size, round_count);

    // Chain state (prev ciphertext block, init with IV)
    std::byte chain[16];
    std::memcpy(chain, key_.data() + key_size, 16);

    // The accumulated buffer becomes part of the first block
    if (!buf_.empty()) {
        size_t need = block_size - buf_.size();
        size_t take = std::min(need, chunk.size());
        buf_.append(chunk.subarr(0, take));
    }

    // Process full blocks from buffer + chunk
    // We need to work with the combined data
    std::bytearray combined;
    if (!buf_.empty()) {
        if (buf_.size() < block_size) return {}; // still not full
        combined = buf_;  // copy the full buffer
        buf_.clear();
    }

    size_t offset = combined.empty() ? 0 : 0;
    if (combined.empty() && chunk.size() < block_size) {
        buf_ = chunk;
        return {};
    }

    // Build processing buffer: combined (if any) + rest of chunk
    std::bytearray proc;
    if (!combined.empty()) proc = combined;
    proc.append(chunk.subarr(proc.empty() ? 0 : block_size - (combined.size() - buf_.size())));

    // Process full blocks
    size_t full = proc.size() / block_size;
    for (size_t i = 0; i < full; ++i) {
        const std::byte* blk = proc.data() + i * block_size;
        result.resize(result.size() + block_size);
        if (dir_ == cipher_dir::Encrypt) {
            std::byte xored[16];
            for (int j = 0; j < 16; ++j) xored[j] = blk[j] ^ chain[j];
            aes_encrypt_blk(xored, result.data() + result.size() - block_size, rk, round_count);
            std::memcpy(chain, result.data() + result.size() - block_size, 16);
        } else {
            aes_decrypt_blk(blk, result.data() + result.size() - block_size, rk, round_count);
            for (int j = 0; j < 16; ++j)
                result.data()[result.size() - block_size + j] ^= chain[j];
            std::memcpy(chain, blk, 16);
        }
    }

    // Buffer leftover
    size_t leftover = proc.size() % block_size;
    if (leftover > 0)
        buf_ = proc.subarr(full * block_size);

    return result;
}

template<size_t K>
std::bytearray aes_cbc<K>::stream_type::end() {
    uint8_t pad_len = static_cast<uint8_t>(block_size - buf_.size());
    for (uint8_t i = 0; i < pad_len; ++i)
        buf_.push_back(std::byte{pad_len});

    std::byte rk[rk_count];
    aes_key_expand(key_.data(), rk, key_size, round_count);

    // Get IV for chaining
    std::byte chain[16];
    std::memcpy(chain, key_.data() + key_size, 16);

    std::bytearray result(block_size, std::byte{0});
    std::byte xored[16];
    for (int j = 0; j < 16; ++j) xored[j] = buf_.data()[j] ^ chain[j];
    aes_encrypt_blk(xored, result.data(), rk, round_count);
    buf_.clear();
    return result;
}

// ─── Explicit instantiations ────────────────────────────────────────────
template class aes_ecb<128>;
template class aes_ecb<192>;
template class aes_ecb<256>;
template class aes_cbc<128>;
template class aes_cbc<192>;
template class aes_cbc<256>;

} // namespace scl2::crypto
