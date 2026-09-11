# xml2 - 保真 XML 库

+ 名称: xml2
+ 命名空间: `scl2::xml2`
+ 文档版本: `0.1.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `xml2` |

> [!NOTE]
> `xml2` 已注册为 CMakeLists 中的 `xml2` 目标（它正在替换旧的 `xml` 模块，旧模块目前未改动）。
> 使用 `add_subdirectory` 或已安装的包时直接链接该目标即可；
> 也可以按下面「独立导入」一节复制源码单独使用。

包含方式：

```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::xml2)
```

```cpp
#include <SharedCppLib2/xml2.hpp>
```

## 描述

`xml2` 提供**可保真（lossless）**的 XML 解析与序列化：读取文件、改动其中一处、
再写回时，**文件其余部分不会被重新排版**——注释、空行、缩进、属性顺序、引号风格、
实体拼写全部原样保留。

它的目标是替换旧的 `xml` 模块（旧模块目前仍在，两者并存于 `xml.hpp` / `xml2.hpp`）。

+ 头文件：`xml2.hpp`；源文件：`xml2.cpp`
+ 独立模块（`[SCL_STANDALONE_MODULE]`），只依赖标准库，兼容 C++17 及以上
+ 命名空间：`scl2::xml2`

> [!WARNING]
> 模块处于早期开发阶段。保真读写（读取、改属性值/文本、追加子节点、写回）已经可用并有测试覆盖；
> 插入到中间 / 删除节点的重新排版尚未实现。

### 为什么不能用 map 式的树

旧的 `xml` 模块（以及多数朴素 XML 库）把属性存进 `std::map`、把文本存成单个
`optional<string>`、输出时统一重新缩进。这样的结构从根本上无法保真：

| 存储方式 | 丢失的内容 |
|---|---|
| `std::map<name, value>` | 属性**顺序**、属性周围的空白 |
| 每个元素一个文本串 | **混合内容**（`<p>a<b>c</b>d</p>`）无法表示 |
| 只保存解码后的文本 | `&amp;` / `&#38;` / CDATA 的原始拼写无法还原 |
| 输出时重新缩进 | 注释位置、空行、用户自己的折行全部丢失 |

`xml2` 的做法是：把子节点保存为**源文本的完整有序分解**——元素、文本（包括纯空白）、
注释、处理指令全都是子节点，按文档顺序排列。这样「序列化」就等价于「递归拼接原文」，
保真是结构上自然成立的，而不是额外机制。

### 独立导入

可以直接把 `xml2.hpp` 和 `xml2.cpp` 复制到你的项目里，无需安装完整的 SharedCppLib2
（它们只依赖标准库）：

```cmake
add_executable(app main.cpp xml2.cpp)
```

也可以打包成库：

```cmake
add_library(sclxml2 xml2.hpp xml2.cpp)
target_link_libraries(yourtarget sclxml2)
```

## 快速开始

```cpp
#include <xml2.hpp>

namespace x = scl2::xml2;

const std::string text = R"(<library>
    <book id="1"   kind='paper'>
        <title>Modern C++</title>
    </book>
</library>)";

x::document doc = x::document::parse(text);

// 读取
doc.get("library/book[1]/@id");      // "1"
doc.get("library/book[1]/title");    // "Modern C++"

// 修改（只影响这一处）
doc.set("library/book[1]/@id", "2");

// 写回
std::string out = doc.serialize();   // 除 id="2" 外，与输入逐字节相同
```

关键就是最后一行：除了被改动的那个属性，其余内容与原文件完全相同。

## 读取

### `document`

| 成员 | 说明 |
|---|---|
| `static document parse(std::string text, fidelity f = fidelity::raw)` | 解析 |
| `serialize()` | 生成文本（未修改部分逐字节原样） |
| `root()` | 根元素 `node` |
| `prolog()` / `epilog()` | 根元素之前 / 之后的原文（XML 声明、DOCTYPE、注释、尾随空白） |
| `select` / `find` / `get` / `set` | 路径查询（见下） |

### `node`

| 成员 | 说明 |
|---|---|
| `is_element()` `is_text()` `is_comment()` `is_pi()` | 节点类别 |
| `name()` | 元素名（含前缀，`x:item` → `"x:item"`） |
| `value()` | 文本：文本节点返回自身文本；元素返回**第一个**直接文本子节点 |
| `attr(key, def)` / `has_attr(key)` | 属性值（已解码） |
| `attributes()` | `const std::vector<attribute>*`，**保持源顺序**，每项含 `name`、`value`、`raw_value` |
| `children()` | `std::vector<node>*`，按文档顺序的全部子节点 |
| `child(name)` / `child_at(name, i)` | 第一个 / 第 i 个（0-based）同名子元素 |
| `select(path)` / `find(path)` | 相对本节点的路径查询 |

> [!TIP]
> `value()` 只取**第一个**直接文本子节点。处理混合内容时请遍历 `children()`，
> 自行判断 `is_text()` / `is_element()`。

## 路径查询

路径是 XPath 的一个实用子集——覆盖配置文件和文档处理的常见需求，语法比完整 XPath 简单得多。

### 语法

| 写法 | 含义 |
|---|---|
| `a/b` | 子元素链 |
| `//a` | 任意深度的后代 |
| `a//b` | `a` 之下任意深度的 `b` |
| `*` | 任意元素名 |
| `a[2]` | 第 **2** 个匹配元素（**1 开始**） |
| `a[@id]` | 含有 `id` 属性 |
| `a[@id=v]` | `id` 属性等于 `v` |
| `a[@id*=v]` | `id` 属性**包含** `v`（模糊匹配） |
| `a/@id` | 取属性值，而不是元素本身 |
| `a:b` | 前缀按**字面**匹配（不做命名空间 URI 解析） |

### 规则

+ 第一步既可以写根元素名（`library/book`），也可以直接从子元素开始（`book`），两种都可用。
+ `//` 必须紧接在元素名之前。
+ `@attr` 只能出现在路径末尾。它产生一个「属性匹配」：`value()` 返回属性值，
  `set_value()` 写回属性。
+ `[n]` 从 1 开始，且**在每个父节点内分别计数**（XPath 语义）：
  `library/book[1]` 会返回**每个** `library` 下的第一个 `book`。
+ 过滤器与下标可以连用：`book[@id][2]`、`book[@id*=a][1]`。
+ 结果按文档顺序返回。
+ `select()` 返回**全部**匹配；`find()`、`get()`、`set()` 只使用**第一个**。

### 示例

以下述文档为例：

```xml
<library>
    <book id="1" kind="paper"><title>Modern C++</title></book>
    <book id="2"><title>Compressed</title></book>
    <empty/>
</library>
```

```cpp
doc.select("library/book").size();          // 2
doc.select("library/book[2]").size();       // 1
doc.get("library/book[1]/@id");             // "1"
doc.get("library/book[@id=2]/title");       // "Compressed"
doc.get("library/book[@kind=paper]/@id");   // "1"
doc.get("library/book[@kind*=pap]/@id");    // "1"   （包含匹配）
doc.get("//title[1]");                      // "Modern C++"  （任意深度）
doc.select("library/*").size();             // 2     （* 只匹配元素，跳过文本与注释）
doc.select("library/book[@id]").size();     // 2
doc.get("library/nope", "fallback");        // "fallback"（无匹配时返回默认值）
```

遍历全部匹配：

```cpp
for (const auto& m : doc.select("library/book")) {
    std::string id = m.target->attr("id");
}
```

### 通过路径写入

```cpp
doc.set("library/book[1]/@id", "9");   // 只重写这一个属性值
doc.set("library/book[1]/title", "New");
```

`set()` 在没有任何匹配时返回 `false`。
若目标元素没有直接文本子节点，会新建一个文本子节点。

### 结构编辑

```cpp
x::document d = x::document::parse("<root>\n    <a/>\n    <c/>\n</root>");

d.find("root/c").insert_before(make_element("b"));   // 在 <c/> 前插入 <b/>
d.find("root/a").insert_after(make_element("b"));    // 在 <a/> 后插入
d.find("root/c").remove();                           // 删除 <c/> 及其缩进
d.find("root/@id").remove();                         // 删除该属性
```

+ `insert_before` / `insert_after` / `remove` 需要 match 来自 `select()` / `find()`
  （依赖其中记录的父节点）。根元素没有父节点也没有兄弟，这些调用会返回 `false`；
  但根元素上的 `@attr` 仍然可以删除。
+ 缩进是**局部推断**的，绝不会重新排版整个文档：

  | 父元素布局 | 结果 |
  |---|---|
  | 多行（`<root>\n    <a/>\n</root>`） | 新元素沿用兄弟的缩进 |
  | 紧凑（`<root><a/></root>`） | 保持紧凑，不会被展开 |
  | 混合内容（`<p>a<b/>c</p>`） | 不凭空插入空白 |

+ 删除元素时会连同它的**前导空白**一起删掉，不会留下空行；若删完后父元素不再有元素子节点，遗留的空白也会被清理。
+ `append_child` 等同于“在末尾插入”。

## 保真级别

`parse()` 接受一个保真级别：

| 级别 | 保留内容 | 适用场景 |
|---|---|---|
| `fidelity::raw`（默认） | 每个标签、属性值、文本节点的原文，以及 prolog/epilog | 编辑文件后写回，其余部分逐字节不动 |
| `fidelity::semantic` | 只有值与结构：不保留原文，丢弃纯空白布局，prolog 只留 XML 声明，epilog 丢弃 | 从大文件中读取数据，或从零生成输出 |

`semantic` 仍然保留属性**顺序**、注释、处理指令与全部值——只是丢掉了排版。
`serialize()` 会重新生成紧凑输出，而不是照抄原文：

```cpp
// 输入: <?xml version="1.0"?>\n<root  a="1"  b='2'>\n    <c>  hi  </c>\n</root>\n
x::document big = x::document::parse(text, x::fidelity::semantic);
big.get("root/@b");                    // "2"
big.serialize();                       // <?xml version="1.0"?><root a="1" b="2"><c>  hi  </c></root>
```

也就是说，用**内存与排版**换保真度；只要数据的场景选 `semantic`。

## 修改会重新生成什么

因为子节点是源文本的完整分解，未被触碰的部分在序列化时都是照抄原文：

| 操作 | 输出影响 |
|---|---|
| （不做修改） | 与输入**逐字节相同** |
| `set_attr` 修改已有属性 | 只重写该属性值；同标签内其他属性的空白、顺序、引号风格不变 |
| `set_attr` 新增属性 | 追加到末尾，沿用原有的缝隙空白 |
| `set_text` | 只重写该文本节点（写入时转义实体） |
| `append_child` / `insert_child` | 多行父元素会沿用兄弟的缩进与结束标签前空白；紧凑单行父元素保持紧凑 |
| `remove_child` / `remove_attr` | 连同自身的前导空白一起删除，其余部分不动 |

元素**只有在被标记为脏**（属性发生变化）时才会重新生成开始标签；
它的子节点与结束标签仍然逐字节照抄。

## 限制与路线图

+ 缩进推断是**局部**的：新节点跟随兄弟风格，但已有节点绝不会被重新排版（没有 pretty-print）。
+ 谓词不支持逻辑运算（`and` / `or`），不支持 `text()`、`last()`。
+ 不做命名空间 URI 解析，前缀按字面匹配。
+ 不处理 DTD / 实体定义，未知实体会原样保留。

## 与旧 `xml` 模块的关系

| | `xml`（旧） | `xml2`（新） |
|---|---|---|
| 属性顺序 | 丢失（`std::map`） | 保留 |
| 混合内容 | 不支持 | 支持 |
| 输出 | 统一重新缩进 | 保真（未改动处原样） |
| 路径查询 | 无 | 支持（含通配与谓词） |
| 文本原文 | 解码后丢失拼写 | 保留 `raw` + 解码 `value` |

`xml2` 稳定后会替换 `xml`；旧模块目前未做改动。
