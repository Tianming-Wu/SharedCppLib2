/*
    SHA-1 implementation file for SharedCppLib2.
*/
#include "sha1.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>

namespace scl2 {

namespace {

inline uint32_t rotl32(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

} // namespace <anonymous>

// ─── One-shot ─────────────────────────────────────────────────────────
scl2::bytearray sha1::hash(const scl2::bytearray& message) {
    stream_type hasher;
    hasher.update(message);
    return hasher.end();
}

std::string sha1::getHexMessageDigest(const std::string& message) {
    scl2::bytearray digest = hash(scl2::bytearray(message));
    std::ostringstream o_s;
    o_s << std::hex << std::setiosflags(std::ios::uppercase);
    for (auto it = digest.begin(); it != digest.end(); ++it)
        o_s << std::setw(2) << std::setfill('0') << static_cast<unsigned short>(*it);
    return o_s.str();
}

scl2::bytearray sha1::getMessageDigest(const scl2::bytearray& message) {
    return hash(message);
}

// ─── Streaming ────────────────────────────────────────────────────────
sha1::stream_type::stream_type()
    : buf_len_(0), total_bits_(0) {
    state_[0] = 0x67452301;
    state_[1] = 0xEFCDAB89;
    state_[2] = 0x98BADCFE;
    state_[3] = 0x10325476;
    state_[4] = 0xC3D2E1F0;
}

void sha1::stream_type::process_block(const uint8_t block[64]) {
    uint32_t w[80];

    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24)
             | (static_cast<uint32_t>(block[i * 4 + 1]) << 16)
             | (static_cast<uint32_t>(block[i * 4 + 2]) << 8)
             |  static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i)
        w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3], e = state_[4];

    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20)       { f = (b & c) | ((~b) & d);       k = 0x5A827999; }
        else if (i < 40)  { f = b ^ c ^ d;                   k = 0x6ED9EBA1; }
        else if (i < 60)  { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else              { f = b ^ c ^ d;                   k = 0xCA62C1D6; }

        uint32_t temp = rotl32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotl32(b, 30);
        b = a;
        a = temp;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
}

void sha1::stream_type::update(const scl2::bytearray& chunk) {
    total_bits_ += chunk.size() * 8;

    size_t offset = 0;
    size_t remaining = chunk.size();

    if (buf_len_ > 0 && buf_len_ + remaining >= 64) {
        size_t copy = 64 - buf_len_;
        std::memcpy(buffer_ + buf_len_, chunk.data() + offset, copy);
        offset += copy;
        remaining -= copy;
        process_block(buffer_);
        buf_len_ = 0;
    }

    while (remaining >= 64) {
        process_block(reinterpret_cast<const uint8_t*>(chunk.data() + offset));
        offset += 64;
        remaining -= 64;
    }

    if (remaining > 0) {
        std::memcpy(buffer_ + buf_len_, chunk.data() + offset, remaining);
        buf_len_ += remaining;
    }
}

scl2::bytearray sha1::stream_type::end() {
    // Padding: 0x80 then zeros, then 64-bit big-endian bit length.
    uint8_t padding[128];
    size_t pad_len = (buf_len_ < 56) ? (56 - buf_len_) : (120 - buf_len_);
    padding[0] = 0x80;
    for (size_t i = 1; i < pad_len; ++i) padding[i] = 0x00;

    for (int i = 0; i < 8; ++i)
        padding[pad_len + i] = static_cast<uint8_t>(total_bits_ >> (56 - 8 * i));

    update(scl2::bytearray(reinterpret_cast<const std::byte*>(padding), pad_len + 8));

    scl2::bytearray result(static_cast<size_t>(20));
    for (int i = 0; i < 5; ++i) {
        result[i * 4]     = static_cast<std::byte>((state_[i] >> 24) & 0xFF);
        result[i * 4 + 1] = static_cast<std::byte>((state_[i] >> 16) & 0xFF);
        result[i * 4 + 2] = static_cast<std::byte>((state_[i] >> 8)  & 0xFF);
        result[i * 4 + 3] = static_cast<std::byte>( state_[i]        & 0xFF);
    }
    return result;
}

} // namespace scl2
