# SharedCppLib2 API Reference

Document version: 0.3.0

SharedCppLib2 is now building its own api (compatible layer). It provides a series of standard for you to use with your own implementation, which allows your code to directly interact with SharedCppLib2 with extremely low learning cost.

The definitions are in `api.hpp`. This file is header-only.


## Own class support

This part contains a series of standard for you to follow when implementing your own class.

By doing so, you gain access to a series of convenient features made by SharedCppLib2.

### String (serialize/deserialize) API layer

An example of the serialize/deserialize API layer is as follows:

```cpp
class MyClass {
    // your class things

public: // The api must be in the public section

    std::string serialize() const; // for serialize
    static MyClass deserialize(const std::string& str); // for deserialize
}

// This is a macro that checks if the class is actually compatible with the api layer. It will cause a compile error if the class does not have the required functions.
scl2_check_generic_serialize_deserialize(MyClass)

```

You can also use the alternative spelling `serialise` and `deserialise` if you prefer British English.

The `serialize` and `deserialize` layer is separated, meaning that you can implement one without the other if you only need one of the functionalities. In that case, you can use the `scl2_check_generic_serialize` or `scl2_check_generic_deserialize` macros to check for the presence of the required functions.


### Bytearray (dump/load) API layer

This is the key part of SharedCppLib2. This API layer allows you to convert almost anything to a byte array or convert it back.

Implement `dump()` and `load()` in your class to make it compatible with `gdump`/`gload`:

```cpp
struct MyData {
    int d1;
    double d2;
    std::string d3;
    std::vector<int> d4;

    scl2::bytearray dump() const {
        scl2::bytearray ba;

        // trivially copyable types can be directly appended
        ba.append(d1);
        ba.append(d2);

        // for strings, use append (length-prefixed automatically)
        ba.append(d3);

        // for STL-compatible containers, use appendContainer
        ba.appendContainer(d4);

        return ba;
    }

    void load(scl2::bytearray& ba) {
        // read in the same order as written
        d1 = ba.read<int>();
        d2 = ba.read<double>();
        d3 = ba.readString();
        d4 = ba.readContainer<std::vector<int>>();
    }
};

// Verify compatibility at compile time
scl2_check_generic_dump(MyData)
scl2_check_generic_load(MyData)
```

### Nested structures

For nested types, call `gdump` / `gload` recursively:

```cpp
class MyClass {
    int meta;
    std::vector<MyData> datal;

public:
    scl2::bytearray dump() const {
        scl2::bytearray ba;
        ba.append(meta);

        // Write container size first, then each element
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

The built-in read cursor in `bytearray` advances automatically with each `read()` call. Just read in the same order you wrote — the cursor handles positions for you.

For further details into error handling and advanced features, refer to [Bytearray](bytearray.md).


### Encryption API layer

This part is for encryption algorithm developers (**providers**), not for general users. If you just want to encrypt your data, go check [Encryption API Usage](#encryption-api-usage).

A provider class must satisfy the `has_encryption_support` (or `has_decryption_support`) concept:

```cpp
// Static API (one-shot):
class MyCipher {
public:
    using key_type = scl2::bytearray;
    static scl2::bytearray encrypt(const scl2::bytearray& data, const scl2::bytearray& key);
    static scl2::bytearray decrypt(const scl2::bytearray& data, const scl2::bytearray& key);
};

// Instance API (pre-configured key):
class MyCipher {
public:
    using key_type = scl2::bytearray;
    explicit MyCipher(const scl2::bytearray& key);
    scl2::bytearray encrypt(const scl2::bytearray& data) const;
    scl2::bytearray decrypt(const scl2::bytearray& data) const;
};
```

Providers may also expose:
- `static constexpr size_t key_size` — fixed key size (checked by `has_fixed_key_size`)
- `class stream_type` — streaming support (checked by `has_streamed_encryption`)

A streaming cipher follows the `begin/update/end` pattern:

```cpp
MyCipher::stream_type cipher(key, cipher_dir::Encrypt);
cipher.update(chunk1);  // returns encrypted blocks
cipher.update(chunk2);
auto last = cipher.end();  // returns final blocks with padding
```

### Hashing API layer

This is also for developers (**providers**). If you just want to hash your data, check [Hashing API Usage](#hashing-api-usage).

A hash provider must satisfy the `has_hashing_support` concept:

```cpp
class MyHash {
public:
    static constexpr size_t result_size = 32;  // optional, enables has_fixed_hash_result_size
    static constexpr size_t block_size = 64;    // optional, enables generic_buffer_size

    static scl2::bytearray hash(const scl2::bytearray& data);

    // Streaming support (optional, checked by has_streamed_hash):
    class stream_type {
    public:
        stream_type();
        void update(const scl2::bytearray& chunk);
        scl2::bytearray end();
    };
};
```




## API Usage

### Encryption API Usage

Any class that satisfies `has_encryption_support` can be used with the generic wrappers or directly.

```cpp
#include <SharedCppLib2/aes.hpp>

scl2::bytearray data = /* your data */;
scl2::bytearray key(static_cast<size_t>(16), std::byte{0});

// Direct call:
auto ct = scl2::aes_ecb_128::encrypt(data, key);
auto pt = scl2::aes_ecb_128::decrypt(ct, key);

// Generic wrapper (works with any provider):
auto ct = scl2::generic_encrypt<scl2::aes_ecb_128>(data, key);
auto pt = scl2::generic_decrypt<scl2::aes_ecb_128>(ct, key);

// Instance API (pre-configured key):
scl2::aes_ecb_128 cipher(key);
auto ct = cipher.encrypt(data);
auto pt = cipher.decrypt(ct);

// Streaming (large data):
scl2::aes_ecb_128::stream_type enc(key, scl2::cipher_dir::Encrypt);
// enc.update(chunk) → encrypted blocks
// enc.end()         → final block with padding
```

### Hashing API Usage

Any class that satisfies `has_hashing_support` can be used:

```cpp
#include <SharedCppLib2/sha256.hpp>

scl2::bytearray data = /* your data */;

// Direct call:
scl2::bytearray hash = scl2::sha256::hash(data);

// Generic wrapper:
scl2::bytearray hash = scl2::generic_hash<scl2::sha256>(data);

// Hex string:
std::string hex = hash.toHex();

// Streaming (large data):
scl2::sha256::stream_type hasher;
hasher.update(chunk1);
hasher.update(chunk2);
scl2::bytearray digest = hasher.end();

// Stream from istream:
std::ifstream file("data.bin", std::ios::binary);
auto digest = scl2::hash_stream<scl2::sha256>(file);
```