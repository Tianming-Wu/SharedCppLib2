/*
    A simple Base64 encoding and decoding library.
    namespace:
        Base64
    link target:
        SharedCppLib2::Base64
*/
#pragma once

#include <string>

#include "bytearray.hpp"

namespace Base64 {

std::string encode(const std::bytearray& input);
std::bytearray decode(const std::string& input);

// This is using bytearray's raw, no-terminator constructor.
// If you want custom behavior, you should construct bytearray
// yourself and call the above functions directly.
// This one is fine with most use cases.
inline std::string encode(const std::string& input) {
    return encode(std::bytearray(input));
}

// std::string encode(unsigned char *input , size_t input_len);
// std::string decode(std::string input);

};
