# SharedCppLib2 API 参考

文档版本: 0.2.0

SharedCppLib2 正在构建自己的 API（兼容层）。它提供了一系列标准，使得你的代码可以与 SharedCppLib2 直接交互，学习成本极低。

定义位于 `api.hpp`。该文件是 header-only 的。

## 自定义类支持

本节包含一系列标准，供你在实现自己的类时遵循。

遵循这些标准后，你的类将获得 SharedCppLib2 提供的一系列便捷功能。

### 字符串（serialize/deserialize）API 层

serialize/deserialize API 层的示例如下：

```cpp
class MyClass {
    // 你的类内容

public: // API 必须在 public 区域

    std::string serialize() const; // 序列化
    static MyClass deserialize(const std::string& str); // 反序列化
}

// 该宏检查类是否与 API 层兼容。如果类缺少必要函数，将导致编译错误。
scl2_check_generic_serialize_deserialize(MyClass)

```

如果你更喜欢英式拼写，也可以使用 `serialise` 和 `deserialise`。

`serialize` 和 `deserialize` 层是分离的，意味着如果你只需要其中一种功能，可以只实现一个。在这种情况下，可以使用 `scl2_check_generic_serialize` 或 `scl2_check_generic_deserialize` 宏来检查所需函数是否存在。

### Bytearray（dump/load）API 层

这是 SharedCppLib2 的关键部分。此 API 层允许你将几乎任何内容转换为字节数组，或将其还原。

这是一个强大的 API 集，因为你可以将字节数组用于文件、网络、管道或任何其他用途。

数据包的转换从未如此简单。

以下是 bytearray（dump/load）API 层的示例：

```cpp
struct MyData {
    int d1;
    double d2;

    std::string d3;
    std::vector<int> d4;

    std::bytearray dump(const MyData& data) const {
        std::bytearray ba;

        // 数字类型可以直接追加到 bytearray
        ba.append(data.d1);
        ba.append(data.d2);

        // 字符串使用 addString()
        ba.addString(data.d3);

        // 注意：宽字符串使用 addWString()

        // 对于 STL 兼容容器，使用 appendContainer()
        // 它会自动处理所有大小和元素
        ba.appendContainer(data.d4);

        return ba;
    }


    // 使用 bytearray_view（必须是引用）进行加载。
    // bytearray_view 处理所有读取操作，
    // bytearray 本身没有这些操作。
    MyData load(const std::bytearray_view& ba) const {
        MyData data;

        // 数字类型可以直接从 bytearray_view 读取
        data.d1 = ba.read<int>();
        data.d2 = ba.read<double>();

        // 字符串使用 readString()
        data.d3 = ba.readString();

        // 注意：宽字符串使用 readWString()

        // 对于 STL 兼容容器，使用 readContainer()
        // 它会自动处理所有大小和元素
        data.d4 = ba.readContainer<std::vector<int>>();

        return data;
    }
};

```

这是一个简单的示例。对于嵌套结构，你也可以递归地使用 dump/load：

```cpp

class MyClass {
    int meta;
    std::vector<MyData> datal;

public:
    std::bytearray dump(const MyClass& data) const {
        std::bytearray ba;

        ba.append(data.meta);
        
        // 目前不支持非基本类型的容器。
        // 不过处理起来仍然很简单。

        // 首先写入容器的大小。
        // 确保使用 appendSize() 以获得正确的类型。
        // 相信我，你不会想调试这个的。
        ba.appendSize(data.datal.size());

        for(const auto& item : data.datal) {
            // 然后使用元素自己的 dump 函数写入每个元素。
            ba.append(MyData::dump(item));
        }

        return ba;
    }

    MyClass load(const std::bytearray_view& ba) const {
        MyClass data;

        data.meta = ba.read<int>();

        // 首先读取容器的大小。
        size_t size = ba.readSize();
        data.datal.reserve(size); // 为容器预留空间
        for(size_t i = 0; i < size; ++i) {
            // 然后使用元素自己的 load 函数读取每个元素。
            data.datal.push_back(MyData::load(ba));
        }

        return data;
    }
};

```

以上示例展示了 bytearray_view 层的全部潜力。你可以从单个源数组中串联调用 load 函数，所有数据映射都会自动处理。

你只需要按顺序读取。确保两端的处理过程完全一致。你可以应用基本的压缩策略，例如对于一个包含 3 种不同类型的数据包，你不需要为其他类型预留数据。

不过，确保代码的健壮性仍然是你的责任。上述示例不包含任何错误处理，如果出现问题（如进入无效状态或数据在传输过程中被截断），很可能会崩溃。

有关错误处理的更多细节，请参考 [Bytearray](bytearray.md)。


### 加密 API 层

这部分面向加密算法开发者（**提供者**），而非普通用户。如果你只想加密数据，请查看[加密 API 用法](#加密-api-用法)。

提供者类必须满足 `has_encryption_support`（或 `has_decryption_support`）概念：

```cpp
// 静态 API（一次性）：
class MyCipher {
public:
    using key_type = std::bytearray;
    static std::bytearray encrypt(const std::bytearray& data, const std::bytearray& key);
    static std::bytearray decrypt(const std::bytearray& data, const std::bytearray& key);
};

// 实例 API（预配置密钥）：
class MyCipher {
public:
    using key_type = std::bytearray;
    explicit MyCipher(const std::bytearray& key);
    std::bytearray encrypt(const std::bytearray& data) const;
    std::bytearray decrypt(const std::bytearray& data) const;
};
```

提供者还可以暴露：
- `static constexpr size_t key_size` — 固定密钥大小（由 `has_fixed_key_size` 检查）
- `class stream_type` — 流式支持（由 `has_streamed_encryption` 检查）

流式加密遵循 begin/update/end 模式：

```cpp
MyCipher::stream_type cipher(key, cipher_dir::Encrypt);
cipher.update(chunk1);  // 返回加密块
cipher.update(chunk2);
auto last = cipher.end();  // 返回带填充的最终块
```

### 哈希 API 层

这部分也面向开发者（**提供者**）。如果你只想计算哈希值，请查看[哈希 API 用法](#哈希-api-用法)。

哈希提供者必须满足 `has_hashing_support` 概念：

```cpp
class MyHash {
public:
    static constexpr size_t result_size = 32;  // 可选，启用 has_fixed_hash_result_size
    static constexpr size_t block_size = 64;    // 可选，启用 generic_buffer_size

    static std::bytearray hash(const std::bytearray& data);

    // 流式支持（可选，由 has_streamed_hash 检查）：
    class stream_type {
    public:
        stream_type();
        void update(const std::bytearray& chunk);
        std::bytearray end();
    };
};
```


## API 用法

### 加密 API 用法

任何满足 `has_encryption_support` 的类都可以通过通用包装器或直接调用使用。

```cpp
#include <SharedCppLib2/aes.hpp>

std::bytearray data = /* 你的数据 */;
std::bytearray key(static_cast<size_t>(16), std::byte{0});

// 直接调用：
auto ct = scl2::aes_ecb_128::encrypt(data, key);
auto pt = scl2::aes_ecb_128::decrypt(ct, key);

// 通用包装器（适用于任何提供者）：
auto ct = scl2::generic_encrypt<scl2::aes_ecb_128>(data, key);
auto pt = scl2::generic_decrypt<scl2::aes_ecb_128>(ct, key);

// 实例 API（预配置密钥）：
scl2::aes_ecb_128 cipher(key);
auto ct = cipher.encrypt(data);
auto pt = cipher.decrypt(ct);

// 流式处理（大文件）：
scl2::aes_ecb_128::stream_type enc(key, scl2::cipher_dir::Encrypt);
// enc.update(chunk) → 加密块
// enc.end()         → 带填充的最终块
```

### 哈希 API 用法

任何满足 `has_hashing_support` 的类都可以使用：

```cpp
#include <SharedCppLib2/sha256.hpp>

std::bytearray data = /* 你的数据 */;

// 直接调用：
std::bytearray hash = scl2::sha256::hash(data);

// 通用包装器：
std::bytearray hash = scl2::generic_hash<scl2::sha256>(data);

// 十六进制字符串：
std::string hex = hash.toHex();

// 流式处理（大文件）：
scl2::sha256::stream_type hasher;
hasher.update(chunk1);
hasher.update(chunk2);
std::bytearray digest = hasher.end();

// 从 istream 流式处理：
std::ifstream file("data.bin", std::ios::binary);
auto digest = scl2::hash_stream<scl2::sha256>(file);
```
