# SHARED CPP Library 2 (SharedCppLib2)

Other languages: [中文](README-zh_CN.md)

**Author**: Tianming ([GitHub](https://github.com/Tianming-Wu))

**Project Page**: [GitHub Repository](https://github.com/Tianming-Wu/SharedCppLib2)

> [!NOTE]
> This library is a dependency for many of my projects. You need to install it first to use those projects, or utilize [automatic dependency fetching](doc/cmake/autofetch.md).

> [!WARNING]
> If you encounter some strange behavior, just MAKE SURE you are using the right build configuration (Debug/Release) for both the library and your project. This is a common mistake that can cause the old bugs to appear even after you updated the library, since this library was configured to keep the binaries at the same time for both Debug and Release (for easier development).

**Free software with NO WARRANTY.** Some modules may be incomplete.

You may use this library for personal or commercial purposes. Attribution is appreciated but not required.

## Overview

A collection of modern C++ utility libraries designed for performance and ease of use. Built with C++23 standard.

Some modules of this project are basically re-inventing the wheel. The purpose is to learn something.

This library is building on top of the standard library, with its own convenient set of APIs. The SharedCppLib2 API is designed for extremely easy use, and it is not hard to learn. Check [API Reference](doc/api_reference.md) for details.

I still tried to make the API design as good (friendly, easy-to-use) as possible.

## Core Modules

- **`ansiio`** - ANSI terminal output and formatting
- **[`stringlist`](doc/stringlist.md)** - Enhanced string manipulation with Qt-inspired API
- **[`arguments`](doc/arguments.md)** - Type-safe command-line argument parsing (argparse-inspired)
- **`Base64`** - Base64 encoding and decoding utilities
- **[`bytearray`](doc/bytearray.md)** - Binary data management and serialization
- **[`sha256`](doc/sha256.md)** - SHA-256 cryptographic hash implementation
- **[`hmac`](doc/hmac.md)** - Header-only HMAC implementation based on hash providers
- **[`logt`](doc/logt.md)** - High-performance asynchronous logging
- **[`logc`](doc/logc.md)** - Colored console output for logt
- **`indexer`** - Data indexing and search utilities
- **`hpcalc`** - High-precision arithmetic operations
- **[`regexfilter`](doc/regexfilter.md)** - Regex-based blacklist/whitelist filtering
- **`atxsort`** - Universal sorting algorithms
- **[`ini`](doc/ini.md)** - INI configuration parser and writer
- **[`json`](doc/json.md)** - JSON parsing, manipulation and serialization (early development)
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

**Available targets:** `variant`, `logd`, `logf`, `sha256`, `basic`, `indexer`, `regexfilter`, `logt`, `logc`, `Base64`, `platform`, `arguments`, `ini`, `abstract`, `xml`, `debug`, `stream`, `console`, `keydb`, `types`, `condition`, `filesystem`, `datauri`, `json`, `network_core`, `network_dns`, `network_tcp`, `network_udp`, `network_http`, `network`, `api`, `hmac`, `logh`, `rerr`, `bits`, `cache`, `exexception`, `engineering`, `multindex`, `percentage`, `RAII`, `singleinst`, `structural_binding`, `typemask`

### 4. Code Example
```cpp
#include <SharedCppLib2/stringlist.hpp>

int main(int argc, char** argv) {
    std::stringlist args(argc, argv);
    // Process command line arguments
    return 0;
}
```

### 5. HMAC Example (with SHA256 provider)

`hmac` is provider-agnostic. The sample below uses `sha256` as provider.

```cpp
#include <SharedCppLib2/hmac.hpp>
#include <SharedCppLib2/sha256.hpp>

std::bytearray key(std::string("secret-key"));
std::bytearray payload(std::string("binary-message"));

std::bytearray tag = scl2::hmac<scl2::sha256>::compute(payload, key);
```

For tag verification, use constant-time comparison:
- [constant_time_compare](doc/constant_time_compare.md)

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