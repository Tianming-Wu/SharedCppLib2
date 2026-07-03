# crc32 - CRC32 Hash Library

+ Name: crc32  
+ Namespace: `scl2`  
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `crc32` |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::crc32)
```

```cpp
#include <SharedCppLib2/crc32.hpp>
```

## Description

CRC-32 (Cyclic Redundancy Check) is a widely used checksum algorithm for error detection. It produces a 32-bit (4-byte) value using the standard Ethernet / PKZip / zlib polynomial (`0xEDB88320`, reflected). Common applications include file integrity verification, network packet checksums, and data deduplication.

**CRC32 is not a cryptographic hash** — it is designed for error detection only, not for security-critical use.

## Quick Start

### One-shot

```cpp
std::bytearray data = std::bytearray("Hello, World!");
std::bytearray digest = scl2::crc32::hash(data);
// digest is 4 bytes, big-endian
```

### Streaming (large data)

```cpp
scl2::crc32::stream_type hasher;
hasher.update(chunk1);
hasher.update(chunk2);
// ... feed any number of chunks
std::bytearray digest = hasher.end();
```

### Stream from istream

```cpp
#include <SharedCppLib2/hash_api.hpp>

std::ifstream file("large.bin", std::ios::binary);
auto digest = scl2::hash_stream<scl2::crc32>(file);
```

### File Integrity Check

```cpp
std::ifstream file("data.bin", std::ios::binary);
auto expected = std::bytearray::fromHex("cbf43926");
auto actual = scl2::hash_stream<scl2::crc32>(file);
bool ok = (expected == actual);
```

## Test Vectors

| Input | CRC32 (hex) |
|-------|-------------|
| `""` (empty) | `00000000` |
| `"abc"` | `352441C2` |
| `"123456789"` | `CBF43926` (standard check value) |
