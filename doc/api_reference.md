# SharedCppLib2 API Reference

Document version: 0.2.0

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

This is the key part of the SharedCppLib2. This api layer allows you to convert almost anything to a byte array or convert it back.

This is a powerful api set, since you can then use the byte array for files, internet, pipelines or anything else.

Converting data packs have not been that easy before.

This is an example for the bytearray (dump/load) api layer:

```cpp
struct MyData {
    int d1;
    double d2;

    std::string d3;
    std::vector<int> d4;

    std::bytearray dump(const MyData& data) const {
        std::bytearray ba;

        // numeric types can be directly appended to the bytearray
        ba.append(data.d1);
        ba.append(data.d2);

        // for string, use addString().
        ba.addString(data.d3);

        // Note: for wstring, use addWString().

        // for STL-compatible containers, use appendContainer().
        // It will automatically handle all sizes and elements for you.
        ba.appendContainer(data.d4);

        return ba;
    }


    // Use bytearray_view (must be a reference) for load.
    // The bytearray_view handles all read stuff, and those
    // does not exist in bytearray itself. 
    MyData load(const std::bytearray_view& ba) const {
        MyData data;

        // numeric types can be directly read from the bytearray_view
        data.d1 = ba.read<int>();
        data.d2 = ba.read<double>();

        // for string, use readString().
        data.d3 = ba.readString();

        // Note: for wstring, use readWString().

        // for STL-compatible containers, use readContainer().
        // It will automatically handle all sizes and elements for you.
        data.d4 = ba.readContainer<std::vector<int>>();

        return data;
    }
};

```

This is a simple example. For nested structures, you can recursively use dump/load as well:

```cpp

class MyClass {
    int meta;
    std::vector<MyData> datal;

public:
    std::bytearray dump(const MyClass& data) const {
        std::bytearray ba;

        ba.append(data.meta);
        
        // Currently, containers of non-primitive types are not supported.
        // However, handling them is still easy.

        // first, write the size of the container.
        // Make sure to use appendSize() to get the correct type even in the future.
        // Trust me, you won't want to debug this.
        ba.appendSize(data.datal.size());

        for(const auto& item : data.datal) {
            // then, write each element using its own dump function.
            ba.append(MyData::dump(item));
        }

        return ba;
    }

    MyClass load(const std::bytearray_view& ba) const {
        MyClass data;

        data.meta = ba.read<int>();

        // read the size of the container first.
        size_t size = ba.readSize();
        data.datal.reserve(size); // reserve space for the container
        for(size_t i = 0; i < size; ++i) {
            // then, read each element using its own load function.
            data.datal.push_back(MyData::load(ba));
        }

        return data;
    }
};

```

The above example shows the full potential of the bytearray_view layer. You can call the load function in series from a single source array, and all data mappings are automatically handled.

You just need to read. Make sure the process at both sides are identical. You can apply basic-level compressions, like for a data pack with 3 different types, you don't need to write the data reserved for the other types.

However, it is still your responsibility to make sure your code is robust enough. The above example does not contain any error handling, and will likely crash if something went wrong, like entered an invalid state or the data got truncated during transmission.

For furthur details into error handling, refer to [Bytearray](bytearray.md).


### Encryption API layer

This part is for encryption algorithm developers (**providers**), not for general users. If you just want to encrypt your data, go check [Encryption API Usage](#encryption-api-usage).

A provider class must satisfy the `has_encryption_support` (or `has_decryption_support`) concept:

```cpp
// Static API (one-shot):
class MyCipher {
public:
    using key_type = std::bytearray;
    static std::bytearray encrypt(const std::bytearray& data, const std::bytearray& key);
    static std::bytearray decrypt(const std::bytearray& data, const std::bytearray& key);
};

// Instance API (pre-configured key):
class MyCipher {
public:
    using key_type = std::bytearray;
    explicit MyCipher(const std::bytearray& key);
    std::bytearray encrypt(const std::bytearray& data) const;
    std::bytearray decrypt(const std::bytearray& data) const;
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

    static std::bytearray hash(const std::bytearray& data);

    // Streaming support (optional, checked by has_streamed_hash):
    class stream_type {
    public:
        stream_type();
        void update(const std::bytearray& chunk);
        std::bytearray end();
    };
};
```




## API Usage

### Encryption API Usage

Any class that satisfies `has_encryption_support` can be used with the generic wrappers or directly.

```cpp
#include <SharedCppLib2/aes.hpp>

std::bytearray data = /* your data */;
std::bytearray key(static_cast<size_t>(16), std::byte{0});

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

std::bytearray data = /* your data */;

// Direct call:
std::bytearray hash = scl2::sha256::hash(data);

// Generic wrapper:
std::bytearray hash = scl2::generic_hash<scl2::sha256>(data);

// Hex string:
std::string hex = hash.toHex();

// Streaming (large data):
scl2::sha256::stream_type hasher;
hasher.update(chunk1);
hasher.update(chunk2);
std::bytearray digest = hasher.end();

// Stream from istream:
std::ifstream file("data.bin", std::ios::binary);
auto digest = scl2::hash_stream<scl2::sha256>(file);
```