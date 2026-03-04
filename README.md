# SHARED CPP Library 2 (SharedCppLib2)

Other languages: [中文](README-zh_CN.md)

**Author**: Tianming ([GitHub](https://github.com/Tianming-Wu))

**Project Page**: [GitHub Repository](https://github.com/Tianming-Wu/SharedCppLib2)

> [!NOTE]
> This library is a dependency for many of my projects. You need to install it first to use those projects, or utilize [automatic dependency fetching](doc/cmake/autofetch.md).

**Free software with NO WARRANTY.** Some modules may be incomplete.

You may use this library for personal or commercial purposes. Attribution is appreciated but not required.

## Overview

A collection of modern C++ utility libraries designed for performance and ease of use. Built with C++23 standard.

Some part of this project is basically re-inventing the wheel. The purpose is to learn something.

I still tried to make the API design as good (friendly, easy-to-use) as possible.

## Core Modules

- **`ansiio`** - ANSI terminal output and formatting
- **[`stringlist`](doc/stringlist.md)** - Enhanced string manipulation with Qt-inspired API
- **[`arguments`](doc/arguments.md)** - Type-safe command-line argument parsing (argparse-inspired)
- **`Base64`** - Base64 encoding and decoding utilities
- **[`bytearray`](doc/bytearray.md)** - Binary data management and serialization
- **[`sha256`](doc/sha256.md)** - SHA-256 cryptographic hash implementation
- **[`logt`](doc/logt.md)** - High-performance asynchronous logging
- **[`logc`](doc/logc.md)** - Colored console output for logt
- **`indexer`** - Data indexing and search utilities
- **`hpcalc`** - High-precision arithmetic operations
- **[`regexfilter`](doc/regexfilter.md)** - Regex-based blacklist/whitelist filtering
- **`atxsort`** - Universal sorting algorithms
- **[`ini`](doc/ini.md)** - INI configuration parser and writer
- **`platform`** - Cross-platform utilities and abstractions
- **`xml`** - XML parsing and serialization library (in development) (Warning: Though the module is basically functional, some functions are yet unsafe and cause crashes.)
- **`html`** - HTML parsing and manipulation library (in development)

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

**Available targets:** `variant`, `logd`, `logf`, `sha256`, `basic`, `indexer`, `regexfilter`, `logt`, `logc`, `Base64`, `platform`, `arguments`

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


### Some comments left by Dev:
Microsoft is doing a bad job at there api (windows.h explicitly). 

It contains SO MANY macros that pollute the global namespace. Some of them can not even work with the C++ standard.

In development, if you are using VSCode for example, be careful when some of your code turned blue, which probably indicates that some macro is messing with your code.

I have no idea how to fix these. I tried to re-package some of the windows.h content in a more modern, standard and namespace nested way in another project, but it is just too much work and it is almost impossible to cover all the use cases.

If you are developing something on Windows, espeacially some shared modules, I recommend that you avoid including windows.h in any of your header files. Only include them in the .cpp files.

And always define `NOMINMAX` and `WIN32_LEAN_AND_MEAN` before including windows.h, to reduce the pollution. If some names are really unusable, like Process32First and Process32Next that somehow Microsoft developers completely covered the ANSI version, you can just `#undef` them.

Note that it would ALWAYS be a good habit of using A and W suffix version of Windows API. I think the macros defined in windows.h is completely for compatibility with old codes (which is a main goal for them). But it has nothing good with modern C++ code. So just use the A and W version directly, and you can avoid a lot of problems.

I added some warnings in the code. You can experience some of my fixes in the `platform_windows` module. They are hardly enough, so add your thing if you also finds something.