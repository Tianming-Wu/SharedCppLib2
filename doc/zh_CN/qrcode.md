# qrcode - 二维码（QR Code）编码器

+ 名称: QRCode
+ 命名空间: `scl2::qrcode`
+ 文档版本: `3.3.0`

## CMake 信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库 | `qrcode`（依赖 `bitmap`） |

引入方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::qrcode)
```

## 描述

QR 码编码器（`scl2::qrcode`），输出 1 位矩阵（`bitmap_1c`）。
实现了完整的 v1–v40 编码流程：

```
data -> encode_data() -> Reed-Solomon ECC -> build_codewords()
     -> place_function_patterns() -> place_data()
     -> apply_mask() -> place_format_info() / place_version_info()
```

纠错使用 GF(2^8) 上的 Reed–Solomon 算法（`scl2::xmath::reedsolomon`）。

> [!NOTE]
> **尚未实现解码**——`scl2::qrcode::decoder::decode()` 目前是桩函数。

## 快速开始

```cpp
#include <SharedCppLib2/qrcode.hpp>
#include <SharedCppLib2/fileio.hpp>

// 编码并生成包含默认 4 模块静区的位图
scl2::bitmap_1c qr = scl2::qrcode::encoder::generate("https://example.com");

// 生成不含静区的裸矩阵，放大后导出为 BMP
auto big = scl2::qrcode::encoder::make_matrix("hello", {}).scaled(4);
scl2::writeFile("qrcode.bmp", big.toBmp());
```

### 选项
```cpp
scl2::qrcode::encoder::options opt;
opt.ec_level   = scl2::qrcode::ErrorCorrectionLevel::H; // L / M / Q / H
opt.version    = scl2::qrcode::Version::vauto;          // 或强制指定 v1..v40
opt.mode       = scl2::qrcode::Mode::automatic;         // numeric / alphanumeric / byte / kanji
opt.mask       = -1;                                    // -1 = 自动选择最佳掩码
opt.quiet_zone = 4;                                     // 静区宽度（模块数）

auto qr = scl2::qrcode::encoder::generate("data", opt);
```

## API 参考

### encoder（静态方法）
| 方法 | 说明 |
|--------|-------------|
| `generate(data, opt)` | 编码并返回包含静区的图像 |
| `make_matrix(data, opt)` | 编码并返回不含静区的裸 N x N 矩阵 |
| `select_version(data, mode, ec_level)` | 能容纳数据的最小版本 |
| `encode_data(...)` | 将数据打包为数据码字（纯函数；公开用于测试） |
| `build_codewords(...)` | 分块、追加 ECC 并交织 |

### 类型
- `Mode` — `automatic` / `numeric` / `alphanumeric` / `byte` / `kanji`。
- `ErrorCorrectionLevel` — `L`（约 7%）、`M`（约 15%）、`Q`（约 25%）、`H`（约 30%）。
- `Version` — `vauto`、`v1` .. `v40`。
- `sizeForVersion(v)` — 矩阵尺寸（模块数）：`17 + 4 * version`。

## 相关文档

- [bitmap](bitmap.md) — 编码器生成的 `bitmap_1c` 类型，以及 BMP 读写。
