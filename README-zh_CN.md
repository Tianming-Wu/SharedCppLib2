# SHARED CPP 库 2 (SharedCppLib2)

[English](README.md)

**作者**: Tianming ([GitHub](https://github.com/Tianming-Wu))

**项目页面**: [GitHub 仓库](https://github.com/Tianming-Wu/SharedCppLib2)

> [!NOTE]
> 这个库是我许多项目的依赖项。您需要先安装此库才能使用那些项目，或者利用[自动依赖获取](doc/zh_CN/cmake/_autofetch.md)功能。

**免费软件，不提供任何担保。** 部分模块可能不完整。

您可以将此库用于个人或商业用途。欢迎注明出处但不是必须的。

## 概述

一个为性能和易用性设计的现代 C++ 实用程序库集合。基于 C++23 标准构建。

## 核心模块

- **`ansiio`** - ANSI 终端输出和格式化
- **[`stringlist`](doc/zh_CN/stringlist.md)** - 增强型字符串操作，提供 Qt 风格的 API
- **`Base64`** - Base64 编码和解码工具
- **[`bytearray`](doc/zh_CN/bytearray.md)** - 二进制数据管理和序列化
- **[`sha256`](doc/zh_CN/sha256.md)** - SHA-256 加密哈希实现
- **[`logt`](doc/zh_CN/logt.md)** - 高性能异步日志记录
- **[`logc`](doc/zh_CN/logc.md)** - 为 logt 提供彩色控制台输出
- **`indexer`** - 数据索引和搜索工具
- **`hpcalc`** - 高精度算术运算
- **[`regexfilter`](doc/zh_CN/regexfilter.md)** - 基于正则表达式的黑名单/白名单过滤
- **`atxsort`** - 通用排序算法

## 遗留模块（不推荐使用）

- `logd` - 简单控制台日志记录器（已弃用）
- `logf` - 简单文件日志记录器（已弃用）

## 已弃用模块

- `eventloop` - 已移至独立项目
- `tasker` - 子线程任务管理器（不再维护）

## 安装

### 1. 克隆仓库
```bash
git clone https://github.com/Tianming-Wu/SharedCppLib2
cd SharedCppLib2
```

### 2. 构建和安装

**调试版本：**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --install build
```

**发布版本：**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

> [!NOTE]
> 在 Windows 上，请在管理员提示符下运行，或在 Windows 11 上使用 `sudo`。`--config` 参数仅适用于多配置生成器（如 Visual Studio）。

### 3. 在您的项目中使用

**CMakeLists.txt：**
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(您的目标 SharedCppLib2::basic)
```

**可用目标：** `variant`, `logd`, `logf`, `sha256`, `basic`, `indexer`, `regexfilter`, `logt`, `logc`, `Base64`, `platform`

### 4. 代码示例
```cpp
#include <SharedCppLib2/stringlist.hpp>

int main(int argc, char** argv) {
    std::stringlist args(argc, argv);
    // 处理命令行参数
    return 0;
}
```

详细的文档可在头文件的 Doxygen 注释中找到。

**完整更新日志**: [WhatsNew](WhatsNew)