# qrcode - QR Code Encoder

+ Name: QRCode
+ Namespace: `scl2::qrcode`
+ Document Version: `3.3.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `qrcode` (depends on `bitmap`) |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::qrcode)
```

## Description

A QR Code encoder (`scl2::qrcode`) that produces 1-bit matrices (`bitmap_1c`).
It implements the full v1–v40 encoding pipeline:

```
data -> encode_data() -> Reed-Solomon ECC -> build_codewords()
     -> place_function_patterns() -> place_data()
     -> apply_mask() -> place_format_info() / place_version_info()
```

Error correction uses Reed–Solomon over GF(2^8) (`scl2::xmath::reedsolomon`).

> [!NOTE]
> Decoding is **not implemented yet** — `scl2::qrcode::decoder::decode()` is a stub.

## Quick Start

```cpp
#include <SharedCppLib2/qrcode.hpp>
#include <SharedCppLib2/fileio.hpp>

// Encode and produce a bitmap including the default 4-module quiet zone
scl2::bitmap_1c qr = scl2::qrcode::encoder::generate("https://example.com");

// Bare matrix without quiet zone, scaled up, exported as BMP
auto big = scl2::qrcode::encoder::make_matrix("hello", {}).scaled(4);
scl2::writeFile("qrcode.bmp", big.toBmp());
```

### Options
```cpp
scl2::qrcode::encoder::options opt;
opt.ec_level   = scl2::qrcode::ErrorCorrectionLevel::H; // L / M / Q / H
opt.version    = scl2::qrcode::Version::vauto;          // or force v1..v40
opt.mode       = scl2::qrcode::Mode::automatic;         // numeric / alphanumeric / byte / kanji
opt.mask       = -1;                                    // -1 = auto-select best
opt.quiet_zone = 4;                                     // white border in modules

auto qr = scl2::qrcode::encoder::generate("data", opt);
```

## API Reference

### encoder (static methods)
| Method | Description |
|--------|-------------|
| `generate(data, opt)` | Encode and return the image including quiet zone |
| `make_matrix(data, opt)` | Encode and return the bare N x N matrix (no quiet zone) |
| `select_version(data, mode, ec_level)` | Smallest version that fits the data |
| `encode_data(...)` | Pack data into data codewords (pure; public for testing) |
| `build_codewords(...)` | Split into blocks, append ECC, interleave |

### Types
- `Mode` — `automatic` / `numeric` / `alphanumeric` / `byte` / `kanji`.
- `ErrorCorrectionLevel` — `L` (~7%), `M` (~15%), `Q` (~25%), `H` (~30%).
- `Version` — `vauto`, `v1` .. `v40`.
- `sizeForVersion(v)` — matrix size in modules: `17 + 4 * version`.

## Related

- [bitmap](bitmap.md) — the `bitmap_1c` type produced by the encoder, plus BMP I/O.
