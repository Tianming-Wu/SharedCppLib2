/*
    CRC32 hash module for SharedCppLib2.

    Implements CRC-32 (Ethernet / PKZip / zlib variant) with polynomial
    0xEDB88320 (reflected).

    This module implements the scl2 hash provider interface and can be used
    with the generic hashing API (hash_api.hpp).
*/

#pragma once

#include <stdint.h>

#include <string>

#include "api.hpp"
#include "bytearray.hpp"

namespace scl2 {

class crc32 {
public:
    static constexpr size_t result_size = 4;
    static constexpr size_t block_size  = 1;

    /// @brief One-shot CRC32 hash.
    static scl2::bytearray hash(const scl2::bytearray& data);

    /// @brief Streaming CRC32 hasher.
    class stream_type {
    public:
        stream_type();
        /// @brief Feed a data chunk.
        void update(const scl2::bytearray& chunk);
        /// @brief Finalize and return the 4-byte CRC32 digest (big-endian).
        scl2::bytearray end();
    private:
        uint32_t crc_;
    };

}; // crc32

scl2_check_hashing_support(crc32);

} // namespace scl2
