# json2 - 无损 JSON

+ Name: json2
+ Namespace: `scl2::json2`
+ Header: `<SharedCppLib2/json2.hpp>`
+ 文档版本: `0.1.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `json2` |

用法：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::json2)
```

```cpp
#include <SharedCppLib2/json2.hpp>
```

`json2` 除标准库外没有任何依赖，因此它是一个[独立模块](standalone_module.md)：把 `json2.hpp` 和
`json2.cpp` 复制进项目就能单独编译。

## 描述

`json` 会丢掉原文：解析成值树，再重新写出一份文档，缩进、键顺序、数字怎么写都由写出的一方决定，
而不是由写文件的人决定。`json2` 负责另一件事：**读一份文档、改一个值、再写回去，没动过的部分
逐字节保持原样。**

```cpp
#include <SharedCppLib2/json2.hpp>
using namespace scl2::json2;

document doc = document::parse(R"({
  "name": "demo",      // 排版会保留
  "count": 1.0,        // 写法也会保留：仍然是 1.0，不会变成 1
  "tags": [ "a", "b" ]
})");

doc.root()["count"] = value(2);
std::cout << doc.serialize();
```

```json
{
  "name": "demo",      // 排版没变
  "count": 2,          // 没被改动的值，写法也没变
  "tags": [ "a", "b" ]
}
```

上面 JSON 里的 `//` 只是为了说明问题——JSON 没有注释，`json2` 也不会凭空造出注释。

解析 → 编辑 → 序列化一圈之后：

| 原样保留 | 重新生成 |
| --- | --- |
| 词法单元之间的缩进、换行与空格 | 通过 API 赋过值的值 |
| 对象成员的顺序 | 新加进去的成员 |
| 数字的写法：`1.0`、`1e5`、`-0`、`0.5000` | 改过名的键 |
| 字符串的转义写法：`\u00e9` 还是 `\u00e9`，`\/` 还是 `\/` | 从零手搓的文档里的分隔符 |
| 重复键（都按原样保留，顺序不变） | `fidelity::semantic` 解析之后的一切 |
| 前置 BOM，以及根值前后的文本 | |

## 保真级别

`document::parse(text, fidelity)` 可选两档：

| | `fidelity::raw`（默认） | `fidelity::semantic` |
| --- | --- | --- |
| 原文 | 保留 | 丢弃 |
| 成员顺序 | 文档顺序 | 文档顺序 |
| 重复键 | 原样保留 | 保留第一个，其余丢掉（与 `json` 一致） |
| 数字写法 | 保留 | 由值重新生成 |
| `serialize()` | 除被改动的部分外逐字节还原 | 紧凑重新生成：`{"a":[1,2]}` |
| 内存 | 每个节点多一个字符串 | 只有值 |

当文档是"工作用的值"而不是"一份文件"时，用 `fidelity::semantic`：它更小，`serialize()` 给出的
是规整形式。

## 原文是怎么存的

每个数组元素、每个对象成员都自带"它前面的文本"（`lead`：逗号和空白），容器自己只存 `tail`，
也就是最后一个元素（或左括号）到右括号之间的文本。于是序列化就是

```
"[" + lead[0] + 元素[0] + lead[1] + 元素[1] + ... + tail + "]"
```

这就是为什么不需要重新排版：排版本身是数据，不是渲染时的选择。标量把原文词法单元保存在
`raw()` 里；**`raw()` 为空表示"没有原文"**，这时才由解码后的值生成。以下情况 raw 为空：以
`fidelity::semantic` 解析、通过 API 赋过值、或者值本来就是手搓的。

逗号归属于**它后面**的元素，这让编辑可预期：删掉一个元素，它的分隔符跟着一起走；只有第一个元素
例外——分隔符在第二个元素的 `lead` 里，需要剥掉。新增元素或成员时会沿用邻居的分隔风格，所以
排版好的文档插入之后仍然保持排版。

## 对象与查找

成员按文档顺序存在 vector 里，所以遍历对象是对连续内存的一次线性扫描，解析就是追加。按键查找
走一个**按需构建**的按 key 排序的索引（只在对象大到值得建索引时构建），因此规模上去之后仍是
O(log n)，并且**查找永远不会重排成员**。

超过多少个成员才建索引是一个编译期开关：

```cpp
#define SCL2_JSON2_INDEX_THRESHOLD 16    // 默认
#define SCL2_JSON2_INDEX_THRESHOLD 0     // 每个对象从第一个成员起就建索引
#define SCL2_JSON2_INDEX_THRESHOLD SIZE_MAX  // 完全不建索引，永远线性扫描
```

| 调用 | 含义 |
|---------|---------|
| `root()["key"]` | 取值；键不存在时追加一个 `null` |
| `root().find("key")` | 返回 `value*`，不存在则为 `nullptr` |
| `root().has_key("key")` | 是否存在 |
| `root().erase_key("key")` | 删掉第一个同键成员 |
| `root().as_object_body().members()` | 成员列表（文档顺序，用于遍历） |
| `root()[0]`、`root().size()` | 数组按下标访问 |
| `root().push_back(value("x"))` | 追加数组元素 |
| `root().erase(0)` | 删掉某个数组元素 |
| `root().at(0)` / `root().at("key")` | 带边界检查的元素 / 成员访问（越界或不在则抛 `std::out_of_range`） |
| `root().front()` / `root().back()` / `root().pop_back()` | 数组两端 |
| `root().length()` | 字符串长度 |
| `root().clear_as_array()` / `root().clear_as_object()` | 变成空数组 / 空对象 |
| `root().empty_as_array()` / `root().empty_as_object()` | 是不是那个类型且为空 |
| `root().items()` / `root().members()` | 元素 / 成员，各自带着它前面的原文 |

## 路径查询（JSON Pointer）

`at_path` / `find_path` / `contains_path` 接受 [JSON Pointer](https://datatracker.ietf.org/doc/html/rfc6901)，
与 `json` 用的是同一套语法：`/a/0/b`，键里的 `/` 写成 `~1`，键里的 `~` 写成 `~0`。空指针指向整份文档。

```cpp
document doc = document::parse(text);

if (doc.contains_path("/servers/0/port"))
    std::cout << doc.at_path("/servers/0/port").as_int();

value* port = doc.find_path("/servers/0/port");   // 指不到东西时为 nullptr
```

| 调用 | 含义 |
| --- | --- |
| `at_path(pointer)` | 取值；指不到东西时抛 `std::out_of_range` |
| `find_path(pointer)` | 返回 `value*`，指不到东西时为 `nullptr` |
| `contains_path(text)` | 相当于 `find_path` 能不能找到 |

指针可以解析一次反复使用，这也是它值得单独做个类型的原因：

```cpp
json2::pointer p("/servers/0/port");   // 只解析一次
doc.at_path(p).set_int(8080);
```

指不到东西的情况：路径中途撞上标量（`/a/0` 是数字时的 `/a/0/x`）、下标带前导零（`/a/01`）、
`-`（写入时表示"末尾之后"的记号），以及下标越界。这些都不抛异常；只有 `at_path` 会抛，而且只在
确实什么都没找到时抛。不以 `/` 开头的指针在**构造**时就被拒绕（`std::invalid_argument`），不是在应用时。

## 再造输出与比较

`serialize()` 是保真的那个。`to_string()` 是重新生成的：忽略原文与排版，只写值本身，由
`exporter` 决定排版。**默认排版与 `scl2::json` 的默认写法一致** —— 一个成员/元素一行、四空格
缩进、键后 `": "`、空容器写在一行。`_apicheck/jsoncrossprobe.cpp` 把同一份文档喂给两个模块，
逐字节比较输出，所以两者不会各自漂走。

```cpp
document ugly = document::parse("{ \"x\" : 1.50, \"y\":[1e5, 2] }");

ugly.serialize();          // { "x" : 1.50, "y":[1e5, 2] }   原样
ugly.to_string();          // 重新排版，四空格缩进
ugly.to_compact_string();  // {"x":1.5,"y":[1e+05,2]}       紧凑
```

| `exporter` | |
| --- | --- |
| `indentStyle` | `none` / `space2` / `space4`（默认）/ `tab` |
| `isCompact` | 完全没有换行，冒号后也不留空格 |
| `isInline` | 换行变成单个空格，所有东西挤在一行 |
| `escapeNonAscii` | 0x7F 以上的字节写成 `\uXXXX`，U+FFFF 以上是代理对；非法 UTF-8 变 `U+FFFD` |
| `exporter::compact_exporter()` / `inline_exporter()` | 最常用的两种设置，现成 |

浮点数用能读回同一个值的最短形式写出，所以 `1.50` 变 `1.5`、`1e5` 变 `1e+05`：两者都是合法
JSON，也确实是同一个值。整数形式的 double 会保留小数部分（`1.0`），所以读回来仍是 double，
不会惄惄变成整型。`json` 从 1.12.1 起写法相同（`_apicheck/jsoncrossprobe.cpp` 连浮点输出也
逐字节对比两个模块）。

值之间可以用 `==` 比较。数组逐元素比；对象**按键比**，所以成员顺序不影响结果；写法也不影响，
因为比的只是值本身。`1` 和 `1.0` 是不同类型，因此不相等。`assign_to(T&)` 把一个标量写进 `bool`、
整型、浮点或 `std::string`，不做任何解析，因此字符串值不会惄惄变成数字。

## 错误

`parse` 抛 `json2::parsing_error`，消息里带偏移量：

```
json2 parsing error: expected ',' or '}' (at offset 42)
```

不可打印的字节会以十六进制显示（`unexpected byte '\xEF'`），因为这几乎总是编码问题——文档中间
冒出一个 BOM，或者拿到的是 UTF-16 文本。

解析器是递归的，所以嵌套上限设为 256 层。写出非有限的数字会抛异常：JSON 没有 inf 也没有 NaN。

## 尚未实现

- “边走边造”不存在的路径：`at_path` 只读，指不到就抛；要造路径就用 `operator[]` 逐层下去
  （`doc.root()["a"]["b"] = value(1)`）
- 排序、合并这类文档库后来往往会长的算法

## 刻意不属于这个模块的部分

- **SharedCppLib2 集成**（`gdump` / `gload`、`bytearray`、`datauri` 扩展）。`json2` 是独立模块，
  也会一直是：库内部的集成放在一个中间层里，依赖方向是“中间层 → json2”，永远不会反过来。
- **文档级的 `dump()` / `load()`**。文档内部需要的形式，`to_compact_string()` 已经给了，
  `document::parse` 也能读回来，再加一套二进制编码只是多一个编码器、不多一个能力。
  真要紧凑的二叉化，用 `jbt` —— 它就是为此存在的。

## 与 `json` 的差异

- `json` 的对象是 `std::map`，所以输出按键排序；`json2` 保持文档顺序。两者互不归一化对方。
- `json` 有 `SCL2_JSON_ENABLE_EXTENSIONS` 后面的 `bytearray` / `data_uri` 扩展；`json2` 目前完全
  没有扩展。
- 数组和对象用 `value::push_back` / `operator[]`，不是 `json_value` 的那套方法；值也没有
  `json` / `json_value` 那样的两层划分。

## 另见

- [json](json.md) —— 值树模块，纯数据处理仍然应该用它
- [xml2](xml2.md) —— 同样的保真思路用在 XML 上，`fidelity::raw` / `fidelity::semantic` 也是那两档
- [standalone_module.md](standalone_module.md) —— 这里的"独立模块"是什么意思
