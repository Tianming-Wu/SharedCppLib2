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

    /** @brief: 使用SHA256算法，获取输入信息的摘要（数字指纹）
    @param[in] message: 输入信息
    @return: 摘要（数字指纹）
    @throw: std::runtime_error if failed
    */
    static std::bytearray hash(const std::bytearray& message);

    /** @brief: 获取十六进制表示的信息摘要（数字指纹）
    @param[in] message: 输入信息
    @return: 十六进制表示的信息摘要（数字指纹）
    @throw: std::runtime_error if failed
    */
    static std::string getHexMessageDigest(const std::string& message);
    static std::bytearray getMessageDigest(const std::bytearray& message);

}; // sha256

scl2_check_hashing_support(sha256);

} // namespace scl2