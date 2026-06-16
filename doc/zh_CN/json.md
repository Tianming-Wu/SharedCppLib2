# json - JSON 库

+ 名称: json
+ 命名空间: `scl2`
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `json` |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::json)
```

```cpp
#include <SharedCppLib2/json.hpp>
```

## 描述

`json` 为 SharedCppLib2 提供了 JSON 解析、操作和序列化功能。它支持标准 JSON 类型（null、布尔、整数、浮点数、字符串、数组、对象），并提供了熟悉的 map/array 访问模式。

定义 `SCL2_JSON_ENABLE_EXTENSIONS` 后，还可以额外支持 `bytearray` 和 `inline_data_uri` 类型，并在解析过程中自动解码 base64 和数据 URI 字符串。在通过 SharedCppLib2 构建时此扩展**默认启用**（CMake 选项 `SCL2_JSON_ENABLE_EXTENSIONS` 默认为 `ON`，并通过 `PUBLIC` 编译定义传播给使用者）。单独使用模块时需要手动定义。

> [!WARNING]
> 该模块处于早期开发阶段。虽然可用，但可能缺少一些像 jsoncpp 这样的成熟库中的高级功能。不过它更加轻量化，并且可以脱离 SharedCppLib2 单独使用——只需复制 `json.hpp` 和 `json.cpp` 即可。

## 快速开始

### 解析与访问

```cpp
#include <SharedCppLib2/json.hpp>

scl2::json j = scl2::json::fromString(R"({
    "name": "test",
    "count": 42,
    "ratio": 3.14,
    "active": true,
    "tags": ["dev", "test"],
    "meta": {
        "author": "me"
    }
})");

std::string name  = j["name"].as_string();       // "test"
int64_t    count  = j["count"].as_int();           // 42
double     ratio  = j["ratio"].as_double();        // 3.14
bool       active = j["active"].as_bool();         // true

// 数组
for (const auto& tag : j["tags"].as_array()) {
    std::cout << tag.as_string() << std::endl;
}

// 嵌套对象
std::string author = j["meta"]["author"].as_string(); // "me"
```

### UTF-8 与中文字符

JSON 是一种 UTF-8 格式（RFC 8259）。本库以原始字节存储字符串——输入什么就存什么。为确保中文跨平台正确处理，请保证输入为 UTF-8：

- **JSON 文件**：保存为 UTF-8 编码（所有 JSON 文件的推荐做法）。
- **MSVC 源码**：添加 `/utf-8` 编译选项，使 `std::string("你好")` 产生 UTF-8 字节。
- **备用方案**：使用 `json_u8()` 辅助函数配合 `u8` 字面量：

```cpp
// MSVC 不加 /utf-8 时："你好" 是 GBK 字节 → 错误
// 加了 /utf-8 时：      "你好" 是 UTF-8 字节 → 正确
// 不加 /utf-8 时用此辅助函数：
j["name"] = scl2::json_value(scl2::json_u8(u8"你好"));
```

从 UTF-8 JSON 文件读取时，中文以 UTF-8 存储，导出时也是 UTF-8——无需额外处理。

### 构建与导出

`json` 类提供了便捷的内置导出方法：

```cpp
scl2::json j;
j["title"]   = scl2::json_value(std::string("Config"));
j["version"] = scl2::json_value(static_cast<int64_t>(2));
j["debug"]   = scl2::json_value(true);

std::string formatted = j.toString();        // 格式化输出（带缩进）
std::string compact   = j.toCompatString();  // 紧凑输出（无多余空白）

// 文件 I/O
scl2::json cfg = scl2::json::fromFile("config.json");
cfg.toFile("config_updated.json");
```

如果需要自定义格式（例如制表符缩进、紧凑导出、内联模式等），请直接使用 [`json_exporter`](#json_exporter)：

```cpp
scl2::json_exporter exporter;
// ... 配置 exporter ...
std::string output = exporter.exportToString(j);
```

### 导出格式示例

同一 JSON 结构使用不同导出器配置的效果：

**原始结构：**
```json
{"debug":false,"person":{"name":"Bob","scores":[95,87]}}
```

**默认（`space4`）：**
```json
{
    "debug": false,
    "person": {
        "name": "Bob",
        "scores": [
            95,
            87
        ]
    }
}
```

**紧凑（`isCompat = true`）：**
```json
{"debug":false,"person":{"name":"Bob","scores":[95,87]}}
```

**制表符缩进（`indent_style::tab`）：**
```json
{
	"debug": false,
	"person": {
		"name": "Bob",
		"scores": [
			95,
			87
		]
	}
}
```

**内联模式（`isInline = true`）：**
```json
{ "debug": false, "person": { "name": "Bob", "scores": [ 95, 87 ] } }
```

**两空格缩进（`indent_style::space2`）：**
```json
{
  "debug": false,
  "person": {
    "name": "Bob",
    "scores": [
      95,
      87
    ]
  }
}
```

> [!TIP]
> 紧凑格式可直接使用 `json::toCompatString()`。其他格式请配置 `json_exporter` 实例后调用 `exportToString()`。

## 核心 API

### json_value

底层值类型。存储以下之一：`nullptr`、`bool`、`int64_t`、`double`、`std::string`、`std::vector<json_value>`、`std::map<std::string, json_value>`。

#### 类型检查

```cpp
bool is_null() const;
bool is_bool() const;
bool is_int() const;
bool is_double() const;
bool is_string() const;
bool is_array() const;
bool is_object() const;
```

#### 类型访问（const 和 mutable）

```cpp
nullptr_t as_null() const;
bool as_bool() const;
int64_t as_int() const;
double as_double() const;
const std::string& as_string() const;
const std::vector<json_value>& as_array() const;
const std::map<std::string, json_value>& as_object() const;
```

每个 `as_*` 方法都有返回引用的 mutable 重载。

#### 数组操作

```cpp
json_value& operator[](size_t index);
const json_value& operator[](size_t index) const;
const json_value& at(size_t index) const;
size_t array_size() const;
std::generator<const json_value&> array_elements() const;  // 支持 range-for
```

#### 对象操作

```cpp
json_value& operator[](const std::string& key);
const json_value& operator[](const std::string& key) const;
const json_value& at(const std::string& key) const;
bool has_key(const std::string& key) const;
size_t object_size() const;
std::generator<std::pair<const std::string&, const json_value&>> object_members() const;
```

#### 通用方法

```cpp
size_t size() const;       // 数组/对象大小、字符串长度，或 0
void clear();              // 重置为 null
json_value_type type() const;
```

### json（继承自 json_value）

继承 `json_value` 的所有构造函数和方法。额外添加：

```cpp
// 工厂方法
static json fromString(const std::string& str);
static json fromFile(const std::string& filename);

// 导出
std::string toString() const;          // 格式化输出
std::string toCompatString() const;    // 紧凑输出（无多余空白）
std::string toFile(const std::string& filename) const;
```

### json_exporter

控制 `json::toString()` 的输出格式。所有公开成员均可由用户直接设置。

```cpp
enum class indent_style { none, space2, space4, tab };

// 工厂辅助方法
static json_exporter compact_exporter();   // isCompat + escapeNonAscii + none indent
static json_exporter inline_exporter();    // isInline + none indent

// 公开字段 — 在调用 exportToString() 前直接设置即可
bool isCompat;             // 紧凑输出（无多余空白）
bool isInline;             // 内联格式（换行替换为空格）
bool escapeNonAscii;       // 将非 ASCII 的 UTF-8 转义为 \uXXXX（最大化可移植性）
indent_style indentStyle;  // 缩进风格（默认：space4）

std::string exportToString(const json& j);
std::string exportToCompatString(const json& j);
```

> [!NOTE]
> `escapeNonAscii` 会将 UTF-8 码点解码并输出为 `\uXXXX` 转义序列，使输出在无法处理原始 UTF-8 的环境中也能安全使用。`compact_exporter()` 预设默认启用此选项。仅在本地使用时，可关闭它以保持中文等非 ASCII 字符的人类可读性。

### json_parser

内部解析器类。用户应使用 `json::fromString()` / `json::fromFile()` 代替。

## 扩展功能（`SCL2_JSON_ENABLE_EXTENSIONS`）

在通过 SharedCppLib2 构建时此扩展**默认启用**——CMake 选项 `SCL2_JSON_ENABLE_EXTENSIONS` 默认为 `ON`，并在 `json` 目标上设置为 `PUBLIC` 编译定义，因此链接到 `SharedCppLib2::json` 的目标会自动获得此定义。

如果单独使用该模块（仅复制 `json.hpp` 和 `json.cpp`），则需要在包含头文件之前手动定义：

```cpp
#define SCL2_JSON_ENABLE_EXTENSIONS
#include <SharedCppLib2/json.hpp>
```

启用后可使用以下额外功能：

- **`bytearray` 类型** — 直接存储二进制数据；序列化为 `"base64:..."` 字符串；以 `base64:` 开头的字符串会在解析时自动解码。
- **`inline_data_uri` 类型** — 存储数据 URI 值；序列化为 URI 字符串；以 `data:` 开头的字符串会在解析时自动解析。
- **`\xNN` 转义序列** — JSON 字符串中的十六进制转义识别。

> [!NOTE]
> 这些扩展产生的是标准 JSON 输出（base64 字符串和数据 URI 字符串），因此与任何标准 JSON 解析器保持互操作。

## JSON 值类型

| 类型 | 枚举值 | C++ 存储 |
|------|--------|---------|
| Null | `json_value_type::null` | `nullptr_t` |
| 布尔 | `json_value_type::boolean` | `bool` |
| 整数 | `json_value_type::integer` | `int64_t` |
| 浮点数 | `json_value_type::floating` | `double` |
| 字符串 | `json_value_type::string` | `std::string` |
| 数组 | `json_value_type::array` | `std::vector<json_value>` |
| 对象 | `json_value_type::object` | `std::map<std::string, json_value>` |
| Bytearray* | `json_value_type::bytearray` | `std::bytearray` |
| 数据 URI* | `json_value_type::data_uri` | `inline_data_uri` |

\* 仅在启用 `SCL2_JSON_ENABLE_EXTENSIONS` 时可用。
