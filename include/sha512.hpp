/*
    sha512 module for SharedCppLib2

    SHA-512 hash (FIPS 180-4). Produces a 512-bit (64-byte) digest.

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

class sha512 {
public:
    static constexpr size_t result_size = 64;
    static constexpr size_t block_size  = 128;

    /// @brief One-shot SHA-512 hash.
    static scl2::bytearray hash(const scl2::bytearray& message);

    static std::string getHexMessageDigest(const std::string& message);
    static scl2::bytearray getMessageDigest(const scl2::bytearray& message);

    /// @brief Streaming SHA-512 hasher.
    class stream_type {
    public:
        stream_type();
        /// @brief Feed a data chunk.
        void update(const scl2::bytearray& chunk);
        /// @brief Finalize and return the 64-byte digest.
        scl2::bytearray end();
    private:
        void process_block(const uint8_t block[128]);

        uint64_t state_[8];
        uint8_t  buffer_[128];
        size_t   buf_len_ = 0;
        uint64_t total_bits_ = 0;
    };

}; // sha512

scl2_check_hashing_support(sha512);

} // namespace scl2
