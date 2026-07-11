# crc32 - CRC32 校验和库

+ 名称: crc32  
+ 命名空间: `scl2`  
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `crc32` |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::crc32)
```

```cpp
#include <SharedCppLib2/crc32.hpp>
```

## 描述

CRC-32（循环冗余校验）是一种广泛用于错误检测的校验和算法。它使用标准 Ethernet / PKZip / zlib 多项式（`0xEDB88320`，反射模式）生成 32 位（4 字节）值。常见应用包括文件完整性验证、网络数据包校验和数据去重。

**CRC32 不是加密哈希**——它专为错误检测设计，不应用于安全关键场景。

## 快速开始

### 一键计算

```cpp
scl2::bytearray data = scl2::bytearray("Hello, World!");
scl2::bytearray digest = scl2::crc32::hash(data);
// digest 为 4 字节，大端序
```

### 流式处理（大块数据）

```cpp
scl2::crc32::stream_type hasher;
hasher.update(chunk1);
hasher.update(chunk2);
// ... 传入任意数量的数据块
scl2::bytearray digest = hasher.end();
```

### 从输入流读取

```cpp
#include <SharedCppLib2/hash_api.hpp>

std::ifstream file("large.bin", std::ios::binary);
auto digest = scl2::hash_stream<scl2::crc32>(file);
```

### 文件完整性校验

```cpp
std::ifstream file("data.bin", std::ios::binary);
auto expected = scl2::bytearray::fromHex("cbf43926");
auto actual = scl2::hash_stream<scl2::crc32>(file);
bool ok = (expected == actual);
```

## 测试向量

| 输入 | CRC32 (十六进制) |
|-------|-------------|
| `""` (空字符串) | `00000000` |
| `"abc"` | `352441C2` |
| `"123456789"` | `CBF43926`（标准验证值） |
