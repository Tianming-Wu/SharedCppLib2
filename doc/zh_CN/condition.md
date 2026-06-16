# Condition - 条件表达式库

+ 名称: condition_node, condition_expression
+ 命名空间: `scl2`
+ 文档版本: `1.1.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `condition` |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::condition)
```

## 描述

Condition 模块提供了一种定义、解析、存储和求值复杂条件结构的方式。它允许你基于一组布尔运算符计算结果。你可以使用函数甚至运行时常量来输入源值。


## 快速开始

### 基本用法

```cpp
#include <SharedCppLib2/condition.hpp>

// 从字符串创建
// 有关格式，请参考下面的格式章节。

scl2::condition_expression expr("A & (B | C)");

expr.setFunction("A", []() { return true; });
expr.setFunction("B", []() { return false; });
expr.setFunction("C", []() { return true; });

bool result = expr.evaluate(); // 在此例中结果为 true。
```

你也可以使用工厂方法来创建条件表达式，但这需要你手动构建树。不建议这样做。

如果你真的想尝试，请参考 `condition.cpp` 中的 `ConditionParser` 类。

这里不会提供相关示例。


### 存储

可以将条件表达式存储为静态字节数组，然后对它做任何你想做的事，比如保存到文件、通过网络发送等。

因此，也可以在编译前预构建条件表达式，然后在运行时加载。这样可以省去解析字符串版本的时间。

```cpp
#include <SharedCppLib2/condition.hpp>

condition_expression expr("A & (B | C)");

// 自动检测功能正在开发中，但目前你需要手动调用保留的 API。
auto ba = scl2::condition_expression::save(expr);

std::ofstream ofs("condition.bin", std::ios::binary);

ofs << ba; // 经过之前的更新，现在可以安全地使用。
// ba.writeAllToStream(ofs); // 或者旧的方式。

ofs.close();

// 加载

auto expr2 = scl2::condition_expression::load(ba);

// 注意：你仍然需要先打开文件，然后读取或写入字节到 bytearray。
// 相关 API 正在开发中，未来可以用一行代码完成：
scl2::writeFile("condition.bin", expr);

// 以及
auto expr = scl2::readAndLoad<condition_expression>("condition.bin");

// 虽然可能性很小，但你也可以加密你的二进制数据。
std::bytearray enba = scl2::encrypt<scl2::aes128>(ba, key, iv);

```

## 格式

条件系统的格式很简单，但不同于布尔代数中广泛使用的格式。

不过，本库也支持那种格式。请查看下面的文档。

下面是条件表达式字符串的默认格式。

其他支持的格式请参见[支持的格式](#支持的格式)章节。

可用运算符：
- `&` 逻辑与（AND）
- `|` 逻辑或（OR）
- `^` 异或（XOR）
- `!` 逻辑非（NOT）
- `(` 和 `)` 分组
- `!&` 与非（NAND）
- `!|` 或非（NOR）
- `!^` 同或（XNOR/NXOR，两者相同）

输入源（端点）是函数或常量。

#### 函数：
函数由其唯一的字符串 ID 在表达式中定义。直接写入函数 ID 即可。

#### 常量：
常量由 `{常量值}` 定义。在将来的版本中，还将支持运行时常量，允许你在运行时注入值。

目前，你只能使用布尔常量，例如 `{true}`, `{false}`, `{1}`, `{0}`。

保留名称有 `true`, `false`, `1`, `0`, `null`, `error`。你不应将它们用作运行时常量的 ID。不过作为函数 ID 是可以的，除了 `1` 和 `0`。


只需将所有内容组合在一起，你就可以写出类似 `A & (B | !C) & {true} & !{false} & !{0} & {1}` 的复杂条件。

编写条件时可以有空格，也可以没有。空格会被忽略。


## 支持的格式

本库还提供了一些其他常用的格式。

要使用这些格式，请使用它们各自的解析器。

### 常规格式

这是最常用的格式。

- 解析器：`parseNormal(str)`
- 转字符串：`toNormalString(node)`

- AND：`AB`
- OR：`A+B`
- NOT：`A'`（后缀）
- XOR：`A^B`

对于 NAND、NOR 等，可以写为 `!(AB)`, `!(A+B)`, `!(A^B)`。


### LaTeX 格式

- 解析器：`parseLatex(str)`
- 转字符串：`toLatexString(node)`

此格式模式提供直接解析或输出 LaTeX 格式的逻辑表达式的能力。

- NOT：`\overline{A}`
- AND：`AB`, `A \cdot B`, `A \wedge B`
- OR：`A + B`, `A \vee B`


## 化简

条件系统提供两个层次的化简方式。

### 基础化简

调用 `condition_node::simplify()` 对条件树进行基础化简。此过程会求值并折叠常量子表达式，并在可能的情况下展平逻辑节点。

> 基础化简是单次遍历过程——它只对树化简一次。某些需要迭代才能发现的化简机会可能会被遗漏。

局限性：

1. 输入是左结合的，结合律运算符可能无法完全化简。为确保尽可能化简，你需要确保第一对是可化简的。
2. 化简过程不是迭代的，这意味着它只对树进行一次化简。如果某些化简机会只能在第一次化简之后才能发现，它们将不会被化简。

### 深度化简（Quine-McCluskey 算法）

如需完全化简，请使用 `condition_simplifier::simplify()`。该函数实现了 Quine-McCluskey 算法，用于生成最小化的积之和表达式。

**用法：**

```cpp
scl2::condition_expression expr("A & (B | C) | !A & B");
scl2::condition_expression simplified = scl2::condition_simplifier::simplify(expr);
```

该算法从表达式生成真值表，找出所有质蕴涵项，选择必要质蕴涵项，并重构出最小化的表达式树。

> [!NOTE]
> Quine-McCluskey 算法目前接受最多 **8 个变量** 的输入。超过 8 个变量的输入将被拒绝。
