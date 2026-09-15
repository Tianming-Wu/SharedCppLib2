# png - PNG Image Codec

+ Name: PNG
+ Namespace: `scl2`
+ Document Version: `3.5.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `png` |
| Dependencies | `basic`, `bitmap`, `zlib`, `crc32` |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::png)
```

## Description

Reads and writes PNG (ISO/IEC 15948, a.k.a. RFC 2083) images as RGBA8 bitmaps.

`scl2::png` derives from `bitmap<rgba8>`, so a decoded image *is* a bitmap: it can be
drawn on with `drawer`, passed to anything taking `bitmap<rgba8>&`, or have its pixels
taken out with `to_bitmap()`. On top of that it carries the PNG-specific information
collected while decoding — the source IHDR, the palette, and the ancillary chunks that
were kept.

Decoding covers everything the PNG specification expects a decoder to handle. Encoding
always writes 8-bit images, and keeps the source's palette or greyscale shape whenever
the pixels still allow it.

## Quick Start

```cpp
#include <SharedCppLib2/png.hpp>

scl2::bytearray raw = scl2::readFile("in.png");

scl2::png img = scl2::png::decode(raw);          // or png::load(raw)
std::cout << img.width() << "x" << img.height() << "\n";

// it is a bitmap, so the whole bitmap API is available
for (scl2::rgba8& px : img.row(0)) px.a = 128;

scl2::writeFile("out.png", img.encode());        // or img.dump()
```

Format sniffing without side effects:

```cpp
if (scl2::png::matches(raw)) { /* raw starts with a PNG signature */ }
```

`matches()` reads at the current cursor position and puts the cursor back where it was;
it returns `false` for data that is not a PNG.

## API

### Types

| Name | Meaning |
|---------|---------|
| `png::color_type` | IHDR colour type: `grayscale`(0) / `rgb`(2) / `palette`(3) / `grayscale_alpha`(4) / `rgba`(6) |
| `png::info` | IHDR of the source file: `width`, `height`, `bit_depth`, `color`, `interlaced` |
| `png::mode` | Requested output layout: `auto_preserve` / `rgb` / `rgba` / `grayscale` / `palette` |
| `png::options` | `format` (`mode`), `filter` (-1 = per-row heuristic, 0..4 = force), `interlace` (write Adam7) |
| `png::chunk` | An ancillary chunk kept from the source: `type` + `data` |

> [!NOTE]
> `info::bit_depth` is the number of bits **per channel**, not per pixel — that is how
> the PNG specification defines it. A "32-bit" PNG is colour type 6 at bit depth 8, and
> "24-bit" is colour type 2 at bit depth 8:
>
> | Colour type | Bits per pixel, depth 8 | depth 16 |
> |---------|---------|---------|
> | 0 greyscale | 8 | 16 |
> | 2 RGB | 24 | 48 |
> | 3 palette | 8 (index) | — |
> | 4 greyscale + alpha | 16 | 32 |
> | 6 RGBA | 32 | 64 |
>
> The pixel container is RGBA8 — 8 bits per channel, 32 bits per pixel — so every image
> up to bit depth 8 lands in it unchanged. 16-bit samples are reduced by taking the high byte.

### Members

| Function | Description |
|---------|---------|
| `static bool matches(const bytearray&)` | Is there a PNG signature at the current cursor? Does not move the cursor |
| `static png decode(const bytearray&)` | Decode a complete PNG stream (throws `std::runtime_error`) |
| `static png load(const bytearray&)` | Alias of `decode()`, following the library's `dump()`/`load()` naming |
| `bytearray encode(const options& = {}) const` | Encode into a PNG stream |
| `bytearray dump() const` | Alias of `encode()` |
| `const info& source_info() const` | IHDR of the file this image came from |
| `const std::vector<rgba8>& palette() const` | Source palette, empty for non-palette images |
| `const std::vector<chunk>& ancillary() const` | Ancillary chunks kept while decoding |
| `const bitmap<rgba8>& as_bitmap() const` | The pixels, seen as a plain bitmap |
| `bitmap<rgba8> to_bitmap() const` | Copy the pixels out, dropping every PNG-specific field |
| `row(y)` / `data()` | Inherited from `bitmap<rgba8>` — see [bitmap](bitmap.md) |

## Decoding coverage

Everything a conforming decoder must handle:

| Feature | Support |
|---------|---------|
| Bit depths | 1, 2, 4, 8, 16 |
| Colour types | 0 grey, 2 RGB, 3 palette, 4 grey+alpha, 6 RGBA |
| Scanline filters | 0 None, 1 Sub, 2 Up, 3 Average, 4 Paeth |
| Interlacing | none and Adam7 (all seven passes) |
| Transparency | `tRNS` palette alpha and grey / RGB colour keys |
| Ancillary chunks | kept for inspection: `gAMA` `cHRM` `sRGB` `iCCP` `sBIT` `bKGD` `pHYs` `tEXt` `zTXt` `iTXt` `tIME` `eXIf` `hIST` `sPLT` |
| Integrity | every chunk CRC is verified; unknown *critical* chunks are rejected |

## Encoding behaviour

Output is always 8-bit (the container is RGBA8). With the default
`mode::auto_preserve` the writer tries to keep the shape of the source:

1. if the source was a palette image and every pixel still maps to an entry of
   that palette (`tRNS` alpha included), write a palette PNG at the original
   bit depth;
2. otherwise, if the source was greyscale and every pixel still has `R == G == B`,
   write greyscale (with alpha if needed);
3. otherwise write RGB when every pixel is opaque, and RGBA when it is not.

`mode::rgb` / `rgba` / `grayscale` / `palette` force a specific layout.
`mode::palette` builds the palette from the image itself and fails above 256 colours.

Each scanline is filtered by the minimum-sum-of-absolute-differences heuristic
unless `options::filter` forces a filter type. `options::interlace` writes Adam7.

## Notes

> [!IMPORTANT]
> **Encoding is not byte-for-byte reproducible.** The `zlib` module uses a
> fixed-Huffman encoder, so the `IDAT` of a re-encoded file differs from the
> original even when every pixel matches. Pixel-level round-trips are reliable;
> byte-level ones are not, by design.

> [!NOTE]
> **Ancillary chunks are not written back.** `ancillary()` exposes them for
> inspection, but `encode()` emits a fresh, minimal PNG. This is deliberate:
> re-emitting something like `bKGD` or `hIST` after the palette changed would
> produce a file that is subtly wrong.

> [!NOTE]
> **Fully transparent pixels keep their colour channels.** For a pixel matched by
> a `tRNS` colour key the stored sample values are preserved and only alpha is set
> to 0. Some decoders (GDI+, for one) zero the colour channels instead. The two
> are indistinguishable once rendered.

> [!WARNING]
> **The Adler-32 trailer is required.** The `IDAT` zlib stream must carry its
> RFC 1950 checksum, and the file should end with `IEND`. Ghostscript 24.1's
> `fpng` device happens to write neither; such files are rejected. Every other
> PNG producer tested (including the other Ghostscript devices) writes both.

## See Also

- [bitmap](bitmap.md) — the pixel container and the drawing toolkit, for working on a decoded image
- [qrcode](qrcode.md) — the other module that produces bitmaps; a generated code can be written out with `encode()`
