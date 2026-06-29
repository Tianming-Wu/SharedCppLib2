# aes - AES Encryption Module

+ Name: aes
+ Namespace: `scl2::crypto` (inline)
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `aes` |

Include usage:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::aes)
```

```cpp
#include <SharedCppLib2/aes.hpp>
```

## Description

`aes` provides AES-128, AES-192, and AES-256 encryption and decryption with ECB and CBC modes, PKCS7 padding, and full FIPS 197 compliance. Both static one-shot and instance-based APIs are supported.

The AES classes implement the `encryption_api` concepts, making them interoperable with any code written against that API.

## Quick Start

### ECB mode (one-shot)

```cpp
#include <SharedCppLib2/aes.hpp>

// 16-byte key for AES-128
std::bytearray key(static_cast<size_t>(16), std::byte{0});
std::bytearray pt = std::bytearray(std::string("Hello, AES!"));

// Encrypt
auto ct = scl2::aes_ecb_128::encrypt(pt, key);

// Decrypt
auto dec = scl2::aes_ecb_128::decrypt(ct, key);
// dec.toStdString() == "Hello, AES!"
```

### CBC mode

```cpp
// key(16) + iv(16) = 32 bytes for AES-128-CBC
std::bytearray keyiv(static_cast<size_t>(32), std::byte{0});

auto ct = scl2::aes_cbc_128::encrypt(pt, keyiv);
auto dec = scl2::aes_cbc_128::decrypt(ct, keyiv);
```

### Instance API (pre-configured key)

```cpp
scl2::aes_ecb_256 cipher(key32);
auto ct = cipher.encrypt(pt);
auto dec = cipher.decrypt(ct);
```

## Class Reference

### Template classes

```cpp
template<size_t KeyBits> class aes_ecb;
template<size_t KeyBits> class aes_cbc;
```

`KeyBits` must be 128, 192, or 256.

### Convenience aliases

```cpp
using aes_ecb_128 = aes_ecb<128>;   // 16-byte key, 10 rounds
using aes_ecb_192 = aes_ecb<192>;   // 24-byte key, 12 rounds
using aes_ecb_256 = aes_ecb<256>;   // 32-byte key, 14 rounds

using aes_cbc_128 = aes_cbc<128>;
using aes_cbc_192 = aes_cbc<192>;
using aes_cbc_256 = aes_cbc<256>;
```

### Compile-time constants

```cpp
static constexpr size_t key_size;    // 16, 24, or 32
static constexpr size_t block_size;  // always 16
static constexpr size_t round_count; // 10, 12, or 14
```

### Static API

```cpp
static std::bytearray encrypt(const std::bytearray& data, const std::bytearray& key);
static std::bytearray decrypt(const std::bytearray& data, const std::bytearray& key);
```

For ECB: `key` must be exactly `key_size` bytes.
For CBC: `key` must be `key_size + 16` bytes (key + IV concatenated).

### Instance API

```cpp
explicit aes_ecb_128(const std::bytearray& key);
std::bytearray encrypt(const std::bytearray& data) const;
std::bytearray decrypt(const std::bytearray& data) const;
```

Construct the cipher with a key, then call `encrypt`/`decrypt` without passing the key again.

## Encryption API Concepts

The AES classes satisfy the `encryption_api` concepts:

```cpp
static_assert(scl2::has_encryption_support<scl2::aes_ecb_128>);
static_assert(scl2::has_decryption_support<scl2::aes_ecb_128>);
static_assert(scl2::has_fixed_key_size<scl2::aes_ecb_128>);
// scl2::generic_key_size<aes_ecb_128>() == 16
```

## Modes

### ECB (Electronic Codebook)

Each 16-byte block is encrypted independently. Deterministic — identical plaintext blocks produce identical ciphertext. Suitable for random-access decryption but not recommended for messages larger than one block without additional authentication.

### CBC (Cipher Block Chaining)

Each plaintext block is XORed with the previous ciphertext block before encryption. The first block uses the IV. Non-deterministic (different IV → different ciphertext). Requires a random/unpredictable IV for security.

## Padding

PKCS7 padding is applied automatically. If the plaintext length is a multiple of 16, a full padding block (16 bytes of `0x10`) is added. Padding is verified and removed on decryption.

## Key Sizes

| Variant | Key Size | Rounds | Round Key Size |
|---------|----------|--------|---------------|
| AES-128 | 16 bytes | 10 | 176 bytes |
| AES-192 | 24 bytes | 12 | 208 bytes |
| AES-256 | 32 bytes | 14 | 240 bytes |

## Related Modules

- [`crypto::xor_op`](xor.md) — Simple XOR encryption for obfuscation
- `encryption_api.hpp` — Concept definitions for encryption providers
