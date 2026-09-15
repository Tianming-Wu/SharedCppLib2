# png - PNG 图像编解码器

+ 名称: PNG
+ 命名空间: `scl2`
+ 文档版本: `3.5.0`

## CMake 信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库 | `png` |
| 依赖 | `basic`、`bitmap`、`zlib`、`crc32` |

引入方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::png)
```

## 描述

把 PNG（ISO/IEC 15948，即 RFC 2083）图像读写为 RGBA8 位图。

`scl2::png` 继承自 `bitmap<rgba8>`，所以一张解码出来的图**就是**一张位图：可以直接用 `drawer` 在上面绘制、可以传给任何收 `bitmap<rgba8>&` 的函数、也可以用 `to_bitmap()` 把像素单独取出来。除此之外它还携带 PNG 专有的信息：源文件的 IHDR、调色板、以及解码时保留下来的辅助块。

解码覆盖 PNG 规范要求解码器处理的全部内容。编码恒输出 8 位图像，并在像素仍然允许时沿用源文件的调色板或灰度形态。

## 快速开始

```cpp
#include <SharedCppLib2/png.hpp>

scl2::bytearray raw = scl2::readFile("in.png");

scl2::png img = scl2::png::decode(raw);          // 或 png::load(raw)
std::cout << img.width() << "x" << img.height() << "\n";

// 它本身就是位图，整套 bitmap API 都能用
for (scl2::rgba8& px : img.row(0)) px.a = 128;

scl2::writeFile("out.png", img.encode());        // 或 img.dump()
```

无副作用的格式嗅探：

```cpp
if (scl2::png::matches(raw)) { /* raw 当前游标处是 PNG 签名 */ }
```

`matches()` 从当前读取游标位置开始判断，判断完把游标放回原处；数据不是 PNG 时返回 `false`。

## 接口

### 类型

| 名称 | 含义 |
|---------|---------|
| `png::color_type` | IHDR 颜色类型：`grayscale`(0) / `rgb`(2) / `palette`(3) / `grayscale_alpha`(4) / `rgba`(6) |
| `png::info` | 源文件的 IHDR：`width`、`height`、`bit_depth`、`color`、`interlaced` |
| `png::mode` | 请求的输出形态：`auto_preserve` / `rgb` / `rgba` / `grayscale` / `palette` |
| `png::options` | `format`（`mode`）、`filter`（-1 = 逐行启发式，0..4 = 强制）、`interlace`（写 Adam7） |
| `png::chunk` | 保留下来的辅助块：`type` + `data` |

> [!NOTE]
> `info::bit_depth` 是**每个通道**的位数，不是每个像素的位数 —— 这是 PNG 规范的定义。
> 常说的“32 位 PNG”是颜色类型 6 加位深 8，“24 位”是颜色类型 2 加位深 8：
>
> | 颜色类型 | 每像素位数（位深 8） | 位深 16 |
> |---------|---------|---------|
> | 0 灰度 | 8 | 16 |
> | 2 RGB | 24 | 48 |
> | 3 调色板 | 8（索引） | — |
> | 4 灰度+alpha | 16 | 32 |
> | 6 RGBA | 32 | 64 |
>
> 像素容器是 RGBA8 —— 每通道 8 位、每像素 32 位 —— 所以位深 8 及以下的图像原样装得下。
> 16 位样本取高字节降到 8 位。

### 成员

| 函数 | 说明 |
|---------|---------|
| `static bool matches(const bytearray&)` | 当前游标处是否为 PNG 签名？不改变游标位置 |
| `static png decode(const bytearray&)` | 解码完整的 PNG 流（失败抛 `std::runtime_error`） |
| `static png load(const bytearray&)` | `decode()` 的别名，沿用库里的 `dump()`/`load()` 命名 |
| `bytearray encode(const options& = {}) const` | 编码为 PNG 流 |
| `bytearray dump() const` | `encode()` 的别名 |
| `const info& source_info() const` | 本图像来源文件的 IHDR |
| `const std::vector<rgba8>& palette() const` | 源调色板；非调色板图为空 |
| `const std::vector<chunk>& ancillary() const` | 解码时保留下来的辅助块 |
| `const bitmap<rgba8>& as_bitmap() const` | 把像素看作一张普通位图 |
| `bitmap<rgba8> to_bitmap() const` | 只拷出像素，丢掉全部 PNG 专有字段 |
| `row(y)` / `data()` | 继承自 `bitmap<rgba8>`，见 [bitmap](bitmap.md) |

## 解码覆盖范围

一个合规解码器必须处理的东西全都支持：

| 特性 | 支持情况 |
|---------|---------|
| 位深 | 1、2、4、8、16 |
| 颜色类型 | 0 灰度、2 RGB、3 调色板、4 灰度+alpha、6 RGBA |
| 行滤波 | 0 None、1 Sub、2 Up、3 Average、4 Paeth |
| 交错 | 非交错与 Adam7（全部 7 个 pass） |
| 透明 | `tRNS` 调色板 alpha，以及灰度 / RGB 颜色键 |
| 辅助块 | 保留以供查看：`gAMA` `cHRM` `sRGB` `iCCP` `sBIT` `bKGD` `pHYs` `tEXt` `zTXt` `iTXt` `tIME` `eXIf` `hIST` `sPLT` |
| 完整性 | 逐块校验 CRC；未知的**关键**块直接拒绝 |

## 编码行为

输出恒为 8 位（容器是 RGBA8）。默认的 `mode::auto_preserve` 会尽量保持源文件的形态：

1. 源是调色板图，且每个像素仍能映射到该调色板的某一项（含 `tRNS` alpha）→ 写调色板 PNG，沿用原位深；
2. 否则源是灰度图、且每个像素仍满足 `R == G == B` → 写灰度（需要时带 alpha）；
3. 否则全不透明写 RGB，有透明写 RGBA。

`mode::rgb` / `rgba` / `grayscale` / `palette` 可以强制指定形态。`mode::palette` 会从图像本身构建调色板，超过 256 色则报错。

除非 `options::filter` 强制，否则每行用"最小绝对差之和"启发式选择滤波器。`options::interlace` 写 Adam7 交错。

## 注意事项

> [!IMPORTANT]
> **编码结果不保证逐字节可复现。** `zlib` 模块用的是固定哈夫曼编码器，所以即使每个像素都一致，重编码后的 `IDAT` 也和原文件不同。像素级往返是可靠的，字节级往返按设计就不追求。

> [!NOTE]
> **辅助块不会被写回。** `ancillary()` 把它们暴露出来供查看，但 `encode()` 只写一个全新的、最小的 PNG。这是有意的：调色板变化之后再写回 `bKGD`、`hIST` 之类的块，会产出一个"看起来对、其实错"的文件。

> [!NOTE]
> **完全透明的像素保留颜色通道。** 被 `tRNS` 颜色键命中的像素，样本值原样保留，只把 alpha 置 0。有些解码器（比如 GDI+）会把颜色通道清零。渲染出来两者没有区别。

> [!WARNING]
> **要求存在 Adler-32 尾校验。** `IDAT` 的 zlib 流必须带 RFC 1950 校验和，文件也应当以 `IEND` 结束。Ghostscript 24.1 的 `fpng` 设备恰好两者都没写，这类文件会被拒绝。其余测试过的 PNG 生成器（包括 Ghostscript 的其它设备）都会正常写出。

## 相关模块

- [bitmap](bitmap.md) —— 像素容器与绘制工具，用来处理解码结果
- [qrcode](qrcode.md) —— 另一个产出位图的模块；生成的二维码可以用 `encode()` 写出
