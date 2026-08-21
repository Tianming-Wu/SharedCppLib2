# bitmap - 像素位图与绘制库

+ 名称: Bitmap
+ 命名空间: `scl2`
+ 文档版本: `3.3.0`

## CMake 信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库 | `bitmap`（依赖 `basic`） |

引入方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::bitmap)
```

## 描述

像素模板位图容器，附带一套轻量的光栅化工具：

- `bitmap<Pixel>` — 通用位图，支持任意像素类型（`scl2::color`、`scl2::rgba8` 等）。
- `bitmap<bool>`（别名 `bitmap_1c`）— 1 位打包单色位图，支持 BMP 读写和可配置的行对齐（字节 / 32 位行），适用于 MCU / 帧缓冲场景。
- `drawer<Pixel>` — 光栅化原语（线 / 矩形 / 圆）、抗锯齿，以及 `pen` / `brush` 样式（位于 `drawer.hpp`）。
- `bitmap_pattern<W,H>` — 便于硬编码固定图案（如二维码定位符、字形等）的 constexpr 友好单色栅格。

抽象接口 `draw_target<Pixel>` 让 `drawer` 可以在不了解具体像素存储的情况下渲染到任意表面。

文件支持：BMP（1 位）。尚不支持 PNG / JPG / GIF。

## 快速开始

### 单色位图（bitmap_1c）
```cpp
#include <SharedCppLib2/bitmap.hpp>

// 8x8 单色位图（bitmap_1c == bitmap<bool>）
scl2::bitmap_1c bm(8, 8);
bm.setPixel(2, 2, true);
bool v = bm.getPixel(2, 2); // true

// 导出为 1 位 BMP 并写入文件
scl2::writeFile("out.bmp", bm.toBmp());

// 从原始字节读回 BMP（fileio::readFile 返回 bytearray）
auto loaded = scl2::bitmap_1c::fromBmp(scl2::readFile("out.bmp"));
```

### 通用 / 彩色位图
```cpp
#include <SharedCppLib2/color.hpp>

scl2::bitmap<scl2::rgba8> rgb(16, 16);            // 紧凑的 4 字节 RGBA 像素
rgb.setPixel(0, 0, scl2::rgba8(255, 0, 0));       // 红色，不透明

scl2::bitmap<scl2::color> c(4, 4, scl2::colors::white);
c.set_pixel(1, 1, scl2::color(0, 128, 255));
```

### 绘制（drawer.hpp）
```cpp
#include <SharedCppLib2/drawer.hpp>

scl2::bitmap_1c bm(64, 64);
scl2::drawer<bool> d(bm, true, false);   // 笔 = 深色，背景 = 浅色

d.draw_line(0, 0, 63, 63);
d.draw_rectangle(8, 8, 40, 40, scl2::fill_mode::none);
d.draw_circle(32.0, 32.0, 20.0, scl2::fill_mode::none);

// 在灰度 / rgba8 位图上抗锯齿绘制
scl2::bitmap<scl2::rgba8> aa(64, 64);
scl2::drawer<scl2::rgba8> dr(aa, scl2::rgba8(255, 255, 255), scl2::rgba8(0, 0, 0));
dr.draw_line_aa(0, 0, 63, 20);                       // Wu 抗锯齿直线
dr.blend_at(10, 10, scl2::rgba8(255, 0, 0, 128), 255); // source-over 混合
```

### 缩放 / 适配
```cpp
auto big  = bm.scaled(4);                        // 最近邻 4 倍放大
auto sml  = bm.scaled_down(2);                   // 2 倍缩小
auto fit  = bm.fit_into(100, 80, scl2::Stretch::Contain); // 信箱式适配
auto tile = bm.fit_into(100, 80, scl2::Stretch::Tile);    // 平铺
```

## API 参考

### bitmap<Pixel>（通用）
| 方法 | 说明 |
|--------|-------------|
| `bitmap(w, h, init)` | 用填充值构造 |
| `set_pixel(x, y, v)` / `get_pixel` | 访问像素（越界抛出异常） |
| `width()` / `height()` / `getSize()` | 尺寸 |
| `resize(w, h)` | 丢弃内容并调整大小 |
| `resize(w, h, align)` | 保留内容并按对齐方式调整大小 |
| `scaled(f)` / `scaled_down(f)` | 最近邻整数缩放 |
| `fit_into(w, h, stretch, align)` | Fill / Cover / Contain / Center / Tile |
| `clear()` | 将所有像素重置为默认值 |

### bitmap<bool> / bitmap_1c
| 方法 | 说明 |
|--------|-------------|
| `setPixel` / `getPixel` | 1 位像素访问（true = 深色） |
| `toBmp()` | 序列化为 1 位 BMP（`bytearray`） |
| `fromBmp(bytes)` | 解析 1 位 BMP（`static`） |
| `toByteArrayPadded()` | 行按 4 字节边界补齐（BMP 布局） |
| `row_align()` / `set_row_align()` | 行对齐（1 = 字节，4 = 32 位） |
| `reverse_color()` | 反转每个像素 |
| `pixelAnd` / `pixelOr` / `pixelXor` / `pixelOverride` | 与另一张位图做按位运算 |
| `extend` / `shrink` | 仅向更大 / 更小方向调整大小 |

### drawer<Pixel>（drawer.hpp）
| 方法 | 说明 |
|--------|-------------|
| `draw_pixel` / `draw_line` / `draw_rectangle` / `draw_circle` | 硬边原语 |
| `draw_line_aa` / `draw_circle_aa` | 抗锯齿（1 位回退到硬边） |
| `blend_at(x, y, src, alpha)` | 按不透明度写入（`rgba8` 为 source-over） |
| `pen` / `brush` | 描边 / 填充设置（颜色、宽度、不透明度） |

### color / rgba8（color.hpp）
- `scl2::color::blend(other, t)` / `operator+` — RGBA 空间插值。
- `scl2::rgba8` — 紧凑的 4 字节位图像素；可与 `color` 互相转换。

### bitmap_pattern<W,H>
带 `set(x, y, v)` / `get(x, y)` 和 `data()` 的 constexpr 单色图案，
可通过 `to_bitmap()` 转换为运行时 `bitmap_1c`。
