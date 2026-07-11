/*
    Base64 encoding and decoding (forwarding to scl2::bytearray).
    This module is now part of basic; include this header for
    standalone base64::encode / base64::decode convenience.
    namespace:
        base64
    link target:
        SharedCppLib2::base64  (INTERFACE, redirects to basic)
*/
#pragma once

#include <string>
#include "bytearray.hpp"

namespace base64 {

inline std::string encode(const scl2::bytearray& input) {
    return input.toBase64();
}

inline scl2::bytearray decode(const std::string& input) {
    return scl2::bytearray::fromBase64(input);
}

inline std::string encode(const std::string& input) {
    return scl2::bytearray(input).toBase64();
}

// std::string encode(unsigned char *input , size_t input_len);
// std::string decode(std::string input);

} // namespace base64

