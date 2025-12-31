# stringlist - 增强型字符串列表库

+ 名称: StringList  
+ 命名空间: `std`  
+ 文档版本: `3.22.5`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `basic` (包含 stringlist) |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::basic)
```

## 描述

StringList 是 `std::vector<std::string>` 的强大扩展，提供了 Qt 风格的字符串操作能力。它继承了所有标准向量操作，同时添加了方便的字符串处理、解析和转换方法。

> [!TIP]
> 如果您熟悉 Qt 的 `QStringList`，您会发现 StringList 为标准 C++ 字符串提供了类似的功能。

## 快速开始

### 基本用法
```cpp
#include <SharedCppLib2/stringlist.hpp>

// 从初始化列表创建
std::stringlist names = {"Alice", "Bob", "Charlie"};

// 使用分隔符连接
std::cout << names.join(", ") << std::endl;
// 输出: Alice, Bob, Charlie

// 将字符串分割为列表
std::stringlist words = std::stringlist::split("hello world from cpp", " ");
```

### 基本构造
```cpp
int main(int argc, char** argv) {
    std::stringlist args(argc, argv);
    
    std::cout << "程序: " << args.vat(0) << std::endl;
    std::cout << "参数: " << args.subarr(1).join(" ") << std::endl;
    
    return 0;
}
```

> [!TIP]
> 对于带有类型安全和自动验证的高级命令行参数解析，请参阅 [`arguments`](arguments.md)。

## 核心功能

### 🔗 字符串连接与分割
在字符串和字符串列表之间转换的高级方法。

### 🔍 搜索与过滤
强大的搜索能力和数据清理操作。

### 📦 数据转换
多种数据源的构造函数。

### 🛠️ 实用操作
常见字符串列表操作的便捷方法。

## 函数参考

### 字符串转换

#### join
```cpp
string join(const string &separator = " ") const;
string join(size_t begin, size_t size = -1, const string &separator = " ") const;
```
将列表元素连接成单个字符串。

**示例:**
```cpp
std::stringlist sl = {"a", "b", "c"};
sl.join();           // "a b c"
sl.join(", ");       // "a, b, c"
sl.join(1, 2, "-");  // "b-c" (从索引1开始，2个元素)
```

#### xjoin
```cpp
string xjoin(const string &separator = " ", const char binding = '\"') const;
```
使用自动引号连接包含分隔符的元素。

**示例:**
```cpp
std::stringlist sl = {"file", "path with spaces"};
sl.xjoin(" ");  // "file \"path with spaces\""
```

#### dbgjoin
```cpp
string dbgjoin(string delimiter = "'") const;
```
使用分隔符连接，便于调试查看。

**示例:**
```cpp
sl.dbgjoin("|");  // "|a|b|c|"
```

#### split
```cpp
static stringlist split(const string &s, const string &delimiter);
static stringlist split(const string &s, const stringlist &delimiters);
```
使用单个或多个分隔符将字符串分割为列表。

**示例:**
```cpp
// 单个分隔符
std::stringlist::split("a,b,c", ",");  // {"a", "b", "c"}

// 多个分隔符  
std::stringlist::split("a,b c\td", {",", " ", "\t"});  // {"a", "b", "c", "d"}
```

> [!NOTE]
> 分割后使用 `remove_empty()` 清理连续分隔符产生的空元素。

#### xsplit & exsplit
```cpp
static stringlist xsplit(const string &s, const string &delim, 
                        const string &begin_bind, string end_bind = "", 
                        bool remove_binding = true);

static stringlist exsplit(const string &s, const string &delim,
                         const string &begin_bind, string end_bind = "",
                         bool remove_binding = false, bool strict = false);
```
支持引号/括号感知的高级分割。

**示例:**
```cpp
// 处理带引号的部分
std::stringlist::xsplit("cmd arg1 \"quoted arg\" arg3", " ", "\"");
// {"cmd", "arg1", "quoted arg", "arg3"}
```

### 搜索操作

#### find & find_last
```cpp
size_t find(const std::string &value, size_t start = 0) const;
size_t find_last(const std::string &value) const;
```
查找字符串的第一次/最后一次出现。

**返回值:** 索引或 `stringlist::npos`（如果未找到）。

#### find_inside
```cpp
point find_inside(const std::string &substring, size_t start = 0, 
                 size_t start_inside = 0) const;
```
在任何列表元素中查找子字符串。

**返回值:** `pair<元素索引, 元素内位置>` 或 `npoint`。

#### contains
```cpp
bool contains(const std::string &value) const;
```
检查是否有任何元素包含该值。

### 数据管理

#### vat
```cpp
string vat(size_t index, const string &default_value = "") const;
```
安全的元素访问，支持默认值。

#### subarr
```cpp
std::stringlist subarr(size_t start, size_t length = 0) const;
```
从列表中提取子范围。

#### remove_empty
```cpp
void remove_empty();
```
从列表中移除所有空字符串。

#### unique
```cpp
stringlist unique();
```
移除重复字符串（保持顺序）。

### 函数式编程

#### exec_foreach
```cpp
void exec_foreach(function<void(size_t, string&)> callback);
```
对每个元素应用函数。

**示例:**
```cpp
sl.exec_foreach([](size_t index, std::string& value) {
    value = std::to_string(index) + ":" + value;
});
```

## 构造函数参考

### 从 C 风格参数
```cpp
stringlist(int argc, char** argv, int start = 0, int end = -1);
```
将 C 风格参数转换为 stringlist。对于高级参数解析，请参阅 [`arguments`](arguments.md)。

### 从初始化列表
```cpp
stringlist(initializer_list<string> elements);
```
```cpp
std::stringlist fruits = {"apple", "banana", "orange"};
```

### 从字符串分割
```cpp
stringlist(const string &text, const string &delimiter);
stringlist(const string &text, const stringlist &delimiters);
```
```cpp
std::stringlist words("hello world from cpp", " ");
```

### 从单个字符串
```cpp
explicit stringlist(const string &single_element);
```
创建包含一个元素的列表。

## 高级用法

### 打包/解包用于序列化
```cpp
std::stringlist data = {"normal", "text with spaces"};
std::string packed = data.pack();  // 自动为空格添加引号
std::stringlist restored = std::stringlist::unpack(packed);
```

### 流集成
```cpp
std::stringlist items;
std::cin >> stringist_split(",", items);  // 解析 CSV 输入
```

### 性能提示

1. **使用 `vat()`** 进行安全的元素访问，而不是边界检查
2. **预分配** 可能的大列表
3. **高效链式操作**:
   ```cpp
   auto result = std::stringlist::split(input, " ")
                 .remove_empty()
                 .unique();
   ```

## 实际示例

### 配置解析
```cpp
std::stringlist config_lines = std::stringlist::split(config_text, "\n")
                               .remove_empty();

for (const auto& line : config_lines) {
    if (line.starts_with("#")) continue;  // 跳过注释
    auto parts = std::stringlist::split(line, "=");
    if (parts.size() == 2) {
        config[parts[0]] = parts[1];
    }
}
```

### 命令构建器
```cpp
std::string build_command(const std::string& program, 
                         const std::vector<std::string>& args) {
    std::stringlist cmd = {program};
    cmd.append(args);  // 添加整个向量
    return cmd.xjoin(" ");  // 自动管理带空格的参数
}

// 或单行版本：
std::string build_command(const std::string& program, 
                         const std::vector<std::string>& args) {
    return std::stringlist{program}.append(args).xjoin(" ");
}
```

## 常见模式

## 搜索与过滤
强大的搜索能力和数据清理操作。对于基于正则表达式的高级过滤，请参考 [regexfilter 库](../regexfilter.md#核心类)。

## 常见模式

### 过滤
```cpp
std::stringlist files = /* ... */;

// 方法1：使用 exec_foreach 和 remove_empty
files.exec_foreach([](size_t i, std::string& file) {
    if (!file.ends_with(".cpp")) {
        file.clear();  // 标记为移除
    }
});
files.remove_empty();

// 方法2：使用 regexfilter 进行高级模式匹配
#include <SharedCppLib2/regexfilter.hpp>
rf::whitelist cpp_files({".*\\.cpp", ".*\\.hpp"});
cpp_files.apply(files);

// 方法3：结合 stringlist 和 regexfilter
auto result = std::stringlist::split(input, "\n")
              .remove_empty()
              .apply_filter([](const std::string& s) {
                  return s.contains("important");
              });
```

## 高级过滤

虽然 stringlist 提供了基础的过滤功能，但对于复杂的模式匹配需求，建议使用专门的 [regexfilter 库](../regexfilter.md#核心类)。

### 简单过滤（内置）
```cpp
// 移除空字符串
list.remove_empty();

// 使用 lambda 过滤
list.exec_foreach([](size_t i, std::string& item) {
    if (item.length() < 3) item.clear();
});
list.remove_empty();
```

### 高级过滤（使用 regexfilter）
```cpp
#include <SharedCppLib2/regexfilter.hpp>

// 创建白名单过滤器
rf::whitelist valid_extensions({".*\\.txt", ".*\\.md", ".*\\.cpp"});
valid_extensions.apply(files);

// 创建黑名单过滤器  
rf::blacklist exclude_patterns({"temp.*", ".*\\.tmp", "backup.*"});
exclude_patterns.apply(files);
```

更多过滤选项请参考 [regexfilter 完整文档](../regexfilter.md#高级用法)。

### 转换
```cpp
std::stringlist paths = {"dir1/file1", "dir2/file2"};
paths.exec_foreach([](size_t i, std::string& path) {
    path = "/usr/local/" + path;
});
```