# SHARED CPP Library 2 (SharedCppLib2)

**Author**: Tianming ([github](https://github.com/Tianming-Wu))

**Project Page**: ([github](https://github.com/Tianming-Wu/SharedCppLib2))

> [!NOTE]
> This project is used by many of my projects. You have to install this first to use some of them. Or try auto fetch.

This is free software with NO ABSOLUTE WARRENTY. Some of the Modules
may be incomplete.

You may use this library for personal or commercial usage. You don't
have to mention the usage of this library in your README or about,
but if you do so I will appreciate.

This is a series of useful cpp classes for multiple purposes.

- `ansiio` for ANSI output features.
- [`stringlist`](doc/stringlist.md) for string list manipulation capabilities.
- `Base64` Base64 encode/decode.
- `bytearray`
- `sha256` for encryption (incomplete).
- [`logt`](doc/logt.md) Sub-threaded logging.
- [`logc`](doc/logc.md) Provides colored logging (ansi features).
- `indexer` for data indexing.
- `logd` simple console logger. (DO NOT USE)
- `logf` simple file logger. (DO NOT USE)
- `hpcalc` high precision calculation.
- `regexfilter` for blacklist / whitelist filter features.

**Deprecated modules:**
- `eventloop` (Seperated to independent project)
- `tasker` Sub-thread task manager (Cannot fix)

## How to install
### 1. Get source from github:
```bash
git clone https://github.com/Tianming-Wu/SharedCppLib2
```

### 2. Install using CMake (Debug):
```bash
cd SharedCppLib2
# Configure Project
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cd build
# Build and install
cmake --build build --config Debug
cmake --install build --config Debug
```

#### Or install Release version:
```bash
cd SharedCppLib2
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release  
cmake --install build --config Release
```

> [!NOTE]
> If you are on Windows, you may need to configure cmake path first, otherwise run in an Administrator prompt, or use `sudo` introduced in the newest Windows 11.

> [!NOTE]
> The `--config` parameter is only needed for multi-configuration generators (like Visual Studio). 
> For single-config generators (like Makefiles, Ninja), use `-DCMAKE_BUILD_TYPE=Debug/Release` instead.

### 3. Add to your project's CMakeLists.txt:
```
find_package(SharedCppLib2 REQUIRED)

target_link_libraries(yourtarget SharedCppLib2::basic)
```

Available target names can be found in [CMakeLists.txt](CMakeLists.txt):
```
# 安装目标
install(TARGETS variant logd logf sha256 basic indexer regexfilter logt logc
...
```

### 4. In your code
```cpp
#include <SharedCppLib2/stringlist.hpp>

int main(int argc, char** argv) {
    std::stringlist args(argc, argv);

    return 0;
}
```
Full usage can be found as documentation inside .hpp files as Doxygen comments.

Full Changelog: [WhatsNew](WhatsNew)

