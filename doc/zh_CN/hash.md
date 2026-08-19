# 哈希算法提供者

SharedCppLib2 提供统一的哈希 API（`hash_api.hpp`）以及一组实现共同接口的哈希算法提供者。本文档列出所有可用的哈希算法，并演示直接使用或通过通用 API 使用它们。

每个提供者都暴露：

```cpp
class provider {
public:
    static constexpr size_t result_size;   // 摘要长度（字节）
    static constexpr size_t block_size;    // 块长度（字节）
    static scl2::bytearray hash(const scl2::bytearray& data); // 一次性
    class stream_type { /* update() / end() */ };            // 流式
};
```

## 可用提供者

| 提供者 | 摘要 | 算法 | 库目标 | 用途 |
|--------|------|------|--------|------|
| `scl2::sha512` | 64 字节 | SHA-512 (FIPS 180-4) | `SharedCppLib2::sha512` | 高强度哈希、签名 |
| `scl2::sha256` | 32 字节 | SHA-256 (FIPS 180-4) | `SharedCppLib2::sha256` | 通用安全哈希 |
| `scl2::sha1` | 20 字节 | SHA-1 (FIPS 180-4) | `SharedCppLib2::sha1` | 遗留兼容 / 非安全索引 |
| `scl2::crc32` | 4 字节 | CRC-32 (0xEDB88320) | `SharedCppLib2::crc32` | 快速错误检测 / 校验和 |

> [!WARNING]
> **`sha1` 已被密码学攻破**（SHAttered 碰撞攻击），仅用于遗留兼容或非安全索引（如资源归档键）。**`crc32` 不是密码学哈希**——它只能检测意外损坏，不能防范恶意篡改。

## 快速开始

### 一次性哈希

```cpp
#include <SharedCppLib2/sha256.hpp>

scl2::bytearray data = scl2::bytearray("Hello, World!");
scl2::bytearray digest = scl2::sha256::hash(data);
std::cout << digest.toHex() << std::endl;  // 64 个十六进制字符
```

### 流式哈希（大数据）

```cpp
#include <SharedCppLib2/sha256.hpp>

scl2::sha256::stream_type hasher;
hasher.update(chunk1);
hasher.update(chunk2);
scl2::bytearray digest = hasher.end();  // 32 字节
```

## 通用哈希 API

`hash_api.hpp` 提供可与任何提供者配合使用的模板，适合编写需要接受多种算法的通用代码。

> [!IMPORTANT]
> 使用通用 API 时，必须**同时**包含算法头文件**和** `hash_api.hpp`：
>
> ```cpp
> #include <SharedCppLib2/hash_api.hpp>   // generic_hash<T>, hash_stream<T>
> #include <SharedCppLib2/sha256.hpp>     // 具体提供者
> ```

```cpp
#include <SharedCppLib2/hash_api.hpp>
#include <SharedCppLib2/sha256.hpp>

// 通用一次性哈希
scl2::bytearray d = scl2::generic_hash<scl2::sha256>(data);

// 编译期摘要长度
static_assert(scl2::generic_hash_result_size<scl2::sha256>() == 32);
```

### 从 istream 流式哈希

```cpp
#include <SharedCppLib2/hash_api.hpp>
#include <SharedCppLib2/sha512.hpp>

std::ifstream file("large.bin", std::ios::binary);
scl2::bytearray digest = scl2::hash_stream<scl2::sha512>(file);
```

## HMAC

所有哈希提供者都可用于 `scl2::hmac<Provider>`：

```cpp
#include <SharedCppLib2/hmac.hpp>
#include <SharedCppLib2/sha256.hpp>

scl2::bytearray key   = scl2::bytearray("secret");
scl2::bytearray data  = scl2::bytearray("message");
scl2::bytearray mac   = scl2::hmac<scl2::sha256>::digest(key, data);
```

## 参见

- [`sha256`](sha256.md) — SHA-256 提供者详情
- [`crc32`](crc32.md) — CRC-32 提供者详情
- [`hmac`](hmac.md) — HMAC 密钥哈希
