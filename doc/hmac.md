# hmac - HMAC Library

+ Name: hmac  
+ Namespace: `scl2`  
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `hmac` (INTERFACE) |

Generic usage (no fixed hash provider):
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::hmac)
```

If this example uses SHA256 as provider:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::hmac SharedCppLib2::sha256)
```

## Description

`hmac` provides a header-only template implementation of Hash-based Message Authentication Code.
It works with any hash provider class that satisfies SharedCppLib2 hashing API:

```cpp
static std::bytearray hash(const std::bytearray& data);
```

Optional provider metadata:

- `static constexpr size_t block_size` (defaults to `64` if absent)
- `static constexpr size_t result_size` (if absent, digest length is treated as dynamic)

## Quick Start

### Generic include

```cpp
#include <SharedCppLib2/hmac.hpp>
```

`hmac` itself does not depend on a specific hash algorithm.
You provide the hash provider class as template argument.

### Example provider: SHA256

```cpp
#include <SharedCppLib2/hmac.hpp>
#include <SharedCppLib2/sha256.hpp>

std::bytearray key(std::string("secret-key"));
std::bytearray message(std::string("hello"));

std::bytearray tag = scl2::hmac<scl2::sha256>::compute(message, key);
std::cout << "HMAC-SHA256: " << tag.toHex() << std::endl;
```

## Core API

### Template
```cpp
template<typename HashClass>
requires has_hashing_support<HashClass>
class hmac;
```

### compute
```cpp
static std::bytearray compute(const std::bytearray& message, const std::bytearray& key);
```

Computes HMAC tag for binary message data.

### Constants

- `hmac<HashClass>::block_size`: block size used by HMAC key normalization
- `hmac<HashClass>::result_size`: expected digest size if provider exposes fixed result size; otherwise `0`

## Algorithm Flow

1. Normalize key to one block:
   - if `key.size() > block_size`, replace key with `Hash(key)`
   - right-pad with `0x00` until `block_size`
2. Build pads:
   - `ipad = key_block XOR 0x36`
   - `opad = key_block XOR 0x5c`
3. Compute:
   - `inner = Hash(ipad || message)`
   - `tag = Hash(opad || inner)`

## Binary Message Verification Pattern

### Sender

- Build canonical binary payload (`std::bytearray`) with fixed field order.
- Compute `tag = hmac<Provider>::compute(payload, key)`.
- Send `payload + tag` (or place tag in metadata/header).

### Receiver

- Parse received payload and tag.
- Recompute expected tag with same key.
- Compare tags with constant-time comparison helper.
- Accept only if tags are equal.

See dedicated guide: [constant_time_compare](constant_time_compare.md)

## Notes

- HMAC provides integrity and authenticity, not confidentiality.
- For confidentiality, combine with encryption.
- For replay protection, include timestamp/nonce in signed payload.
