# Condition

+ Name: condition_node, condition_expression
+ Namespace: `scl2`
+ Document Version: `1.1.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `condition` |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::condition)
```

## Description

Condition module provides a way to define, parse, store, and evaluate complex condition structures. It allows you to calculate a result based on a set of boolean operators. You can input source values using functions, or even runtime-constant values.


## Quick Start

### Basic Usage

```cpp
#include <SharedCppLib2/condition.hpp>

// Create from string
// For the format, check the format section below.

scl2::condition_expression expr("A & (B | C)");

expr.setFunction("A", []() { return true; });
expr.setFunction("B", []() { return false; });
expr.setFunction("C", []() { return true; });

bool result = expr.evaluate(); // result will be true in this case.
```

You can also create the condition expression using the factory method, but that requires you to construct the tree manually. It is not suggested.

If you actually want to try that, refer to the `ConditionParser` class in `condition.cpp`.

There won't be example for that.


### Storage
It is possible to store the condition expression as a static array of bytes, and do whatever you want with it, like saving to a file, sending over the network, etc.

So it is also possible to prebuild a condition expression before compile time, and load it at runtime. This saves parsing time for the string version.

```cpp
#include <SharedCppLib2/condition.hpp>

condition_expression expr("A & (B | C)");

// Automatic detection is on the way, but for now you need
// to manually call the reserved api.
auto ba = scl2::condition_expression::save(expr);

std::ofstream ofs("condition.bin", std::ios::binary);

ofs << ba; // After an previous update is is now safe to use.
// ba.writeAllToStream(ofs); // Or the old way.

ofs.close();

// load

auto expr2 = scl2::condition_expression::load(ba);

// Note: you still need to open the file then read or wirte bytes to a bytearray.
// Relavant api is on the way, so in the future it can be done with one line:
scl2::writeFile("condition.bin", expr);

// and
auto expr = scl2::readAndLoad<condition_expression>("condition.bin");

// Although pretty unlikely to happen, you can encrypt your binary.
std::bytearray enba = scl2::encrypt<scl2::aes128>(ba, key, iv);

```

## Format

The format of the condition system is simple, but diffent from the format that is widely used in boolean algebra.

However, that format is also supported by this library. Check the document below.

Below is the default format of the condition expression string.

You can find other supported formats in [Supported Formats](#supported-formats) section.

Available operators:
- `&` for logical AND
- `|` for logical OR
- `^` for XOR
- `!` for logical NOT
- `(` and `)` for grouping
- `!&` for NAND
- `!|` for NOR
- `!^` for XNOR/NXOR (same thing)

Input sources (Endpoints) are functions or constants.

#### Functions:
Functions are defined by their unique string id in the expression. Directly write the function id.

#### Constants:
Constants are defined by `{constant_value}`. In a future version, runtime-constant will be available, which allows you to inject values at runtime.

Currently, you can only use boolean constants like `{true}`, `{false}`, `{1}`, `{0}`.

Reserved names are `true`, `false`, `1`, `0`, `null`, `error`. You should not use these names for runtime-constant id. They are fine to be used as function id though, except for `1` and `0`.


Simply merge everything together, and you can write complex conditions like `A & (B | !C) & {true} & !{false} & !{0} & {1}`.

You can write the condition with or without spaces. They will be ignored. 


## Supported Formats

This library also provides some other formats that were commonly used.

To use these formats, use their own parser.

### Normal Format

This is the most commonly used format.

- parser: `parseNormal(str)`
- to_string: `toNormalString(node)`

- AND: `AB`
- OR: `A+B`
- NOT: `A'` (postfix)
- XOR: `A^B`

For NAND, NOR, etc, you can write them as `!(AB)`, `!(A+B)`, `!(A^B)`.


### Latex Format

- parser: `parseLatex(str)`
- to_string: `toLatexString(node)`

This format mode provides direct ability to parse or output logical expressions written in LaTex format.

- NOT: `\overline{A}`
- AND: `AB`, `A \cdot B`, `A \wedge B`
- OR: `A + B`, `A \vee B`


## Simplification

The condition system provides two levels of simplification.

### Basic Simplification

Call `condition_node::simplify()` to perform a basic simplification on the condition tree. This process evaluates and collapses constant sub-expressions, and flattens logical nodes where possible.

> Basic simplification is a single-pass process — it simplifies the tree once. Some simplification opportunities that require iterative passes may be missed.

Limitations:

1. The input is left-combined, and associative operators may not be fully simplified. To make sure it was simplified as much as possible, you need to make sure the first pair is simplifiable.
2. The simplification process is not iterative, which means it only simplifies the tree once. If there are some simplification opportunities that can only be found after the first simplification, they will not be simplified.

### Deep Simplification (Quine-McCluskey Algorithm)

For full simplification, use `condition_simplifier::simplify()`. This implements the Quine-McCluskey algorithm to produce a minimal sum-of-products expression.

**Usage:**

```cpp
scl2::condition_expression expr("A & (B | C) | !A & B");
scl2::condition_expression simplified = scl2::condition_simplifier::simplify(expr);
```

The algorithm generates the truth table from the expression, finds all prime implicants, selects essential prime implicants, and reconstructs a minimal tree.

> [!NOTE]
> The Quine-McCluskey algorithm currently accepts inputs with up to **8 variables**. Inputs with more variables will be rejected.