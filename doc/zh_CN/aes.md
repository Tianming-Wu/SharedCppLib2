# aes - AES 加密模块

+ 名称: aes
+ 命名空间: `scl2::crypto` (inline)
+ 文档版本: `1.1.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `aes` |

包含方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::aes)
```

```cpp
#include <SharedCppLib2/aes.hpp>
```

## 描述

`aes` 提供 AES-128、AES-192 和 AES-256 的加密和解密，支持 ECB 和 CBC 模式、PKCS7 填充，完全符合 FIPS 197 标准。同时支持静态一次性调用和基于实例的 API。

## 快速开始

### ECB 模式（一次性调用）

```cpp
#include <SharedCppLib2/aes.hpp>

// AES-128 需要 16 字节密钥
std::bytearray key(static_cast<size_t>(16), std::byte{0});
std::bytearray pt = std::bytearray(std::string("Hello, AES!"));

// 加密
auto ct = scl2::aes_ecb_128::encrypt(pt, key);

// 解密
auto dec = scl2::aes_ecb_128::decrypt(ct, key);
// dec.toStdString() == "Hello, AES!"
```

### CBC 模式

CBC 模式下，16 字节的 IV 附加在密钥后面，作为一个 `key_type` 传入：

```cpp
// AES-128-CBC: key(16) + iv(16) = 32 字节
std::bytearray key(static_cast<size_t>(16), std::byte{0x2b});       // 密钥
std::bytearray iv(static_cast<size_t>(16), std::byte{0x00});        // 初始向量

// 拼接为一个 key_type
std::bytearray keyiv = key;
keyiv.append(iv);

auto ct = scl2::aes_cbc_128::encrypt(pt, keyiv);
auto dec = scl2::aes_cbc_128::decrypt(ct, keyiv);
```

> [!IMPORTANT]
> **IV 必须是随机的，且同一密钥每次加密使用不同的 IV。**
> 重复使用 IV 会破坏 CBC 的安全性。
> 简单做法：生成 16 字节随机数，拼在密文前面发送，解密时再提取：
>
> ```cpp
> // 加密
> std::bytearray iv = /* 16 字节随机数 */;
> std::bytearray keyiv = key;
> keyiv.append(iv);
> auto ct = aes_cbc_128::encrypt(data, keyiv);
> // 存储或传输: iv + ct
>
> // 解密
> std::bytearray iv2 = received.subarr(0, 16);   // 提取 IV
> std::bytearray ct2 = received.subarr(16);       // 提取密文
> std::bytearray keyiv2 = key;
> keyiv2.append(iv2);
> auto pt = aes_cbc_128::decrypt(ct2, keyiv2);
> ```

### 实例 API（预配置密钥）

```cpp
scl2::aes_ecb_256 cipher(key32);
auto ct = cipher.encrypt(pt);
auto dec = cipher.decrypt(ct);
```

## 类参考

### 模板类

```cpp
template<size_t KeyBits> class aes_ecb;
template<size_t KeyBits> class aes_cbc;
```

`KeyBits` 必须为 128、192 或 256。

### 便捷别名

```cpp
using aes_ecb_128 = aes_ecb<128>;   // 16 字节密钥，10 轮
using aes_ecb_192 = aes_ecb<192>;   // 24 字节密钥，12 轮
using aes_ecb_256 = aes_ecb<256>;   // 32 字节密钥，14 轮

using aes_cbc_128 = aes_cbc<128>;
using aes_cbc_192 = aes_cbc<192>;
using aes_cbc_256 = aes_cbc<256>;
```

### 编译期常量

```cpp
static constexpr size_t key_size;    // 16、24 或 32
static constexpr size_t block_size;  // 始终为 16
static constexpr size_t round_count; // 10、12 或 14
```

### 静态 API

```cpp
static std::bytearray encrypt(const std::bytearray& data, const std::bytearray& key);
static std::bytearray decrypt(const std::bytearray& data, const std::bytearray& key);
```

ECB：`key` 必须恰好为 `key_size` 字节。
CBC：`key` 必须为 `key_size + 16` 字节（密钥 + IV 拼接）。

### 实例 API

```cpp
explicit aes_ecb_128(const std::bytearray& key);
std::bytearray encrypt(const std::bytearray& data) const;
std::bytearray decrypt(const std::bytearray& data) const;
```

用密钥构造加密器，然后调用 `encrypt`/`decrypt` 时无需再次传入密钥。

## 密钥验证

AES 本身无法验证解密密钥是否与加密密钥一致——使用错误密钥解密只会产生垃圾数据。

不过，本实现使用了 **PKCS7 填充**，这提供了一种弱形式的验证：如果解密密钥错误，填充字节几乎肯定无效，`decrypt()` 将抛出 `std::invalid_argument`。正确的密钥总是会产生有效的填充。

如需更强的验证，请对密文使用 [HMAC](../zh_CN/hmac.md)：

```cpp
auto ct = scl2::aes_ecb_128::encrypt(data, key);
auto tag = scl2::hmac<scl2::sha256>::compute(ct, auth_key);
// 同时存储 ct 和 tag

// 解密时：
// 重新计算 tag 并比较，确认后再解密
```

## 流式处理（大文件）

对于无法一次性读入内存的大文件，使用 `stream_type`：

```cpp
// 加密
scl2::aes_ecb_128::stream_type enc(key, scl2::cipher_dir::Encrypt);
auto block1 = enc.update(chunk1);
auto block2 = enc.update(chunk2);
auto last   = enc.end();  // PKCS7 填充的最终块

// 解密
scl2::aes_ecb_128::stream_type dec(key, scl2::cipher_dir::Decrypt);
auto pt1 = dec.update(ct1);
auto pt2 = dec.update(ct2);
auto pt3 = dec.end();     // 去除填充后的最终块
```

### 从 istream 流式处理

```cpp
#include <SharedCppLib2/encryption_api.hpp>  // 提供 encrypt_stream

std::ifstream in("plain.bin", std::ios::binary);
std::ofstream out("cipher.bin", std::ios::binary);
scl2::encrypt_stream<scl2::aes_ecb_128>(in, out, key);
```

## 模式

### ECB（电子密码本）

每个 16 字节块独立加密。确定性——相同的明文块产生相同的密文。适合随机访问解密，但不推荐用于超过一个块的消息（除非配合消息认证码）。

### CBC（密码块链接）

每个明文块在加密前与上一个密文块进行 XOR。第一个块使用 IV。非确定性（不同的 IV → 不同的密文）。需要随机/不可预测的 IV 以确保安全性。

## 填充

自动应用 PKCS7 填充。如果明文长度是 16 的倍数，则添加一个完整的填充块（16 字节的 `0x10`）。解密时验证并移除填充。

## 密钥长度

| 变体 | 密钥长度 | 轮数 | 轮密钥大小 |
|---------|----------|--------|---------------|
| AES-128 | 16 字节 | 10 | 176 字节 |
| AES-192 | 24 字节 | 12 | 208 字节 |
| AES-256 | 32 字节 | 14 | 240 字节 |

## 相关模块

- `crypto::xor_op` — 用于混淆的简单 XOR 加密
- `encryption_api.hpp` — 加密提供者的概念定义
