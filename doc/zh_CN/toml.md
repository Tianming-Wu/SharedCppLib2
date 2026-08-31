# toml - TOML 库

+ 名称: toml
+ 命名空间: `scl2`
+ 文档版本: `0.1.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `toml` |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::toml)
```

```cpp
#include <SharedCppLib2/toml.hpp>
```

## 描述

`toml` 是 SharedCppLib2 的 TOML（Tom's Obvious, Minimal Language）解析与序列化模块。一个 TOML 文档本质上就是**一张键值对表**，它天然对应 `scl2::toml` 对象，表中的值则是 `scl2::toml_value`。

支持的值类型：`string`、`integer`、`floating`、`boolean`、`date-time`、`array`、`table` —— TOML **没有 `null` 类型**。

支持的语法：
- `key = value` 键值对以及**点式键**（`a.b.c = 1`）
- `[table]` 表头以及 **`[[array-of-tables]]`** 数组表头
- 数组 `[1, 2, 3]` 以及**内联表** `{ a = 1, b = "x" }`（兼容多行内联表）
- basic（`"..."`、转义、`"""` 多行）与 literal（`'...'`、`'''` 多行）字符串
- 整数（十进制 / 十六进制 / 八进制 / 二进制，允许下划线）、浮点数（含 `inf` / `nan`）、布尔值
- 注释（`#` 到行尾）
- 日期时间按原样保留（例如 `2024-01-01T12:00:00Z`）

> [!WARNING]
> 该模块处于早期开发阶段（v0.1.0）。已覆盖现实配置文件（包括 Minecraft 的 `META-INF/neoforge.mods.toml`）所需的 TOML 1.0 核心语法；规范的严格边界情形未完全强制。

## 快速开始

### 解析与访问

```cpp
#include <SharedCppLib2/toml.hpp>

auto doc = scl2::toml::fromString(R"(
title = "My App"
version = 1.5
enabled = true
released = 2024-01-01T12:00:00Z

[database]
host = "localhost"
port = 5432

[[servers]]
name = "alpha"
[[servers]]
name = "beta"
)");

std::string title    = doc["title"].as_string();              // "My App"
double      version  = doc["version"].as_double();            // 1.5
bool        enabled  = doc["enabled"].as_bool();              // true
std::string released = doc["released"].as_datetime();         // "2024-01-01T12:00:00Z"
std::string host     = doc["database"]["host"].as_string();   // "localhost"
size_t      n        = doc["servers"].array_size();           // 2
std::string first    = doc["servers"][0]["name"].as_string(); // "alpha"
```

### 从文件解析

```cpp
auto doc = scl2::toml::fromFile("config.toml");   // UTF-8
```

### 序列化回写

```cpp
std::string text = doc.toString();   // 序列化为 TOML 文本
doc.toFile("copy.toml");             // 写入文件
```

### 读取 Minecraft `META-INF/neoforge.mods.toml`

典型的 mod 文件包含顶层键、一个 `[[mods]]` 数组表，以及 `[[dependencies.<modid>]]`：

```cpp
auto doc = scl2::toml::fromFile("META-INF/neoforge.mods.toml");

std::string modLoader = doc["modLoader"].as_string();   // "javafml"

// [[mods]] 永远是数组，即使只有一项：
for (const auto& m : doc["mods"].as_array()) {
    std::string id   = m["modId"].as_string();          // "hostilenetworks"
    std::string ver  = m["version"].as_string();        // "6.5.1"  <- 是字符串，不是数字
    std::string desc = m["description"].as_string();    // '''...''' 多行字符串
}

// 多个 [[dependencies.<modid>]] 块成为数组元素：
for (const auto& dep : doc["dependencies"]["hostilenetworks"].as_array()) {
    std::string modId = dep["modId"].as_string();       // "minecraft" / "neoforge" / ...
    std::string type  = dep["type"].as_string();        // "required"
    std::string range = dep["versionRange"].as_string(); // "[1.21.1,)"
}
```

## 核心 API

### `toml_value` —— 表或数组中的单个值

| 类别 | 成员 |
|---|---|
| 类型判断 | `is_bool()`、`is_int()`、`is_double()`、`is_string()`、`is_datetime()`、`is_array()`、`is_table()` |
| 只读访问 | `as_bool()`、`as_int()`、`as_double()`、`as_string()`、`as_datetime()`、`as_array()`、`as_table()` |
| 写访问 | 对应的可变 `as_xxx()` 重载 |
| 数组 | `operator[](size_t)`、`array_size()`、`push_back()`、`empty_as_array()` |
| 表 | `operator[](const std::string&)`、`at(key)`、`has_key()`、`contains()`、`table_size()` |
| 通用 | `type()` → `toml_value_type`、`size()`、`empty()`、`operator==` |

### `toml` —— 文档（永远是一张表）

```cpp
static toml fromString(const std::string& str);                      // 解析
static toml fromFile(const std::filesystem::path& path);             // 解析文件

std::string toString() const;                                        // 序列化
std::string toFile(const std::filesystem::path& path) const;         // 序列化并写文件

bool has_key(const std::string& key) const;                          // 是否存在该键
bool contains(const std::string& key) const;
toml_value&       operator[](const std::string& key);                // 不存在则创建
const toml_value& operator[](const std::string& key) const;          // 不存在则抛异常
size_t size() const;                                                 // 成员个数
bool empty() const;

toml_table&       table();                                           // 直接访问底层 map（用于遍历）
const toml_table& table() const;
```

### 值类型一览

| `toml_value_type` | TOML 示例 | C++ 访问 |
|---|---|---|
| `string` | `"hello"`、`'x'`、`"""多行"""` | `as_string()` |
| `integer` | `42`、`0x2A`、`0o52`、`0b101010` | `as_int()` |
| `floating` | `3.14`、`1e10`、`inf`、`nan` | `as_double()` |
| `boolean` | `true` / `false` | `as_bool()` |
| `datetime` | `2024-01-01T12:00:00Z` | `as_datetime()`（原始文本） |
| `array` | `[1, 2, 3]` | `as_array()` |
| `table` | `[a]` 表头、`{ a = 1 }` 内联表 | `as_table()` |

## 注意事项

- **没有 `null` 类型**：TOML 没有 null；默认构造的 `toml_value` 是空字符串。
- **`[[...]]` 永远是数组**：即使只出现一次，也要用 `[0]` 访问。
- **兼容多行内联表**（例如 `mods = [ { ... }, { ... } ]` 跨多行）——TOML 规范不允许，但现实文件（如 `neoforge.mods.toml`）会这么写。
- **点式访问**：对嵌套表 / 数组使用 `operator[]` 链式访问时，自动处理数组表。
- **带引号的数字是字符串**：mod 文件里 `version = "6.5.1"` 是*字符串*——用 `as_string()`，不是 `as_int()`。
