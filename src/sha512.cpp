/*
    SHA-512 implementation file for SharedCppLib2.
*/
#include "sha512.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>

namespace scl2 {

namespace {

inline uint64_t rotr64(uint64_t x, int n) { return (x >> n) | (x << (64 - n)); }
inline uint64_t big_sigma0(uint64_t x) { return rotr64(x, 28) ^ rotr64(x, 34) ^ rotr64(x, 39); }
inline uint64_t big_sigma1(uint64_t x) { return rotr64(x, 14) ^ rotr64(x, 18) ^ rotr64(x, 41); }
inline uint64_t small_sigma0(uint64_t x) { return rotr64(x, 1) ^ rotr64(x, 8) ^ (x >> 7); }
inline uint64_t small_sigma1(uint64_t x) { return rotr64(x, 19) ^ rotr64(x, 61) ^ (x >> 6); }
inline uint64_t ch(uint64_t x, uint64_t y, uint64_t z) { return (x & y) ^ ((~x) & z); }
inline uint64_t maj(uint64_t x, uint64_t y, uint64_t z) { return (x & y) ^ (x & z) ^ (y & z); }

const uint64_t add_constant_[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

const uint64_t initial_state_[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

} // namespace <anonymous>

// ─── One-shot ─────────────────────────────────────────────────────────
scl2::bytearray sha512::hash(const scl2::bytearray& message) {
    stream_type hasher;
    hasher.update(message);
    return hasher.end();
}

std::string sha512::getHexMessageDigest(const std::string& message) {
    scl2::bytearray digest = hash(scl2::bytearray(message));
    std::ostringstream o_s;
    o_s << std::hex << std::setiosflags(std::ios::uppercase);
    for (auto it = digest.begin(); it != digest.end(); ++it)
        o_s << std::setw(2) << std::setfill('0') << static_cast<unsigned short>(*it);
    return o_s.str();
}

scl2::bytearray sha512::getMessageDigest(const scl2::bytearray& message) {
    return hash(message);
}

// ─── Streaming ────────────────────────────────────────────────────────
sha512::stream_type::stream_type()
    : buf_len_(0), total_bits_(0) {
    std::memcpy(state_, initial_state_, sizeof(initial_state_));
}

void sha512::stream_type::process_block(const uint8_t block[128]) {
    uint64_t w[80];

    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint64_t>(block[i * 8]) << 56)
             | (static_cast<uint64_t>(block[i * 8 + 1]) << 48)
             | (static_cast<uint64_t>(block[i * 8 + 2]) << 40)
             | (static_cast<uint64_t>(block[i * 8 + 3]) << 32)
             | (static_cast<uint64_t>(block[i * 8 + 4]) << 24)
             | (static_cast<uint64_t>(block[i * 8 + 5]) << 16)
             | (static_cast<uint64_t>(block[i * 8 + 6]) << 8)
             |  static_cast<uint64_t>(block[i * 8 + 7]);
    }
    for (int i = 16; i < 80; ++i)
        w[i] = small_sigma1(w[i - 2]) + w[i - 7] + small_sigma0(w[i - 15]) + w[i - 16];

    uint64_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    uint64_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

    for (int i = 0; i < 80; ++i) {
        uint64_t t1 = h + big_sigma1(e) + ch(e, f, g) + add_constant_[i] + w[i];
        uint64_t t2 = big_sigma0(a) + maj(a, b, c);
        h = g; g = f; f = e;
        e = d + t1;
        d = c; c = b; b = a;
        a = t1 + t2;
    }

    state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
}

void sha512::stream_type::update(const scl2::bytearray& chunk) {
    total_bits_ += chunk.size() * 8;

    size_t offset = 0;
    size_t remaining = chunk.size();

    if (buf_len_ > 0 && buf_len_ + remaining >= 128) {
        size_t copy = 128 - buf_len_;
        std::memcpy(buffer_ + buf_len_, chunk.data() + offset, copy);
        offset += copy;
        remaining -= copy;
        process_block(buffer_);
        buf_len_ = 0;
    }

    while (remaining >= 128) {
        process_block(reinterpret_cast<const uint8_t*>(chunk.data() + offset));
        offset += 128;
        remaining -= 128;
    }

    if (remaining > 0) {
        std::memcpy(buffer_ + buf_len_, chunk.data() + offset, remaining);
        buf_len_ += remaining;
    }
}

scl2::bytearray sha512::stream_type::end() {
    // Padding: 0x80 then zeros, then 128-bit big-endian bit length.
    uint8_t padding[256];
    size_t pad_len = (buf_len_ < 112) ? (112 - buf_len_) : (240 - buf_len_);
    padding[0] = 0x80;
    for (size_t i = 1; i < pad_len; ++i) padding[i] = 0x00;

    // SHA-512 length is 128 bits; the high 64 bits are zero for realistic inputs.
    for (int i = 0; i < 8; ++i) padding[pad_len + i] = 0x00;
    for (int i = 0; i < 8; ++i)
        padding[pad_len + 8 + i] = static_cast<uint8_t>(total_bits_ >> (56 - 8 * i));

    update(scl2::bytearray(reinterpret_cast<const std::byte*>(padding), pad_len + 16));

    scl2::bytearray result(static_cast<size_t>(64));
    for (int i = 0; i < 8; ++i) {
        result[i * 8]     = static_cast<std::byte>((state_[i] >> 56) & 0xFF);
        result[i * 8 + 1] = static_cast<std::byte>((state_[i] >> 48) & 0xFF);
        result[i * 8 + 2] = static_cast<std::byte>((state_[i] >> 40) & 0xFF);
        result[i * 8 + 3] = static_cast<std::byte>((state_[i] >> 32) & 0xFF);
        result[i * 8 + 4] = static_cast<std::byte>((state_[i] >> 24) & 0xFF);
        result[i * 8 + 5] = static_cast<std::byte>((state_[i] >> 16) & 0xFF);
        result[i * 8 + 6] = static_cast<std::byte>((state_[i] >> 8)  & 0xFF);
        result[i * 8 + 7] = static_cast<std::byte>( state_[i]        & 0xFF);
    }
    return result;
}

} // namespace scl2
