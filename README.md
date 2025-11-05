# SHARED CPP Library 2 (SharedCppLib2)

[中文](README-zh_CN.md)

**Author**: Tianming ([GitHub](https://github.com/Tianming-Wu))

**Project Page**: [GitHub Repository](https://github.com/Tianming-Wu/SharedCppLib2)

> [!NOTE]
> This library is a dependency for many of my projects. You need to install it first to use those projects, or utilize [automatic dependency fetching](doc/cmake/autofetch.md).

**Free software with NO WARRANTY.** Some modules may be incomplete.

You may use this library for personal or commercial purposes. Attribution is appreciated but not required.

## Overview

A collection of modern C++ utility libraries designed for performance and ease of use. Built with C++23 standard.

## Core Modules

- **`ansiio`** - ANSI terminal output and formatting
- **[`stringlist`](doc/stringlist.md)** - Enhanced string manipulation with Qt-inspired API
- **`Base64`** - Base64 encoding and decoding utilities
- **[`bytearray`](doc/bytearray.md)** - Binary data management and serialization
- **[`sha256`](doc/sha256.md)** - SHA-256 cryptographic hash implementation
- **[`logt`](doc/logt.md)** - High-performance asynchronous logging
- **[`logc`](doc/logc.md)** - Colored console output for logt
- **`indexer`** - Data indexing and search utilities
- **`hpcalc`** - High-precision arithmetic operations
- **[`regexfilter`](doc/regexfilter.md)** - Regex-based blacklist/whitelist filtering
- **`atxsort`** - Universal sorting algorithms

## Legacy Modules (Not Recommended)

- `logd` - Simple console logger (deprecated)
- `logf` - Simple file logger (deprecated)

## Deprecated Modules

- `eventloop` - Moved to separate project
- `tasker` - Sub-thread task manager (unmaintained)

## Installation

### 1. Clone the Repository
```bash
git clone https://github.com/Tianming-Wu/SharedCppLib2
cd SharedCppLib2
```

### 2. Build and Install

**For Debug:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --install build
```

**For Release:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

> [!NOTE]
> On Windows, run in Administrator prompt or use `sudo` on Windows 11. The `--config` parameter is only needed for multi-configuration generators like Visual Studio.

### 3. Use in Your Project

**CMakeLists.txt:**
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(your_target SharedCppLib2::basic)
```

**Available targets:** `variant`, `logd`, `logf`, `sha256`, `basic`, `indexer`, `regexfilter`, `logt`, `logc`, `Base64`, `platform`

### 4. Code Example
```cpp
#include <SharedCppLib2/stringlist.hpp>

int main(int argc, char** argv) {
    std::stringlist args(argc, argv);
    // Process command line arguments
    return 0;
}
```

Detailed documentation is available in header files as Doxygen comments.

**Full Changelog**: [WhatsNew](WhatsNew)