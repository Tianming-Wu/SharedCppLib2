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

// std::string encode(unsigned char *input , size_t input_len);
// std::string decode(std::string input);

};
