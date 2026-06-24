# yaml - YAML 库

+ 名称: yaml
+ 命名空间: `scl2::yaml`
+ 文档版本: `0.1.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `yaml` |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::yaml)
```

```cpp
#include <SharedCppLib2/yaml.hpp>
```

## 描述

`yaml` 为 SharedCppLib2 提供了 YAML 1.2 解析功能。它支持基于行的流式解析器，并通过特性标志逐步引入完整的 YAML 规范。

> [!WARNING]
> 该模块处于早期开发阶段（v0.1.0）。目前实现了块式映射和序列、标量、注释以及双引号转义。流式风格（`{}`、`[]`）、锚点（`&`、`*`）、标签（`!!`）和多文档流尚未支持，但已在特性标志系统中预留了设计。
>
> **该模块仅支持读取。** 目前没有导出器或序列化支持。可以将 YAML 解析为 `document` 树进行读取，但无法写入 YAML 输出。

### 独立导入

你可以将 `yaml.hpp` 和 `yaml.cpp` 作为独立文件使用，无需安装完整的 SharedCppLib2。将它们复制到项目中并在 CMake 中添加：

```cmake
add_library(sclyaml yaml.hpp yaml.cpp)
target_link_libraries(yourtarget sclyaml)
```

任何有效的构建配置均可——以上示例仅是一种方式。

## 快速开始

### 解析与访问

```cpp
#include <SharedCppLib2/yaml.hpp>

auto doc = scl2::yaml::document::fromString(
    "name: Bob\n"
    "scores:\n"
    "  - 95\n"
    "  - 87\n"
);

std::string name   = doc["name"].as_string();     // "Bob"
int64_t    first   = doc["scores"][0].as_int();   // 95
bool       is_obj  = doc["scores"].is_array();    // true
```

### 从文件解析

```cpp
#include <fstream>

std::ifstream file("config.yaml");
auto doc = scl2::yaml::document::fromStream(file);
```

### 特性标志

控制解析器接受哪些 YAML 特性：

```cpp
scl2::yaml::features feat;
feat.comments   = false;  // 禁用 # 注释
feat.anchors    = true;   // 启用 &/*（待实现）

auto doc = scl2::yaml::document::fromString(input, feat);
```

## 核心 API

### value

底层数据类型。存储以下之一：`nullptr`、`bool`、`int64_t`、`double`、`std::string`、`std::vector<value>`、`std::map<std::string, value>`、`alias_ref`。

#### 类型检查

```cpp
bool is_null() const;
bool is_bool() const;
bool is_int() const;
bool is_double() const;
bool is_string() const;
bool is_array() const;
bool is_object() const;
bool is_alias() const;
```

#### 类型访问（const 和 mutable）

```cpp
nullptr_t as_null() const;
bool as_bool() const;
int64_t as_int() const;
double as_double() const;
const std::string& as_string() const;
const std::vector<value>& as_array() const;
const std::map<std::string, value>& as_object() const;
const alias_ref& as_alias() const;
```

每个 `as_*` 方法都有返回引用的 mutable 重载。

#### 数组操作

```cpp
value& operator[](size_t index);
const value& operator[](size_t index) const;
const value& at(size_t index) const;
size_t array_size() const;
std::generator<const value&> array_elements() const;
```

#### 对象操作

```cpp
value& operator[](const std::string& key);
const value& operator[](const std::string& key) const;
const value& at(const std::string& key) const;
bool has_key(const std::string& key) const;
size_t object_size() const;
bool remove_member(const std::string& key);
std::generator<std::pair<const std::string&, const value&>> object_members() const;
```

#### 通用方法

```cpp
size_t size() const;       // 数组/对象大小、字符串长度，或 0
void clear();              // 重置为 null
type type() const;         // 基于索引的类型查询
```

### document（继承自 value）

继承 `value` 的所有构造函数和方法。额外添加：

```cpp
// 工厂方法
static document fromStream(std::istream& input, features feat = {});
static document fromString(const std::string& input, features feat = {});
```

### features

控制解析器接受哪些 YAML 特性。直接在 `parser` 实例上设置，或传递给 `fromStream`/`fromString`。

```cpp
struct features {
    bool comments   = true;   // # 行注释
    bool multi_line = true;   // | 和 > 标量块（尚未实现）
    bool anchors    = false;  // &anchor 和 *alias（尚未实现）
    bool tags       = false;  // !!type 标签（尚未实现）
    bool multi_doc  = false;  // --- / ... 文档分隔符（尚未实现）
};
```

### parser

内部流式解析器。用户应优先使用 `document::fromString()` / `document::fromStream()`。对于流式多文档输入，使用 `parseNext()`。

```cpp
class parser {
public:
    static document fromStream(std::istream& input, features feat = {});
    static document fromString(const std::string& input, features feat = {});
    bool parseNext(std::istream& input, document& out);

    features feat;
};
```

## 支持的 YAML 语法

### 标量

| 输入 | 类型 |
|------|------|
| `null`, `~` | null |
| `true`, `True`, `TRUE`, `yes`, `Yes`, `YES`, `on`, `On`, `ON` | bool (true) |
| `false`, `False`, `FALSE`, `no`, `No`, `NO`, `off`, `Off`, `OFF` | bool (false) |
| `42`, `-17`, `+3` | int |
| `3.14`, `-2.5`, `1e10` | double |
| `hello`, `"hello"`, `'hello'` | string |

### 块式映射

```yaml
name: Alice
age: 30
scores:
  - 95
  - 87
```

### 块式序列

```yaml
- Alice
- Bob
- Carol
```

### 紧凑语法

```yaml
people:
  - name: Alice
    age: 25
  - name: Bob
    age: 30
```

### 注释

```yaml
# 注释行
key: value  # 行内注释
```

### 双引号转义

| 转义 | 结果 |
|------|------|
| `\n` | 换行 |
| `\t` | 制表符 |
| `\\` | 反斜杠 |
| `\"` | 双引号 |
| `\a` `\b` `\f` `\r` `\v` `\e` | 控制字符 |
| `\xNN` | 十六进制字节 |
| `\uXXXX` | Unicode BMP |
| `\UXXXXXXXX` | Unicode 全范围 |

## YAML 值类型

| 类型 | 枚举值 | C++ 存储 |
|------|--------|----------|
| Null | `type::null` | `nullptr_t` |
| Boolean | `type::boolean` | `bool` |
| Integer | `type::integer` | `int64_t` |
| Floating | `type::floating` | `double` |
| String | `type::string` | `std::string` |
| Array | `type::array` | `std::vector<value>` |
| Object | `type::object` | `std::map<std::string, value>` |
| Alias | `type::alias` | `alias_ref` |
