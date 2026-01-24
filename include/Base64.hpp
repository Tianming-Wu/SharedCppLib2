/*
    A simple Base64 encoding and decoding library.
    namespace:
        Base64
    link target:
        SharedCppLib2::Base64
*/
#pragma once

#include <iostream>
#include <string>
#include <string.h>

namespace Base64 {

std::string encode(unsigned char *input , size_t input_len);

std::string decode(std::string input);

};
