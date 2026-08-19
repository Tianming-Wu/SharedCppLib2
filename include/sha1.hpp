/*
    sha1 module for SharedCppLib2

    SHA-1 hash (FIPS 180-4). Produces a 160-bit (20-byte) digest.

    ⚠️ SHA-1 is cryptographically broken (SHAttered collision attack).
    Use it only for legacy compatibility or non-security indexing — do NOT
    use it for any security-critical purpose.

    This module implements the scl2 hash provider interface and can be used
    with the generic hashing API (hash_api.hpp). See doc/hash.md for the list
    of all available hash providers.
*/

#pragma once

#include <stdint.h>

#include <string>

#include "api.hpp"
#include "bytearray.hpp"

namespace scl2 {

class sha1 {
public:
    static constexpr size_t result_size = 20;
    static constexpr size_t block_size  = 64;

    /// @brief One-shot SHA-1 hash.
    static scl2::bytearray hash(const scl2::bytearray& message);

    static std::string getHexMessageDigest(const std::string& message);
    static scl2::bytearray getMessageDigest(const scl2::bytearray& message);

    /// @brief Streaming SHA-1 hasher.
    class stream_type {
    public:
        stream_type();
        /// @brief Feed a data chunk.
        void update(const scl2::bytearray& chunk);
        /// @brief Finalize and return the 20-byte digest.
        scl2::bytearray end();
    private:
        void process_block(const uint8_t block[64]);

        uint32_t state_[5];
        uint8_t  buffer_[64];
        size_t   buf_len_ = 0;
        uint64_t total_bits_ = 0;
    };

}; // sha1

scl2_check_hashing_support(sha1);

} // namespace scl2
