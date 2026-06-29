/*
    sha256 module for SharedCppLib2

    I'm not the author of some of its code. I do added some features and adapt it
    to the bytearray class in this library.

    This module is part of the SharedCppLib2 Crypto Intergration.
*/

#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "api.hpp"
#include "bytearray.hpp"

namespace scl2 {

class sha256 {
public:
    static constexpr size_t result_size = 32;
    static constexpr size_t block_size = 64;

    /// @brief One-shot SHA-256 hash.
    static std::bytearray hash(const std::bytearray& message);

    static std::string getHexMessageDigest(const std::string& message);
    static std::bytearray getMessageDigest(const std::bytearray& message);

    /// @brief Streaming SHA-256 hasher.
    class stream_type {
    public:
        stream_type();
        /// @brief Feed a data chunk.
        void update(const std::bytearray& chunk);
        /// @brief Finalize and return the 32-byte digest.
        std::bytearray end();
    private:
        void process_block(const uint8_t block[64]);

        uint32_t state_[8];
        uint8_t  buffer_[64];
        size_t   buf_len_ = 0;
        uint64_t total_bits_ = 0;
    };

}; // sha256

scl2_check_hashing_support(sha256);

} // namespace scl2