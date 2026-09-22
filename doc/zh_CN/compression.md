# compression - 压缩 provider 与算法标识符

+ 名称: compression
+ 命名空间: `scl2`
+ 文档版本: `1.0.0`

## CMake 信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库 | `compression` |
| 依赖 | `basic` |

引入方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::compression)
```

## 描述

在程序运行期间选择压缩算法。

`compression_api.hpp` 描述的是"一个 provider 长什么样"，库按类型匹配 provider。这在选择本身是运行时决定的时候就不够用了 —— 比如文件记录了自己是用哪个算法写的，或者由应用来配置一个。这个头补上的就是这种情况需要的两样东西：一个抹掉类型的外壳，和一个文件能记录的标识符。

## 快速开始

```cpp
#include <SharedCppLib2/compression.hpp>
#include <SharedCppLib2/zlib.hpp>

scl2::compression_provider provider = scl2::compression_provider::from<scl2::zlib>();

scl2::bytearray packed = provider.compress(data);
scl2::bytearray back   = provider.decompress(packed);
```

自己的算法也不必先包成类型：

```cpp
scl2::compression_provider mine = scl2::compression_provider::from(
    [](const scl2::bytearray& in) { return compressWith(in); },
    [](const scl2::bytearray& in) { return decompressWith(in); });
```

## 标识符

```cpp
enum class compress_algo : uint8_t { none = 0, zlib = 1 };

inline constexpr uint8_t user_compression_base = 128;
```

小于 `user_compression_base` 的值属于库，含义固定。从 `user_compression_base` 往上属于应用，由应用自己挑号。中间留出的空档是为了让库以后能加算法，而不至于和某个应用已经选走的号撞上。

`compression_id` 就是传递这种值的类型：从 `compress_algo` 直接构造，从裸数字则要显式：

```cpp
scl2::compression_id builtin = scl2::compress_algo::zlib;
scl2::compression_id mine{uint8_t{200}};
scl2::compression_id nothing;        // 0，表示没有算法
```

## 接口

### compression_provider

| 函数 | 说明 |
|---------|---------|
| `static from<T>()` | 包装一个符合 `compression_api.hpp` 描述的 provider |
| `static from(compress, decompress)` | 直接包装一对函数；任一半可以留空 |
| `hasCompression()` / `hasDecompression()` | 哪一半在 |
| `isUsable()` | 两半都在 |
| `compress(data)` / `decompress(data)` | 执行其中一半；那一半缺失时抛 `std::runtime_error` |
| `verify()` / `verify(probe)` | 把一段探针跑一遍两半，报告是否原样回来 |
| `static defaultProbe()` | `verify()` 不给探针时用的那 256 字节 |

### compression_id

| 函数 | 说明 |
|---------|---------|
| `compression_id(compress_algo)` | 指一个内置算法 |
| `explicit compression_id(uint8_t)` | 指一个应用自己选的号 |
| `value()` | 那个数字 |
| `isNone()` | 是不是 0 |
| `isBuiltin()` / `isUser()` | 落在哪个区间 |
| `operator==` | 指同一个号就算相等 |

## 注意

> [!IMPORTANT]
> **标识符不描述算法。** 两个程序能互相读文件，前提是它们对每个数字的含义有共识，而库无法检查这件事。这和一个协议版本号是同一类约定：它属于写这个格式的人。

> [!NOTE]
> **`verify()` 是检查，不是证明。** 它只说明两半在**给它的那段探针上**一致，所以"只在别的输入上不配套"的一对会通过，"解压端其实期待另一种算法"也会通过。它本身也从不会自动运行 —— 库在保存和打开路径上一次都不调它，这是有意的：Release 构建相信输入。要在自己的测试里调用它，并且用一段像你真实数据的探针。

> [!WARNING]
> **两半不配套是要付数据代价的。** 用一个函数压、用另一个不是它逆函数的解，得到的文件读不回来：长度和结构对不上，所以打开时会失败，而不是返回错值。如果那次保存覆盖了唯一的副本，数据就没了。在把重要的东西交给一对函数之前先检查它，并且记住"小输入上正常"完全不能说明大输入。

## 相关模块

- [bytearray](bytearray.md) —— 进出的数据
- [xkeydb](xkeydb.md) —— 用这套东西把算法记进文件里的模块
