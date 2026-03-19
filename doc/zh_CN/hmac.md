# hmac - HMAC 消息认证码库

+ 名称: hmac  
+ 命名空间: `scl2`  
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `hmac`（INTERFACE） |

通用用法（不绑定具体哈希实现）：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::hmac)
```

若示例使用 SHA256 作为 provider：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::hmac SharedCppLib2::sha256)
```

## 描述

`hmac` 是一个仅头文件（header-only）的 HMAC 模板实现。
它可与所有满足 SharedCppLib2 哈希 API 的 provider 类协同工作：

```cpp
static std::bytearray hash(const std::bytearray& data);
```

可选的 provider 元信息：

- `static constexpr size_t block_size`（未提供时默认使用 `64`）
- `static constexpr size_t result_size`（未提供时视为动态长度摘要）

## 快速开始

### 通用头文件引入

```cpp
#include <SharedCppLib2/hmac.hpp>
```

`hmac` 本身不依赖特定哈希算法。
你通过模板参数传入 hash provider 类。

### 示例 provider：SHA256

```cpp
#include <SharedCppLib2/hmac.hpp>
#include <SharedCppLib2/sha256.hpp>

std::bytearray key(std::string("secret-key"));
std::bytearray message(std::string("hello"));

std::bytearray tag = scl2::hmac<scl2::sha256>::compute(message, key);
std::cout << "HMAC-SHA256: " << tag.toHex() << std::endl;
```

## 核心 API

### 模板定义
```cpp
template<typename HashClass>
requires has_hashing_support<HashClass>
class hmac;
```

### compute
```cpp
static std::bytearray compute(const std::bytearray& message, const std::bytearray& key);
```

用于计算二进制消息的 HMAC 标签。

### 常量

- `hmac<HashClass>::block_size`：HMAC 归一化密钥时使用的块大小
- `hmac<HashClass>::result_size`：若 provider 声明固定摘要长度则为该值，否则为 `0`

## 算法流程

1. 将密钥归一化到一个块：
   - 若 `key.size() > block_size`，先执行 `Hash(key)`
   - 右侧补 `0x00` 至 `block_size`
2. 构造填充块：
   - `ipad = key_block XOR 0x36`
   - `opad = key_block XOR 0x5c`
3. 计算：
   - `inner = Hash(ipad || message)`
   - `tag = Hash(opad || inner)`

## 二进制消息验签流程

### 发送端

- 按固定字段顺序构造 canonical `std::bytearray` 负载。
- 计算 `tag = hmac<Provider>::compute(payload, key)`。
- 发送 `payload + tag`（或将 tag 放入头部/元信息）。

### 接收端

- 解析收到的 payload 与 tag。
- 用同一密钥重算期望 tag。
- 使用常量时间比较对比两者。
- 一致才接受消息。

详见专题文档：[constant_time_compare](constant_time_compare.md)

## 注意事项

- HMAC 提供完整性与来源认证，不提供保密性。
- 需要保密时，请叠加加密算法。
- 需要防重放时，请将时间戳/nonce 纳入签名内容。
