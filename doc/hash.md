# Hash Providers

SharedCppLib2 provides a unified hashing API (`hash_api.hpp`) plus a set of hash providers implementing a common interface. This document lists all available hash algorithms and shows how to use them directly or through the generic API.

Each provider exposes:

```cpp
class provider {
public:
    static constexpr size_t result_size;   // digest length in bytes
    static constexpr size_t block_size;    // block length in bytes
    static scl2::bytearray hash(const scl2::bytearray& data); // one-shot
    class stream_type { /* update() / end() */ };            // streaming
};
```

## Available Providers

| Provider | Digest | Algorithm | Library target | Use case |
|----------|--------|-----------|----------------|----------|
| `scl2::sha512` | 64 bytes | SHA-512 (FIPS 180-4) | `SharedCppLib2::sha512` | High-strength hashing, signatures |
| `scl2::sha256` | 32 bytes | SHA-256 (FIPS 180-4) | `SharedCppLib2::sha256` | General-purpose security hashing |
| `scl2::sha1` | 20 bytes | SHA-1 (FIPS 180-4) | `SharedCppLib2::sha1` | Legacy / non-security indexing |
| `scl2::crc32` | 4 bytes | CRC-32 (0xEDB88320) | `SharedCppLib2::crc32` | Fast error detection / checksum |

> [!WARNING]
> **`sha1` is cryptographically broken** (SHAttered collision attack). Use it only for legacy compatibility or non-security indexing (e.g. resource archive keys). **`crc32` is not a cryptographic hash** — it detects accidental corruption, not malicious tampering.

## Quick Start

### One-shot hashing

```cpp
#include <SharedCppLib2/sha256.hpp>

scl2::bytearray data = scl2::bytearray("Hello, World!");
scl2::bytearray digest = scl2::sha256::hash(data);
std::cout << digest.toHex() << std::endl;  // 64 hex chars
```

### Streaming hashing (large data)

```cpp
#include <SharedCppLib2/sha256.hpp>

scl2::sha256::stream_type hasher;
hasher.update(chunk1);
hasher.update(chunk2);
scl2::bytearray digest = hasher.end();  // 32 bytes
```

## Generic Hashing API

`hash_api.hpp` provides templates that work with any provider. This is useful when writing generic code that should accept multiple algorithms.

> [!IMPORTANT]
> When using the generic API, you must include **both** the algorithm header **and** `hash_api.hpp`:
>
> ```cpp
> #include <SharedCppLib2/hash_api.hpp>   // generic_hash<T>, hash_stream<T>
> #include <SharedCppLib2/sha256.hpp>     // the concrete provider
> ```

```cpp
#include <SharedCppLib2/hash_api.hpp>
#include <SharedCppLib2/sha256.hpp>

// Generic one-shot hash
scl2::bytearray d = scl2::generic_hash<scl2::sha256>(data);

// Compile-time digest size
static_assert(scl2::generic_hash_result_size<scl2::sha256>() == 32);
```

### Streaming from an istream

```cpp
#include <SharedCppLib2/hash_api.hpp>
#include <SharedCppLib2/sha512.hpp>

std::ifstream file("large.bin", std::ios::binary);
scl2::bytearray digest = scl2::hash_stream<scl2::sha512>(file);
```

## HMAC

All hash providers can be used with `scl2::hmac<Provider>`:

```cpp
#include <SharedCppLib2/hmac.hpp>
#include <SharedCppLib2/sha256.hpp>

scl2::bytearray key   = scl2::bytearray("secret");
scl2::bytearray data  = scl2::bytearray("message");
scl2::bytearray mac   = scl2::hmac<scl2::sha256>::digest(key, data);
```

## See Also

- [`sha256`](sha256.md) — SHA-256 provider details
- [`crc32`](crc32.md) — CRC-32 provider details
- [`hmac`](hmac.md) — HMAC keyed hashing
