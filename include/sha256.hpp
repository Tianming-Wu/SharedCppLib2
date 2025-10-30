#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "bytearray.hpp"

namespace sha256 {

/** @brief: 使用SHA256算法，获取输入信息的摘要（数字指纹）
@param[in] message: 输入信息
@param[out] _digest: 摘要（数字指纹）
@return: 是否成功
*/
bool encrypt(const std::bytearray& message, 
                std::bytearray* _digest);

/** @brief: 获取十六进制表示的信息摘要（数字指纹）
@param[in] message: 输入信息
@return: 十六进制表示的信息摘要（数字指纹）
*/
std::string getHexMessageDigest(const std::string& message);
std::bytearray getMessageDigest(std::bytearray message);

} // namespace shaunit

