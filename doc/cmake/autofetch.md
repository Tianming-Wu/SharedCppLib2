# Auto-fetch Dependencies with CMake

+ Name: Auto-fetch Guide  
+ Document Version: `1.0.0`

## Overview

CMake's `FetchContent` module allows you to automatically download and build dependencies as part of your build process. This eliminates the need for manual installation of SharedCppLib2 and ensures consistent version management across different development environments.

## Basic Usage

### Method 1: Direct Fetch (Recommended)
```cmake
include(FetchContent)

FetchContent_Declare(
  SharedCppLib2
  GIT_REPOSITORY https://github.com/Tianming-Wu/SharedCppLib2.git
  GIT_TAG        main  # or specific version tag
)

FetchContent_MakeAvailable(SharedCppLib2)

# Link with your target
target_link_libraries(your_target SharedCppLib2::basic)
```

### Method 2: With Specific Version
```cmake
include(FetchContent)

FetchContent_Declare(
  SharedCppLib2
  GIT_REPOSITORY https://github.com/Tianming-Wu/SharedCppLib2.git
  GIT_TAG        v1.17.1  # Use specific release version
)

FetchContent_MakeAvailable(SharedCppLib2)
```

## Complete Example

### CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.14)
project(MyProject)

set(CMAKE_CXX_STANDARD 23)

# Enable FetchContent
include(FetchContent)

# Declare SharedCppLib2 dependency
FetchContent_Declare(
  SharedCppLib2
  GIT_REPOSITORY https://github.com/Tianming-Wu/SharedCppLib2.git
  GIT_TAG        main
)

# Make it available
FetchContent_MakeAvailable(SharedCppLib2)

# Create your executable
add_executable(my_app main.cpp)

# Link with SharedCppLib2
target_link_libraries(my_app SharedCppLib2::basic)
```

### main.cpp
```cpp
#include <SharedCppLib2/stringlist.hpp>
#include <iostream>

int main(int argc, char** argv) {
    std::stringlist args(argc, argv);
    
    std::cout << "Program: " << args.vat(0) << std::endl;
    std::cout << "Arguments: " << args.subarr(1).join(" ") << std::endl;
    
    return 0;
}
```

## Advanced Configuration

### Using Specific Branch
```cmake
FetchContent_Declare(
  SharedCppLib2
  GIT_REPOSITORY https://github.com/Tianming-Wu/SharedCppLib2.git
  GIT_TAG        development  # Use development branch
)
```

### With Commit Hash
```cmake
FetchContent_Declare(
  SharedCppLib2
  GIT_REPOSITORY https://github.com/Tianming-Wu/SharedCppLib2.git
  GIT_TAG        a1b2c3d4e5f67890  # Specific commit hash
)
```

### Custom Build Options
```cmake
# Set options before making available
set(CMAKE_BUILD_TYPE Release)  # Force Release build

FetchContent_Declare(
  SharedCppLib2
  GIT_REPOSITORY https://github.com/Tianming-Wu/SharedCppLib2.git
  GIT_TAG        main
)

FetchContent_MakeAvailable(SharedCppLib2)
```

## Multiple Dependencies Example

```cmake
cmake_minimum_required(VERSION 3.14)
project(MyProject)

include(FetchContent)

# Fetch SharedCppLib2
FetchContent_Declare(
  SharedCppLib2
  GIT_REPOSITORY https://github.com/Tianming-Wu/SharedCppLib2.git
  GIT_TAG        main
)

# Fetch another dependency (example)
FetchContent_Declare(
  jsoncpp
  GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
  GIT_TAG        1.9.5
)

# Make all dependencies available
FetchContent_MakeAvailable(SharedCppLib2 jsoncpp)

add_executable(my_app main.cpp)
target_link_libraries(my_app 
    SharedCppLib2::basic
    jsoncpp_lib
)
```

## Available Targets

After fetching, you can link with these SharedCppLib2 targets:

- `SharedCppLib2::basic` (includes stringlist, bytearray)
- `SharedCppLib2::sha256`
- `SharedCppLib2::logt`
- `SharedCppLib2::logc`
- `SharedCppLib2::regexfilter`
- `SharedCppLib2::variant`
- `SharedCppLib2::indexer`
- `SharedCppLib2::Base64`
- `SharedCppLib2::platform`

## Best Practices

### 1. Version Pinning
Always specify a specific version tag or commit hash for production use:
```cmake
GIT_TAG v1.17.1  # Good: Specific version
GIT_TAG main     # Acceptable: Latest development (for testing)
```

### 2. Build Type
Set build type according to your needs:
```cmake
set(CMAKE_BUILD_TYPE Release)  # For production
set(CMAKE_BUILD_TYPE Debug)    # For development
```

### 3. Cache Control
Use `FETCHCONTENT_UPDATES_DISCONNECTED` to prevent automatic updates during development:
```cmake
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
```

### 4. Offline Development
For offline scenarios, you can pre-download dependencies:
```bash
cmake -B build -DFETCHCONTENT_FULLY_DISCONNECTED=ON
```

## Troubleshooting

### Common Issues

1. **Network Errors**: Ensure you have internet access during the first build
2. **Version Conflicts**: Use specific tags to avoid breaking changes
3. **Build Failures**: Check that your CMake version is 3.14 or newer

### Debugging
Enable verbose output for debugging:
```cmake
set(FETCHCONTENT_QUIET OFF)
```

## Benefits

- **Simplified Setup**: No manual installation required
- **Version Control**: Exact versions specified in CMakeLists.txt
- **Reproducible Builds**: Consistent across all environments
- **CI/CD Friendly**: Works seamlessly in automated build systems

## Comparison with Manual Installation

| Aspect | Manual Installation | Auto-fetch |
|--------|---------------------|------------|
| Setup Complexity | Manual steps required | Fully automated |
| Version Management | Manual updates | Controlled in CMake |
| Team Collaboration | Inconsistent versions | Consistent across team |
| CI/CD Integration | Additional setup | Works out-of-the-box |

This approach is recommended for most projects, especially when starting new development or working in team environments.