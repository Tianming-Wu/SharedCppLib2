# toml - TOML 库

+ 名称: toml
+ 命名空间: `scl2`
+ 文档版本: `0.3.0`

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

`toml` 是[独立模块](standalone_module.md)，但依赖 [`orderedmap`](orderedmap.md)——表就是
`scl2::ordered_map`——所以把模块拷进自己的工程时，要连同 `orderedmap.hpp` 一起拷。

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
> 该模块处于早期开发阶段（v0.3.0）。已覆盖现实配置文件（包括 Minecraft 的 `META-INF/neoforge.mods.toml`）所需的 TOML 1.0 核心语法；规范的严格边界情形未完全强制。

## 保真

TOML 文件是人写出来的，程序写回去之后应该还是那个人的文件。因此 `fromString()` 与
`fromFile()` 默认保留原文：

```cpp
scl2::toml doc = scl2::toml::fromString(text);   // 默认就是 fidelity::raw
doc["database"]["port"] = 6432;                  // 改一个值
doc.toString();                                  // 只有那一行变，注释照样在
```

| 按原文保留 | 重新生成 |
| --- | --- |
+| 所有注释，行尾的和单独成行的 | 被赋值过的值 |
+| 成员的顺序 | 新增的成员 |
+| 空行与缩进 | 手工构造的值 |
+| 行尾（CRLF 还是 CRLF）、BOM、最后一行没有换行符 | `fidelity::semantic` 解析后的全部内容 |
+| 值的写法：`0x1F`、`1_000_000`、`'literal'`、`1.0`、`1979-05-27T07:32:00Z` | |
+| `[a.b]` / `[[a.b]]` 表头、内联表、点式键、跨行数组 | |

编辑时由它带来的规矩：

- **改过的值会把周围的一切留下**——键、值前面的空白、它那一行末尾的注释。
- **新成员落进它所在那一段的值区末尾**，也就是下一个 `[header]` 之前，这正是 TOML 要求的位置。
  新表自成一段 `[header]` 块。
- **往数组里追加元素会沿用这个数组的分隔风格**——单独一行的 `"b",` 仍然单独一行——而删掉第一个
  元素时会把它的逗号一并带走。

`fidelity::semantic` 是另一档：只保留值，成员顺序仍然是文件里的顺序，`toString()` 会重新生成
一份没有注释的标准文档。

```cpp
scl2::toml doc = scl2::toml::fromString(text, scl2::fidelity::semantic);
```

### 原文保存在哪里

每个成员都带着它周围那几段原文：它前面的空行与注释（`lead()`）、它当时写的键（`key_text()`）、
键与值之间的文本（`separator()`），以及它那一行剩下的部分（含行尾，`trail()`）。标量还带着它
的记号（`raw()`）；容器自己不存整段原文，因为它是从里面的成员写出来的——正因如此，改一个元素
不会在别处留下一份过期的整体拷贝。

`has_source()` 说明这个值是不是从文件里来的，`dirty()` 说明它的值后来是否被换过：被换过的值会
重新写，而它的注释保留。手工构造的值用 `set_raw("0x1F")`、`set_comment("why")`、
`set_lead("# section\n")` 来指定要写的原文。

文档本身没有 `dirty()`：想知道有没有变过，把读进来的原文留着，和 `toString()` 比一下。

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
| 删除 | `erase(key)` 删成员、`erase(index)` 删数组元素 |
| 原文（读） | `has_source()`、`dirty()`、`raw()`、`lead()`、`trail()`、`key_text()`、`separator()`、`tail()`、`header_text()`、`table_style()`、`is_inline_table()` |
| 原文（写） | `set_raw(text)`、`set_lead(text)`、`set_trail(text)`、`set_comment(text)`、`set_inline_table()` |
| 通用 | `type()` → `toml_value_type`、`size()`、`empty()`、`operator==` |
| 赋值 | `assign_to(T& dest)` — 把该值写入用户变量（具体类型或 `std::variant`） |

### `toml` —— 文档（永远是一张表）

```cpp
static toml fromString(const std::string& str, fidelity f = fidelity::raw);   // 解析
static toml fromFile(const std::filesystem::path& path, fidelity f = fidelity::raw);

std::string toString() const;                                        // 序列化
std::string toFile(const std::filesystem::path& path) const;         // 序列化并写文件

fidelity mode() const;                                               // 用哪一档解析的
const std::string& epilog() const;                                   // 最后一个成员之后的原文
void set_epilog(std::string text);

bool has_key(const std::string& key) const;                          // 是否存在该键
bool contains(const std::string& key) const;
toml_value&       operator[](const std::string& key);                // 不存在则创建
const toml_value& operator[](const std::string& key) const;          // 不存在则抛异常
size_t size() const;                                                 // 成员个数
bool empty() const;
bool erase(const std::string& key);                                  // 删除成员，注释一起走

toml_table&       table();                                           // 直接访问底层 map（用于遍历）
const toml_table& table() const;                                     // scl2::ordered_map：文档顺序
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
- **表就是 `scl2::ordered_map`**：`toml_table` 按文件里的顺序遍历，`find` / `at` / `count` 也都在；`operator[]` 遇到新键就追加在末尾。`toml_value` 仍然可以用 `std::map` 构造，`assign_to()` 也仍然能填满一个。
- **`toString()` 按文件里的顺序写成员**（手工构造的按插入顺序），不是按键顺序——往返需要的就是这个。想按键顺序遍历，用 `table().sorted_begin()`。
- **`[header]` 之后的键值属于那张表。** 这是 TOML 的语义，不是解析器的习惯：顶层键必须写在第一个表头之前。通过 API 添加成员时，它会落到该去的位置。
- **内联表的数组是值**：`points = [{ x = 1 }]` 留在它那一行，不会再被改写成 `[[points]]` 块。手工用表搭出来的数组仍然写成块。
- **通过 `as_xxx()` 可变重载直接改值**（例如 `doc["a"].as_int() = 5`）会改变值，但保留读进来时的写法；想让记号重新生成，请用赋值（`doc["a"] = 5`）。
- **键在必须加引号时会加**（`"a.b"`、`"a b"`），所以看起来像点式路径的键不会在写回时变成一个路径。

## 尚未实现

- 重排成员：`table()` 是 `ordered_map`，所以 `move_before()`、`insert_at()`、`sort_by()` 都在，
  只是 `toml` 自己没有对应的简写。
- 重排版：写出时保持文件的版式，不重新缩进或对齐。
- 严格性：能读但不常见的文件会被接受（见上文说明），所以并不是每份 TOML 1.0 会拒绝的文档
  这里都会拒绝。
