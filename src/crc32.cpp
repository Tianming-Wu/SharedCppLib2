/*
    CRC32 implementation file for SharedCppLib2.
*/
#include "crc32.hpp"

#include <array>

namespace scl2 {

// ─── CRC-32 lookup table (polynomial 0xEDB88320, reflected) ──────────
static const std::array<uint32_t, 256>& crc32_table() {
    static const std::array<uint32_t, 256> table = []() {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j) {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xEDB88320;
                else
                    crc >>= 1;
            }
            t[i] = crc;
        }
        return t;
    }();
    return table;
}

// ─── One-shot ─────────────────────────────────────────────────────────
std::bytearray crc32::hash(const std::bytearray& data) {
    stream_type hasher;
    hasher.update(data);
    return hasher.end();
}

// ─── Streaming ────────────────────────────────────────────────────────
crc32::stream_type::stream_type()
    : crc_(0xFFFFFFFF)
{}

void crc32::stream_type::update(const std::bytearray& chunk) {
    const auto& table = crc32_table();
    for (size_t i = 0; i < chunk.size(); ++i) {
        uint8_t index = static_cast<uint8_t>(crc_ ^ static_cast<uint32_t>(chunk[i]));
        crc_ = (crc_ >> 8) ^ table[index];
    }
}

std::bytearray crc32::stream_type::end() {
    uint32_t final_crc = crc_ ^ 0xFFFFFFFF;

    // Big-endian output to match standard CRC-32 (PKZip / zlib convention).
    std::bytearray result(4);
    result[0] = static_cast<std::byte>((final_crc >> 24) & 0xFF);
    result[1] = static_cast<std::byte>((final_crc >> 16) & 0xFF);
    result[2] = static_cast<std::byte>((final_crc >> 8) & 0xFF);
    result[3] = static_cast<std::byte>(final_crc & 0xFF);
    return result;
}

} // namespace scl2
