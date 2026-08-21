# bitmap - Pixel Bitmap and Drawing Library

+ Name: Bitmap
+ Namespace: `scl2`
+ Document Version: `3.3.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `bitmap` (depends on `basic`) |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::bitmap)
```

## Description

A pixel-templated bitmap container plus a small rasterization toolkit:

- `bitmap<Pixel>` — generic bitmap for any pixel type (`scl2::color`, `scl2::rgba8`, ...).
- `bitmap<bool>` (alias `bitmap_1c`) — 1-bit packed monochrome with BMP I/O and configurable row alignment (byte / 32-bit rows) for MCU / framebuffer use.
- `drawer<Pixel>` — rasterization primitives (line / rectangle / circle), anti-aliasing, and `pen` / `brush` styling (in `drawer.hpp`).
- `bitmap_pattern<W,H>` — constexpr-friendly monochrome raster for hard-coding fixed patterns (QR markers, glyphs, ...).

The abstract `draw_target<Pixel>` interface lets `drawer` render onto any surface without knowing the concrete pixel storage.

File support: BMP (1-bit). PNG / JPG / GIF are not supported yet.

## Quick Start

### Monochrome bitmap (bitmap_1c)
```cpp
#include <SharedCppLib2/bitmap.hpp>

// 8x8 monochrome bitmap (bitmap_1c == bitmap<bool>)
scl2::bitmap_1c bm(8, 8);
bm.setPixel(2, 2, true);
bool v = bm.getPixel(2, 2); // true

// Export as a 1-bit BMP and write to file
scl2::writeFile("out.bmp", bm.toBmp());

// Read a BMP back from raw bytes (fileio::readFile returns a bytearray)
auto loaded = scl2::bitmap_1c::fromBmp(scl2::readFile("out.bmp"));
```

### Generic / color bitmaps
```cpp
#include <SharedCppLib2/color.hpp>

scl2::bitmap<scl2::rgba8> rgb(16, 16);            // compact 4-byte RGBA pixels
rgb.setPixel(0, 0, scl2::rgba8(255, 0, 0));       // red, opaque

scl2::bitmap<scl2::color> c(4, 4, scl2::colors::white);
c.set_pixel(1, 1, scl2::color(0, 128, 255));
```

### Drawing (drawer.hpp)
```cpp
#include <SharedCppLib2/drawer.hpp>

scl2::bitmap_1c bm(64, 64);
scl2::drawer<bool> d(bm, true, false);   // pen = dark, off = light

d.draw_line(0, 0, 63, 63);
d.draw_rectangle(8, 8, 40, 40, scl2::fill_mode::none);
d.draw_circle(32.0, 32.0, 20.0, scl2::fill_mode::none);

// Anti-aliased drawing on grayscale / rgba8 bitmaps
scl2::bitmap<scl2::rgba8> aa(64, 64);
scl2::drawer<scl2::rgba8> dr(aa, scl2::rgba8(255, 255, 255), scl2::rgba8(0, 0, 0));
dr.draw_line_aa(0, 0, 63, 20);                       // Wu anti-aliased line
dr.blend_at(10, 10, scl2::rgba8(255, 0, 0, 128), 255); // source-over blend
```

### Scaling / fitting
```cpp
auto big  = bm.scaled(4);                        // nearest-neighbor 4x upscale
auto sml  = bm.scaled_down(2);                   // 2x downscale
auto fit  = bm.fit_into(100, 80, scl2::Stretch::Contain); // letterbox
auto tile = bm.fit_into(100, 80, scl2::Stretch::Tile);    // repeat
```

## API Reference

### bitmap<Pixel> (generic)
| Method | Description |
|--------|-------------|
| `bitmap(w, h, init)` | Construct with a fill value |
| `set_pixel(x, y, v)` / `get_pixel` | Access a pixel (throws on out-of-bounds) |
| `width()` / `height()` / `getSize()` | Dimensions |
| `resize(w, h)` | Discard content and resize |
| `resize(w, h, align)` | Resize preserving content with alignment |
| `scaled(f)` / `scaled_down(f)` | Nearest-neighbor integer scale |
| `fit_into(w, h, stretch, align)` | Fill / Cover / Contain / Center / Tile |
| `clear()` | Reset all pixels to default |

### bitmap<bool> / bitmap_1c
| Method | Description |
|--------|-------------|
| `setPixel` / `getPixel` | 1-bit pixel access (true = dark) |
| `toBmp()` | Serialize as a 1-bit BMP (`bytearray`) |
| `fromBmp(bytes)` | Parse a 1-bit BMP (`static`) |
| `toByteArrayPadded()` | Rows padded to 4-byte boundaries (BMP layout) |
| `row_align()` / `set_row_align()` | Row alignment (1 = byte, 4 = 32-bit) |
| `reverse_color()` | Invert every pixel |
| `pixelAnd` / `pixelOr` / `pixelXor` / `pixelOverride` | Bitwise ops with another bitmap |
| `extend` / `shrink` | Resize only toward larger / smaller |

### drawer<Pixel> (drawer.hpp)
| Method | Description |
|--------|-------------|
| `draw_pixel` / `draw_line` / `draw_rectangle` / `draw_circle` | Hard-edge primitives |
| `draw_line_aa` / `draw_circle_aa` | Anti-aliased (1-bit falls back to hard-edge) |
| `blend_at(x, y, src, alpha)` | Write with opacity (source-over for `rgba8`) |
| `pen` / `brush` | Stroke / fill settings (color, width, alpha) |

### color / rgba8 (color.hpp)
- `scl2::color::blend(other, t)` / `operator+` — RGBA-space interpolation.
- `scl2::rgba8` — compact 4-byte pixel for bitmaps; converts to / from `color`.

### bitmap_pattern<W,H>
`constexpr` monochrome pattern with `set(x, y, v)` / `get(x, y)` and `data()`,
convertible to a runtime `bitmap_1c` via `to_bitmap()`.
