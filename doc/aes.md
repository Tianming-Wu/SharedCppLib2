# aes - AES Encryption Module

+ Name: aes
+ Namespace: `scl2::crypto` (inline)
+ Document Version: `1.1.0`

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

In CBC mode, the 16-byte IV is appended to the key as a single `key_type`:

```cpp
// AES-128-CBC: key(16) + iv(16) = 32 bytes
std::bytearray key(static_cast<size_t>(16), std::byte{0x2b});       // secret key
std::bytearray iv(static_cast<size_t>(16), std::byte{0x00});        // initial vector

// Concatenate into one key_type
std::bytearray keyiv = key;
keyiv.append(iv);

auto ct = scl2::aes_cbc_128::encrypt(pt, keyiv);
auto dec = scl2::aes_cbc_128::decrypt(ct, keyiv);
```

> [!IMPORTANT]
> The **IV must be random and unique** for each encryption with the same key.
> Never reuse an IV — it breaks CBC security.
> A simple way: generate 16 random bytes and prepend them to the ciphertext,
> then extract the IV on decryption:
>
> ```cpp
> // Encryption
> std::bytearray iv = /* 16 random bytes */;
> std::bytearray keyiv = key;
> keyiv.append(iv);
> auto ct = aes_cbc_128::encrypt(data, keyiv);
> // Transmit or store: iv + ct
>
> // Decryption
> std::bytearray iv2 = received.subarr(0, 16);        // extract IV
> std::bytearray ct2 = received.subarr(16);            // extract ciphertext
> std::bytearray keyiv2 = key;
> keyiv2.append(iv2);
> auto pt = aes_cbc_128::decrypt(ct2, keyiv2);
> ```

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

## Key Verification

AES itself cannot verify whether the decryption key matches the encryption key — decrypting with the wrong key simply produces garbage data.

However, this implementation uses **PKCS7 padding**, which provides a weak form of verification: if the decryption key is incorrect, the padding bytes will almost certainly be invalid, and `decrypt()` will throw `std::invalid_argument`. A correct key will always produce valid padding.

For stronger verification, use [HMAC](../doc/hmac.md) on the ciphertext:

```cpp
auto ct = scl2::aes_ecb_128::encrypt(data, key);
auto tag = scl2::hmac<scl2::sha256>::compute(ct, auth_key);
// store both ct and tag

// On decryption:
// recompute tag and compare before decrypting
```

## Streaming (Large Data)

For files or large data that cannot fit in memory, use `stream_type`:

```cpp
// Encrypt
scl2::aes_ecb_128::stream_type enc(key, scl2::cipher_dir::Encrypt);
auto block1 = enc.update(chunk1);
auto block2 = enc.update(chunk2);
auto last   = enc.end();  // PKCS7 padded final block

// Decrypt
scl2::aes_ecb_128::stream_type dec(key, scl2::cipher_dir::Decrypt);
auto pt1 = dec.update(ct1);
auto pt2 = dec.update(ct2);
auto pt3 = dec.end();     // unpadded final block
```

### Stream from istream

```cpp
#include <SharedCppLib2/encryption_api.hpp>  // for encrypt_stream

std::ifstream in("plain.bin", std::ios::binary);
std::ofstream out("cipher.bin", std::ios::binary);
scl2::encrypt_stream<scl2::aes_ecb_128>(in, out, key);
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
