# regexfilter - 正则表达式过滤库

+ 名称: regexfilter  
+ 命名空间: `rf`  
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `regexfilter` |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::regexfilter)
```

## 描述

regexfilter 是一个基于正则表达式的强大字符串过滤库，为 stringlist 操作提供黑名单和白名单功能。它使用 C++ 标准正则表达式实现高级模式匹配和过滤能力。

## 快速开始

### 基本用法
```cpp
#include <SharedCppLib2/regexfilter.hpp>
#include <SharedCppLib2/stringlist.hpp>

int main() {
    std::stringlist files = {"main.cpp", "temp.txt", "backup.tmp", "header.h"};
    
    // 创建黑名单排除临时文件
    rf::blacklist exclude_temp({".*\\.tmp", "temp.*"});
    exclude_temp.apply(files);
    
    // files 现在包含: {"main.cpp", "header.h"}
    
    return 0;
}
```

### 白名单示例
```cpp
std::stringlist sources = {"main.cpp", "util.cpp", "data.txt", "config.ini"};

// 创建白名单只保留 C++ 源文件
rf::whitelist include_cpp({".*\\.cpp", ".*\\.hpp"});
include_cpp.apply(sources);

// sources 现在包含: {"main.cpp", "util.cpp"}
```

## 核心类

### base_list
实现核心正则匹配和过滤逻辑的基础类。

#### 构造函数
```cpp
base_list(const std::stringlist& patterns = std::stringlist());
```
使用指定的正则表达式模式创建过滤器。无效的正则表达式模式会被静默忽略。

#### filtered
```cpp
bool filtered(const std::string& s) const;
```
检查字符串是否匹配任何正则表达式模式。

**返回值:** 如果字符串匹配任何模式返回 `true`，否则返回 `false`。

#### apply
```cpp
int apply(std::stringlist& list, bool reverse) const;
```
对 stringlist 应用过滤。

**参数:**
- `list`: 要过滤的 stringlist（原位修改）
- `reverse`: 如果为 `true`，保留匹配项；如果为 `false`，移除匹配项

**返回值:** 从列表中移除的项目数量

### blacklist
排除匹配指定模式的字符串。

#### 构造函数
```cpp
blacklist(const std::stringlist& patterns = std::stringlist());
```
使用指定的排除模式创建黑名单。

#### filtered
```cpp
bool filtered(const std::string& s) const;
```
**返回值:** 如果字符串匹配任何黑名单模式（应被排除）返回 `true`

#### apply
```cpp
bool apply(std::stringlist& list) const;
```
从列表中移除所有匹配黑名单模式的字符串。

**返回值:** 如果有任何项目被移除返回 `true`

### whitelist
只包含匹配指定模式的字符串。

#### 构造函数
```cpp
whitelist(const std::stringlist& patterns = std::stringlist());
```
使用指定的包含模式创建白名单。

#### filtered
```cpp
bool filtered(const std::string& s) const;
```
**返回值:** 如果字符串不匹配任何白名单模式（应被排除）返回 `true`

#### apply
```cpp
bool apply(std::stringlist& list) const;
```
从列表中移除所有不匹配白名单模式的字符串。

**返回值:** 如果有任何项目被移除返回 `true`

## 高级用法

### 复杂模式匹配
```cpp
// 过滤特定域名的电子邮件
rf::blacklist email_filter({".*@spam\\.com", ".*@temp\\.org"});

// 使用复杂模式过滤文件
rf::whitelist source_files({
    ".*\\.(cpp|cxx|cc)$", 
    ".*\\.(hpp|hxx|hh)$",
    "CMakeLists\\.txt",
    ".*\\.cmake$"
});
```

### 组合多个过滤器
```cpp
std::stringlist items = {"file1.cpp", "file2.txt", "temp.cpp", "backup.h"};

// 首先排除临时文件
rf::blacklist temp_filter({"temp.*", "backup.*"});
temp_filter.apply(items);

// 然后只保留源文件
rf::whitelist source_filter({".*\\.cpp", ".*\\.h"});
source_filter.apply(items);

// items 现在包含: {"file1.cpp"}
```

### 错误处理
```cpp
// 无效的正则表达式模式会自动忽略
rf::blacklist safe_filter({"valid.*", "[invalid-regex", "another.*"});
// 只有 "valid.*" 和 "another.*" 会被使用
```

## 实际应用示例

### 日志文件过滤
```cpp
std::stringlist log_entries = {
    "2024-01-15 INFO: 应用程序已启动",
    "2024-01-15 DEBUG: 加载配置中",
    "2024-01-15 ERROR: 数据库连接失败",
    "2024-01-15 WARN: 内存使用率过高"
};

// 过滤掉 DEBUG 消息
rf::blacklist debug_filter({".*DEBUG:.*"});
debug_filter.apply(log_entries);

// 只保留 ERROR 和 WARN 消息
rf::whitelist error_filter({".*ERROR:.*", ".*WARN:.*"});
error_filter.apply(log_entries);
```

### 文件系统操作
```cpp
#include <filesystem>
namespace fs = std::filesystem;

std::stringlist get_filtered_files(const fs::path& directory) {
    std::stringlist all_files;
    
    for (const auto& entry : fs::directory_iterator(directory)) {
        all_files.push_back(entry.path().filename().string());
    }
    
    // 排除系统和临时文件
    rf::blacklist system_files({
        "\\..*",           // 隐藏文件 (Unix)
        ".*\\.tmp",        // 临时文件
        "Thumbs\\.db",     // Windows 缩略图缓存
        "~\\.*"            // 备份文件
    });
    
    system_files.apply(all_files);
    return all_files;
}
```

### 配置过滤
```cpp
std::stringlist load_config_with_filter(const std::string& filename) {
    std::ifstream file(filename);
    std::stringlist lines;
    
    // 读取所有行
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    
    // 移除注释和空行
    rf::blacklist comments({
        "^\\s*#.*",    // Shell/Python 风格注释
        "^\\s*//.*",   // C++ 风格注释
        "^\\s*;.*",    // INI 风格注释
        "^\\s*$"       // 空行
    });
    
    comments.apply(lines);
    return lines;
}
```

## 性能提示

1. **预编译模式**: 构造过滤器对象一次并重复使用
2. **使用特定模式**: 更具体的正则表达式模式匹配速度更快
3. **组合操作**: 按顺序应用多个过滤器，而不是创建复杂的单一模式
4. **批量处理**: 如果内存有限，分批过滤大列表

## 模式示例

### 常用正则表达式模式
```cpp
// 文件扩展名
".*\\.(jpg|jpeg|png|gif)$"           // 图像文件
".*\\.(cpp|cxx|cc|hpp|hxx|hh)$"      // C++ 源文件
".*\\.(txt|md|rst)$"                 // 文本文件

// 日志级别
".*\\b(ERROR|FATAL)\\b:.*"           // 错误消息
".*\\b(DEBUG|TRACE)\\b:.*"           // 调试消息

// 数据格式
"^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"  // 电子邮件地址
"^\\d{4}-\\d{2}-\\d{2}$"                            // 日期格式
```

## 与 StringList 的集成

regexfilter 与 stringlist 无缝协作：

```cpp
std::stringlist data = std::stringlist::split(input_text, "\n")
                      .remove_empty();

rf::blacklist unwanted_patterns({"垃圾内容", "广告"});
unwanted_patterns.apply(data);

// 继续处理清理后的数据
```