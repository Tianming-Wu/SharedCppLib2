/*
    sha256 module for SharedCppLib2

    I'm not the author of some of its code. I do added some features and adapt it
    to the bytearray class in this library.

    This module is almost stand-alone, and only depends on bytearray (basic.hpp).
    It can be extracted, but it's not suggested.


    Currently, this module only provides the most basic functionality of SHA256,
    which is to use it as a hash function.
    It cannot actually encrypt data in the sense of a cipher, and it does not support HMAC or other features.
    It also does not support incremental hashing, which means you have to provide the whole message at once.

*/

#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "bytearray.hpp"

///TODO: Change naming of the below function to hash(). (API definition)

namespace sha256 {

/** @brief: 使用SHA256算法，获取输入信息的摘要（数字指纹）
@param[in] message: 输入信息
@return: 摘要（数字指纹）
@throw: std::runtime_error if failed
*/
std::bytearray encrypt(const std::bytearray& message);

/** @brief: 获取十六进制表示的信息摘要（数字指纹）
@param[in] message: 输入信息
@return: 十六进制表示的信息摘要（数字指纹）
@throw: std::runtime_error if failed
*/
std::string getHexMessageDigest(const std::string& message);
std::bytearray getMessageDigest(const std::bytearray& message);

} // namespace sha256

