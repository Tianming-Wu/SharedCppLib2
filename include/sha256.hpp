#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace shaunit {

class Sha256
{
public:
    //! 默认构造函数
    Sha256() {}

    //! 析构函数
    virtual ~Sha256() {}

    /** @brief: 使用SHA256算法，获取输入信息的摘要（数字指纹）
    @param[in] message: 输入信息
    @param[out] _digest: 摘要（数字指纹）
    @return: 是否成功
    */
    bool encrypt(const std::vector<uint8_t>& message, 
                 std::vector<uint8_t>* _digest);

    /** @brief: 获取十六进制表示的信息摘要（数字指纹）
    @param[in] message: 输入信息
    @return: 十六进制表示的信息摘要（数字指纹）
    */
    std::string getHexMessageDigest(const std::string& message);

protected:
    /// SHA256算法中定义的6种逻辑运算 ///
    inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) const;
    inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) const;
    inline uint32_t big_sigma0(uint32_t x) const;
    inline uint32_t big_sigma1(uint32_t x) const;
    inline uint32_t small_sigma0(uint32_t x) const;
    inline uint32_t small_sigma1(uint32_t x) const;

    /** @brief: SHA256算法对输入信息的预处理，包括“附加填充比特”和“附加长度值”
    附加填充比特: 在报文末尾进行填充，先补第一个比特为1，然后都补0，直到长度满足对512取模后余数是448。需要注意的是，信息必须进行填充。
    附加长度值: 用一个64位的数据来表示原始消息（填充前的消息）的长度，并将其补到已经进行了填充操作的消息后面。
    @param[in][out] _message: 待处理的信息
    @return: 是否成功
    */
    bool preprocessing(std::vector<uint8_t>* _message) const;

    /** @brief: 将信息分解成连续的64Byte大小的数据块
    @param[in] message: 输入信息，长度为64Byte的倍数
    @param[out] _chunks: 输出数据块
    @return: 是否成功
    */
    bool breakTextInto64ByteChunks(const std::vector<uint8_t>& message, 
                                   std::vector<std::vector<uint8_t> >* _chunks) const;

    /** @brief: 由64Byte大小的数据块，构造出64个4Byte大小的字。
    构造算法：前16个字直接由数据块分解得到，其余的字由如下迭代公式得到：
            W[t] = small_sigma1(W[t-2]) + W[t-7] + small_sigma0(W[t-15]) + W[t-16]
    @param[in] chunk: 输入数据块，大小为64Byte
    @param[out] _words: 输出字
    @return: 是否成功
    */
    bool structureWords(const std::vector<uint8_t>& chunk, 
                        std::vector<uint32_t>* _words) const;

    /** @breif: 基于64个4Byte大小的字，进行64次循环加密
    @param[in] words: 64个4Byte大小的字
    @param[in][out] _message_digest: 信息摘要
    @return: 是否成功
    */
    bool transform(const std::vector<uint32_t>& words, 
                   std::vector<uint32_t>* _message_digest) const;

    /** @brief: 输出最终的哈希值（数字指纹）
    @param[in] input: 步长为32bit的哈希值
    @param[out] _output: 步长为8bit的哈希值
    @return: 是否成功
    */
    bool produceFinalHashValue(const std::vector<uint32_t>& input,
                               std::vector<uint8_t>* _output) const;


private:
    static std::vector<uint32_t> initial_message_digest_; // 在SHA256算法中的初始信息摘要，这些常量是对自然数中前8个质数的平方根的小数部分取前32bit而来。
    static std::vector<uint32_t> add_constant_; // 在SHA256算法中，用到64个常量，这些常量是对自然数中前64个质数的立方根的小数部分取前32bit而来。
};

// 内联函数&模版函数的定义 /

inline uint32_t Sha256::ch(uint32_t x, uint32_t y, uint32_t z) const
{
    return (x & y) ^ ((~x) & z);
}

inline uint32_t Sha256::maj(uint32_t x, uint32_t y, uint32_t z) const
{
    return (x & y) ^ (x & z) ^ (y & z);
}

inline uint32_t Sha256::big_sigma0(uint32_t x) const
{
    return (x >> 2 | x << 30) ^ (x >> 13 | x << 19) ^ (x >> 22 | x << 10);
}

inline uint32_t Sha256::big_sigma1(uint32_t x) const
{
    return (x >> 6 | x << 26) ^ (x >> 11 | x << 21) ^ (x >> 25 | x << 7);
}

inline uint32_t Sha256::small_sigma0(uint32_t x) const
{
    return (x >> 7 | x << 25) ^ (x >> 18 | x << 14) ^ (x >> 3);
}

inline uint32_t Sha256::small_sigma1(uint32_t x) const
{
    return (x >> 17 | x << 15) ^ (x >> 19 | x << 13) ^ (x >> 10);
}

} // namespace shaunit

