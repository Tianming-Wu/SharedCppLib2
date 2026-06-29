/*
    Crypto XOR Utility for SharedCppLib2
    Tianming-Wu 2026.03.20

    This module provides a simple utility function for performing XOR
    operations on bytearrays.

    It is used for simple encryption and decryption tasks where 
    security is not a very concern, such as obfuscating data or
    creating simple checksums.

    It provides a basic obstacle for users that might try to read the
    text data directly, and not being able to extract the key or the
    original data from the executable or memory.

    Warning: does not validate the key length.

*/

#pragma once

#include "api.hpp"
#include "bytearray.hpp"

namespace scl2 { inline namespace crypto {


// Why is the name "xor" not usable?

class xor_op {
public:
    using key_type = std::bytearray;

    static std::bytearray encrypt(const std::bytearray& data, const std::bytearray& key);
    static std::bytearray decrypt(const std::bytearray& data, const std::bytearray& key);

}; // xor_op

scl2_check_encryption_support(xor_op);
scl2_check_decryption_support(xor_op);


// Different from normal xor_op, it does some extra operations in the key
// and result, to make it less straightforward and more obfuscated. It is not intended
// to be more secure, but just to be a little bit more difficult to analyze.
class zxor_op {
    using key_type = std::bytearray;

    static std::bytearray encrypt(const std::bytearray& data, const std::bytearray& key);
    static std::bytearray decrypt(const std::bytearray& data, const std::bytearray& key);
    
};


} } // namespace scl2::crypto