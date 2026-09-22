# compression - Compression Providers and Algorithm Identifiers

+ Name: compression
+ Namespace: `scl2`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `compression` |
| Dependencies | `basic` |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::compression)
```

## Description

Choosing a compression algorithm while the program runs.

`compression_api.hpp` describes what a provider looks like as a type, and the library matches
providers by type. That works until the choice is a runtime one - a file says which algorithm
it was written with, or an application configures one. This header adds the two things such a
choice needs: a holder that erases the type, and an identifier that a file can record.

## Quick Start

```cpp
#include <SharedCppLib2/compression.hpp>
#include <SharedCppLib2/zlib.hpp>

scl2::compression_provider provider = scl2::compression_provider::from<scl2::zlib>();

scl2::bytearray packed = provider.compress(data);
scl2::bytearray back   = provider.decompress(packed);
```

An algorithm of your own, without wrapping it in a type:

```cpp
scl2::compression_provider mine = scl2::compression_provider::from(
    [](const scl2::bytearray& in) { return compressWith(in); },
    [](const scl2::bytearray& in) { return decompressWith(in); });
```

## Identifiers

```cpp
enum class compress_algo : uint8_t { none = 0, zlib = 1 };

inline constexpr uint8_t user_compression_base = 128;
```

Values below `user_compression_base` belong to the library and keep their meaning. From
`user_compression_base` up, the range belongs to applications, who pick their own numbers.
The gap leaves room for the library to add an algorithm later without colliding with one an
application already chose.

`compression_id` is how such a value is passed around. It is built from a `compress_algo`
without ceremony, and from a plain number explicitly:

```cpp
scl2::compression_id builtin = scl2::compress_algo::zlib;
scl2::compression_id mine{uint8_t{200}};
scl2::compression_id nothing;        // 0, the absence of an algorithm
```

## API

### compression_provider

| Function | Description |
|---------|---------|
| `static from<T>()` | Wraps a provider as `compression_api.hpp` describes them |
| `static from(compress, decompress)` | Wraps a pair given directly; either half may be empty |
| `hasCompression()` / `hasDecompression()` | Which halves are there |
| `isUsable()` | Both are |
| `compress(data)` / `decompress(data)` | Run one half. Throws `std::runtime_error` when that half is missing |
| `verify()` / `verify(probe)` | Runs a probe through both halves and reports whether it came back unchanged |
| `static defaultProbe()` | The 256-byte probe `verify()` uses when given none |

### compression_id

| Function | Description |
|---------|---------|
| `compression_id(compress_algo)` | Names a built-in |
| `explicit compression_id(uint8_t)` | Names one an application chose |
| `value()` | The number |
| `isNone()` | It is 0 |
| `isBuiltin()` / `isUser()` | Which range it is in |
| `operator==` | Two identifiers are equal when they name the same number |

## Notes

> [!IMPORTANT]
> **An identifier does not describe an algorithm.** Two programs read each other's files only
> if they agree on what each number means, and the library cannot check that. It is the same
> kind of agreement as a protocol version: it belongs to whoever writes the format.

> [!NOTE]
> **`verify()` is a check, not a proof.** It says the two halves agree on the probe it was
> given, so a pair that disagrees on other input passes, and a decompressor that expects a
> different algorithm passes too. It also never runs by itself - nothing in the library calls
> it on the save or open path, on purpose: a release build trusts what it is handed. Call it
> from your own tests, and with a probe that looks like your data.

> [!WARNING]
> **A pair that does not match costs data.** Compressing with one function and decompressing
> with another that is not its inverse produces a file that cannot be read back: the length
> and structure will not line up, so opening it fails rather than returning wrong values. If
> that save was the only copy, the data is gone. Check a pair before trusting anything
> important to it, and remember that "works on small input" says nothing about large input.

## See Also

- [bytearray](bytearray.md) — what goes in and comes out
- [xkeydb](xkeydb.md) — the module that uses this to record an algorithm in its files
