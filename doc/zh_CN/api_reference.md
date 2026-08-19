# SharedCppLib2 API 参考

文档版本: 0.3.0

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

在你的类中实现 `dump()` 和 `load()`，即可与 `gdump`/`gload` 兼容：

```cpp
struct MyData {
    int d1;
    double d2;
    std::string d3;
    std::vector<int> d4;

    scl2::bytearray dump() const {
        scl2::bytearray ba;

        // 可平凡拷贝类型可以直接追加
        ba.append(d1);
        ba.append(d2);

        // 字符串使用 append（自动添加长度前缀）
        ba.append(d3);

        // 对于 STL 兼容容器，使用 appendContainer
        ba.appendContainer(d4);

        return ba;
    }

    void load(scl2::bytearray& ba) {
        // 按写入顺序读取
        d1 = ba.read<int>();
        d2 = ba.read<double>();
        d3 = ba.readString();
        d4 = ba.readContainer<std::vector<int>>();
    }
};

// 编译期验证兼容性
scl2_check_generic_dump(MyData)
scl2_check_generic_load(MyData)
```

### 嵌套结构

对于嵌套类型，递归调用 `gdump` / `gload`：

```cpp
class MyClass {
    int meta;
    std::vector<MyData> datal;

public:
    scl2::bytearray dump() const {
        scl2::bytearray ba;
        ba.append(meta);

        // 先写入容器大小，再写入每个元素
        ba.append(static_cast<uint32_t>(datal.size()));
        for (const auto& item : datal)
            ba.append(scl2::gdump(item));

        return ba;
    }

    void load(scl2::bytearray& ba) {
        meta = ba.read<int>();

        uint32_t size = ba.read<uint32_t>();
        datal.reserve(size);
        for (uint32_t i = 0; i < size; ++i)
            datal.push_back(scl2::gload<MyData>(ba));
    }
};
```

`bytearray` 内置的读取游标在每次 `read()` 调用后自动前进。只需按写入顺序读取——游标会自动处理位置。

有关错误处理和高级功能，请参考 [Bytearray](bytearray.md)。


### 加密 API 层

这部分面向加密算法开发者（**提供者**），而非普通用户。如果你只想加密数据，请查看[加密 API 用法](#加密-api-用法)。

提供者类必须满足 `has_encryption_support`（或 `has_decryption_support`）概念：

```cpp
// 静态 API（一次性）：
class MyCipher {
public:
    using key_type = scl2::bytearray;
    static scl2::bytearray encrypt(const scl2::bytearray& data, const scl2::bytearray& key);
    static scl2::bytearray decrypt(const scl2::bytearray& data, const scl2::bytearray& key);
};

// 实例 API（预配置密钥）：
class MyCipher {
public:
    using key_type = scl2::bytearray;
    explicit MyCipher(const scl2::bytearray& key);
    scl2::bytearray encrypt(const scl2::bytearray& data) const;
    scl2::bytearray decrypt(const scl2::bytearray& data) const;
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

    static scl2::bytearray hash(const scl2::bytearray& data);

    // 流式支持（可选，由 has_streamed_hash 检查）：
    class stream_type {
    public:
        stream_type();
        void update(const scl2::bytearray& chunk);
        scl2::bytearray end();
    };
};
```


## API 用法

### 加密 API 用法

任何满足 `has_encryption_support` 的类都可以通过通用包装器或直接调用使用。

```cpp
#include <SharedCppLib2/aes.hpp>

scl2::bytearray data = /* 你的数据 */;
scl2::bytearray key(static_cast<size_t>(16), std::byte{0});

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

scl2::bytearray data = /* 你的数据 */;

// 直接调用：
scl2::bytearray hash = scl2::sha256::hash(data);

// 通用包装器：
scl2::bytearray hash = scl2::generic_hash<scl2::sha256>(data);

// 十六进制字符串：
std::string hex = hash.toHex();

// 流式处理（大文件）：
scl2::sha256::stream_type hasher;
hasher.update(chunk1);
hasher.update(chunk2);
scl2::bytearray digest = hasher.end();

// 从 istream 流式处理：
std::ifstream file("data.bin", std::ios::binary);
auto digest = scl2::hash_stream<scl2::sha256>(file);
```
