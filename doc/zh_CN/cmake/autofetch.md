# 使用 CMake 自动获取依赖

+ 名称: 自动获取指南  
+ 文档版本: `1.0.0`

## 概述

CMake 的 `FetchContent` 模块允许您在构建过程中自动下载和构建依赖项。这消除了手动安装 SharedCppLib2 的需要，并确保在不同开发环境中保持一致的版本管理。

## 基本用法

### 方法 1：直接获取（推荐）
```cmake
include(FetchContent)

FetchContent_Declare(
  SharedCppLib2
  GIT_REPOSITORY https://github.com/Tianming-Wu/SharedCppLib2.git
  GIT_TAG        main  # 或特定版本标签
)

FetchContent_MakeAvailable(SharedCppLib2)

# 与您的目标链接
target_link_libraries(您的目标 SharedCppLib2::basic)
```

### 方法 2：使用特定版本
```cmake
include(FetchContent)

FetchContent_Declare(
  SharedCppLib2
  GIT_REPOSITORY https://github.com/Tianming-Wu/SharedCppLib2.git
  GIT_TAG        v1.17.1  # 使用特定发布版本
)

FetchContent_MakeAvailable(SharedCppLib2)
```

## 完整示例

### CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.14)
project(MyProject)

set(CMAKE_CXX_STANDARD 23)

# 启用 FetchContent
include(FetchContent)

# 声明 SharedCppLib2 依赖
FetchContent_Declare(
  SharedCppLib2
  GIT_REPOSITORY https://github.com/Tianming-Wu/SharedCppLib2.git
  GIT_TAG        main
)

# 使其可用
FetchContent_MakeAvailable(SharedCppLib2)

# 创建您的可执行文件
add_executable(my_app main.cpp)

# 与 SharedCppLib2 链接
target_link_libraries(my_app SharedCppLib2::basic)
```

### main.cpp
```cpp
#include <SharedCppLib2/stringlist.hpp>
#include <iostream>

int main(int argc, char** argv) {
    std::stringlist args(argc, argv);
    
    std::cout << "程序: " << args.vat(0) << std::endl;
    std::cout << "参数: " << args.subarr(1).join(" ") << std::endl;
    
    return 0;
}
```

## 高级配置

### 使用特定分支
```cmake
FetchContent_Declare(
  SharedCppLib2
  GIT_REPOSITORY https://github.com/Tianming-Wu/SharedCppLib2.git
  GIT_TAG        development  # 使用开发分支
)
```

### 使用提交哈希
```cmake
FetchContent_Declare(
  SharedCppLib2
  GIT_REPOSITORY https://github.com/Tianming-Wu/SharedCppLib2.git
  GIT_TAG        a1b2c3d4e5f67890  # 特定提交哈希
)
```

### 自定义构建选项
```cmake
# 在使其可用之前设置选项
set(CMAKE_BUILD_TYPE Release)  # 强制发布构建

FetchContent_Declare(
  SharedCppLib2
  GIT_REPOSITORY https://github.com/Tianming-Wu/SharedCppLib2.git
  GIT_TAG        main
)

FetchContent_MakeAvailable(SharedCppLib2)
```

## 多个依赖项示例

```cmake
cmake_minimum_required(VERSION 3.14)
project(MyProject)

include(FetchContent)

# 获取 SharedCppLib2
FetchContent_Declare(
  SharedCppLib2
  GIT_REPOSITORY https://github.com/Tianming-Wu/SharedCppLib2.git
  GIT_TAG        main
)

# 获取另一个依赖项（示例）
FetchContent_Declare(
  jsoncpp
  GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
  GIT_TAG        1.9.5
)

# 使所有依赖项可用
FetchContent_MakeAvailable(SharedCppLib2 jsoncpp)

add_executable(my_app main.cpp)
target_link_libraries(my_app 
    SharedCppLib2::basic
    jsoncpp_lib
)
```

## 可用目标

获取后，您可以与这些 SharedCppLib2 目标链接：

- `SharedCppLib2::basic`（包含 stringlist、bytearray）
- `SharedCppLib2::sha256`
- `SharedCppLib2::logt`
- `SharedCppLib2::logc`
- `SharedCppLib2::regexfilter`
- `SharedCppLib2::variant`
- `SharedCppLib2::indexer`
- `SharedCppLib2::Base64`
- `SharedCppLib2::platform`

## 最佳实践

### 1. 版本固定
在生产使用中始终指定特定的版本标签或提交哈希：
```cmake
GIT_TAG v1.17.1  # 良好：特定版本
GIT_TAG main     # 可接受：最新开发版本（用于测试）
```

### 2. 构建类型
根据您的需求设置构建类型：
```cmake
set(CMAKE_BUILD_TYPE Release)  # 用于生产
set(CMAKE_BUILD_TYPE Debug)    # 用于开发
```

### 3. 缓存控制
使用 `FETCHCONTENT_UPDATES_DISCONNECTED` 防止开发期间自动更新：
```cmake
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
```

### 4. 离线开发
对于离线场景，您可以预下载依赖项：
```bash
cmake -B build -DFETCHCONTENT_FULLY_DISCONNECTED=ON
```

## 故障排除

### 常见问题

1. **网络错误**：确保在首次构建期间有互联网访问
2. **版本冲突**：使用特定标签以避免破坏性更改
3. **构建失败**：检查您的 CMake 版本是否为 3.14 或更新

### 调试
启用详细输出以进行调试：
```cmake
set(FETCHCONTENT_QUIET OFF)
```

## 优势

- **简化设置**：无需手动安装
- **版本控制**：在 CMakeLists.txt 中指定确切版本
- **可重现构建**：在所有环境中保持一致
- **CI/CD 友好**：在自动化构建系统中无缝工作

## 与手动安装的比较

| 方面 | 手动安装 | 自动获取 |
|------|----------|----------|
| 设置复杂性 | 需要手动步骤 | 完全自动化 |
| 版本管理 | 手动更新 | 在 CMake 中控制 |
| 团队协作 | 版本不一致 | 团队间一致 |
| CI/CD 集成 | 需要额外设置 | 开箱即用 |

这种方法推荐用于大多数项目，特别是在开始新开发或在团队环境中工作时。