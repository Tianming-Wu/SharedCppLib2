#include "sha256.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace sha256 {

// 在SHA256算法中的初始信息摘要，这些常量是对自然数中前8个质数的平方根的小数部分取前32bit而来。
std::vector<uint32_t> initial_message_digest_ = 
{ 
    0x6a09e667, 0xbb67ae85, 0x3c6ef372,
    0xa54ff53a, 0x510e527f, 0x9b05688c,
    0x1f83d9ab, 0x5be0cd19 
};

// 在SHA256算法中，用到64个常量，这些常量是对自然数中前64个质数的立方根的小数部分取前32bit而来。
std::vector<uint32_t> add_constant_ = 
{
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

// 内联函数&模版函数的定义 /
inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ ((~x) & z); }
inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
inline uint32_t big_sigma0(uint32_t x) { return (x >> 2 | x << 30) ^ (x >> 13 | x << 19) ^ (x >> 22 | x << 10); }
inline uint32_t big_sigma1(uint32_t x) { return (x >> 6 | x << 26) ^ (x >> 11 | x << 21) ^ (x >> 25 | x << 7); }
inline uint32_t small_sigma0(uint32_t x) { return (x >> 7 | x << 25) ^ (x >> 18 | x << 14) ^ (x >> 3); }
inline uint32_t small_sigma1(uint32_t x) { return (x >> 17 | x << 15) ^ (x >> 19 | x << 13) ^ (x >> 10); }

/** @brief: SHA256算法对输入信息的预处理，包括“附加填充比特”和“附加长度值”
附加填充比特: 在报文末尾进行填充，先补第一个比特为1，然后都补0，直到长度满足对512取模后余数是448。需要注意的是，信息必须进行填充。
附加长度值: 用一个64位的数据来表示原始消息（填充前的消息）的长度，并将其补到已经进行了填充操作的消息后面。
@param[in][out] _message: 待处理的信息
@return: 是否成功
*/
bool preprocessing(std::bytearray* _message);

/** @brief: 将信息分解成连续的64Byte大小的数据块
@param[in] message: 输入信息，长度为64Byte的倍数
@param[out] _chunks: 输出数据块
@return: 是否成功
*/
bool breakTextInto64ByteChunks(const std::bytearray& message, 
                                std::vector<std::bytearray >* _chunks);

/** @brief: 由64Byte大小的数据块，构造出64个4Byte大小的字。
构造算法：前16个字直接由数据块分解得到，其余的字由如下迭代公式得到：
        W[t] = small_sigma1(W[t-2]) + W[t-7] + small_sigma0(W[t-15]) + W[t-16]
@param[in] chunk: 输入数据块，大小为64Byte
@param[out] _words: 输出字
@return: 是否成功
*/
bool structureWords(const std::bytearray& chunk, 
                    std::vector<uint32_t>* _words);

/** @breif: 基于64个4Byte大小的字，进行64次循环加密
@param[in] words: 64个4Byte大小的字
@param[in][out] _message_digest: 信息摘要
@return: 是否成功
*/
bool transform(const std::vector<uint32_t>& words, 
                std::vector<uint32_t>* _message_digest);

/** @brief: 输出最终的哈希值（数字指纹）
@param[in] input: 步长为32bit的哈希值
@param[out] _output: 步长为8bit的哈希值
@return: 是否成功
*/
bool produceFinalHashValue(const std::vector<uint32_t>& input,
                            std::bytearray* _output);


bool encrypt(const std::bytearray& input_message, 
                     std::bytearray* _digest)
{
    if (!input_message.empty() && _digest)
    {
        //! ????????
        std::bytearray message = input_message;
        preprocessing(&message);

        //! ???????????????64Byte??��???????
        std::vector<std::bytearray> chunks;
        breakTextInto64ByteChunks(message, &chunks);

        //! ??64Byte??��??????�?????64??4Byte??��????????????????????
        std::vector<uint32_t> message_digest(initial_message_digest_); // ??????????

        std::vector<uint32_t> words;
        for (const auto& chunk : chunks)
        {
            structureWords(chunk, &words);
            transform(words, &message_digest);
        }

        //! ?????????
        produceFinalHashValue(message_digest, _digest);

        return true;
    }
    else
    {
        return false;
    }
}

std::string getHexMessageDigest(const std::string& message)
{
    if (!message.empty())
    {
        std::bytearray __message;
        for (auto it = message.begin(); it != message.end(); ++it)
        {
            __message.push_back(static_cast<std::byte>(*it));
        }

        std::bytearray digest;
        encrypt(__message, &digest);

        std::ostringstream o_s;
        o_s << std::hex << std::setiosflags(std::ios::uppercase);
        for (auto it = digest.begin(); it != digest.end(); ++it)
        {
            o_s << std::setw(2) << std::setfill('0')
                << static_cast<unsigned short>(*it);
        }

        return o_s.str();
    }
    else
    {
        return "";
    }
}

std::bytearray getMessageDigest(std::bytearray message) {
    if (!message.empty())
    {
        std::bytearray digest;
        encrypt(message, &digest);
        return digest;
    }
    else
    {
        return std::bytearray();
    }
}

bool preprocessing(std::bytearray* _message)
{
    if (_message)
    {
        const uint64_t original_bit_size = _message->size() * 8;

        //! ??????????
        size_t remainder = _message->size() % 64;
        if (remainder < 56)
        {
            _message->append(0x80); // 0x80 == 10000000
            for (size_t i = 1; i < 56 - remainder; ++i)
            {
                _message->append(std::byte{0x00});
            }
        }
        else if (remainder == 56)
        {
            _message->append(0x80);
            for (size_t i = 1; i < 64; ++i)
            {
                _message->append(std::byte{0x00});
            }
        }
        else
        {
            _message->append(0x80);
            for (size_t i = 1; i < 64 - remainder + 56; ++i)
            {
                _message->append(std::byte{0x00});
            }
        }

        //! ???????????????
        for (int i = 1; i <= 8; ++i)
        {
            std::byte c = static_cast<std::byte>(original_bit_size >> (64 - 8 * i));
            _message->append(c);
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool breakTextInto64ByteChunks(const std::bytearray& message, 
                                       std::vector<std::bytearray>* _chunks)
{
    if (_chunks && 0 == message.size() % 64)
    {
        _chunks->clear(); // ??????buffer

        size_t quotient = message.size() / 64;
        for (size_t i = 0; i < quotient; ++i)
        {
            std::bytearray temp(message.begin() + i * 64, message.begin() + (i + 1) * 64);
            _chunks->push_back(temp);
        }
        return true;
    }
    else
    {
        return false;
    }
}

bool structureWords(const std::bytearray& chunk, 
                            std::vector<uint32_t>* _words)
{
    if (_words && 64 == chunk.size())
    {
        _words->resize(64);

        auto& words = *_words;
        for (int i = 0; i < 16; ++i)
        {
            words[i] = (static_cast<uint32_t>(chunk[i * 4]) << 24)
                       | (static_cast<uint32_t>(chunk[i * 4 + 1]) << 16)
                       | (static_cast<uint32_t>(chunk[i * 4 + 2]) << 8)
                       | static_cast<uint32_t>(chunk[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i)
        {
            words[i] = small_sigma1(words[i - 2])
                       + words[i - 7]
                       + small_sigma0(words[i - 15])
                       + words[i - 16];
        }
        return true;
    }
    else
    {
        return false;
    }
}

bool transform(const std::vector<uint32_t>& words, 
                       std::vector<uint32_t>* _message_digest)
{
    if (_message_digest && 8 == _message_digest->size() && 64 == words.size())
    {
        std::vector<uint32_t> d = *_message_digest;

        for (int i = 0; i < 64; ++i)
        {
            uint32_t temp1 = d[7] + big_sigma1(d[4]) + ch(d[4], d[5], d[6]) + add_constant_[i] + words[i];
            uint32_t temp2 = big_sigma0(d[0]) + maj(d[0], d[1], d[2]);
            
            d[7] = d[6];
            d[6] = d[5];
            d[5] = d[4];
            d[4] = d[3] + temp1;
            d[3] = d[2];
            d[2] = d[1];
            d[1] = d[0];
            d[0] = temp1 + temp2;
        }

        for (int i = 0; i < 8; ++i)
        {
            (*_message_digest)[i] += d[i];
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool produceFinalHashValue(const std::vector<uint32_t>& input, 
                                   std::bytearray* _output)
{
    if (_output)
    {
        _output->clear();

        for (auto it = input.begin(); it != input.end(); ++it)
        {
            for (int i = 0; i < 4; i++)
            {
                _output->append(static_cast<std::byte>((*it) >> (24 - 8 * i)));
            }
        }
        return true;
    }
    else
    {
        return false;
    }
}

} // namespace shaunit

