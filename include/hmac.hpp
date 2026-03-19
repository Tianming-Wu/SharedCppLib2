/*
    Hash-based Message Authentication Code (HMAC) Module for SharedCppLib2
    Tianming-Wu 2026.03.20

    Provides a way to verify the integrity and authenticity of a
    message using a secret key and a hash function.


    * This module is a compatibility layer.
*/

#pragma once

#include <stdexcept>

#include "api.hpp"
#include "bytearray.hpp"

namespace scl2 {

template<typename HashClass>
requires has_hashing_support<HashClass>
class hmac {
public:
    using hash_provider = HashClass;
    using key_type = std::bytearray;

    static constexpr size_t block_size = []() constexpr {
        if constexpr (requires { { hash_provider::block_size } -> std::convertible_to<size_t>; }) {
            return static_cast<size_t>(hash_provider::block_size);
        }
        return static_cast<size_t>(64);
    }();

    static constexpr size_t result_size = generic_hash_result_size<hash_provider>();

    static std::bytearray compute(const std::bytearray& message, const key_type& key)
    {
        std::bytearray normalized_key = key;
        if (normalized_key.size() > block_size)
        {
            normalized_key = generic_hash<hash_provider>(normalized_key);
            if (normalized_key.size() > block_size)
            {
                throw std::runtime_error("scl2::hmac::compute: hash provider output is larger than HMAC block_size");
            }
        }

        std::bytearray key_block(block_size, B(0x00));
        for (size_t i = 0; i < normalized_key.size(); ++i)
        {
            key_block[i] = normalized_key[i];
        }

        std::bytearray inner_pad(block_size, B(0x00));
        std::bytearray outer_pad(block_size, B(0x00));
        for (size_t i = 0; i < block_size; ++i)
        {
            inner_pad[i] = key_block[i] ^ B(0x36);
            outer_pad[i] = key_block[i] ^ B(0x5c);
        }

        std::bytearray inner_message;
        inner_message.reserve(block_size + message.size());
        inner_message.append(inner_pad);
        inner_message.append(message);

        std::bytearray inner_digest = generic_hash<hash_provider>(inner_message);
        if constexpr (result_size != 0)
        {
            if (inner_digest.size() != result_size)
            {
                throw std::runtime_error("scl2::hmac::compute: hash provider returned unexpected digest size");
            }
        }
        else if (inner_digest.empty())
        {
            throw std::runtime_error("scl2::hmac::compute: hash provider returned empty digest");
        }

        std::bytearray outer_message;
        outer_message.reserve(block_size + inner_digest.size());
        outer_message.append(outer_pad);
        outer_message.append(inner_digest);

        std::bytearray result = generic_hash<hash_provider>(outer_message);
        if constexpr (result_size != 0)
        {
            if (result.size() != result_size)
            {
                throw std::runtime_error("scl2::hmac::compute: hash provider returned unexpected digest size");
            }
        }
        else if (result.empty())
        {
            throw std::runtime_error("scl2::hmac::compute: hash provider returned empty digest");
        }

        return result;
    }


};

} // namespace scl2