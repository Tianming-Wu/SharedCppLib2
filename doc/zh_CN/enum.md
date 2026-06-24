# Enum - 枚举工具库

+ 名称: enum
+ 命名空间: `scl2`
+ 文档版本: `1.1.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | 仅头文件（直接包含） |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::basic)  // 或任何能提供 include 路径的目标
```

```cpp
#include <SharedCppLib2/enum.hpp>
```

## 描述

enum 模块提供了一套用于处理 C++ 枚举的工具集合。包括：

- **枚举大小自动检测** — 自动检测连续枚举的值的数量
- **位标志运算符** — 为位标志枚举生成 `|`、`&`、`^`、`~`（及复合赋值）运算符
- **位标志迭代** — 使用 C++23 生成器遍历位标志枚举中已设置的位
- **枚举到字符串映射** — 将枚举值映射为可读字符串，位标志枚举支持多种回退策略

## 快速开始

### 基本枚举转字符串

```cpp
#include <SharedCppLib2/enum.hpp>

enum class Color {
    Red,
    Green,
    Blue
};

scl2_strenum(Color, strenum_fallback_value,
    scl2_pair(Color::Red,   "Red"),
    scl2_pair(Color::Green, "Green"),
    scl2_pair(Color::Blue,  "Blue")
)

int main() {
    Color c = Color::Green;
    std::cout << scl2::to_string(c).value_or("Unknown") << std::endl;
    // 输出: Green
    return 0;
}
```

> [!NOTE]
> 每个键值对必须使用 `scl2_pair(K, V)` 包裹，因为 C/C++ 预处理器不会将花括号 `{}` 视为嵌套分隔符——大括号内的逗号会错误地拆分可变参数。

### 位标志枚举与运算符

```cpp
#include <SharedCppLib2/enum.hpp>

enum class Flags : uint32_t {
    None  = 0,
    Read  = 1 << 0,
    Write = 1 << 1,
    Exec  = 1 << 2,
};

scl2_enum_bitop(Flags)
scl2_bitenum_traits(Flags, 0, 3)

int main() {
    Flags f = Flags::Read | Flags::Write;

    for (Flags bit : scl2::bitenum_iterator(f)) {
        // 依次产生 Flags::Read, 然后 Flags::Write
    }

    return 0;
}
```

### 位标志枚举转字符串

```cpp
enum class Permissions : uint32_t {
    Read  = 1 << 0,
    Write = 1 << 1,
    Exec  = 1 << 2,
};

scl2_enum_bitop(Permissions)
scl2_bitenum_traits(Permissions, 0, 3)
scl2_bitstrenum(Permissions, bitstrenum_fallback_partial,
    scl2_pair(Permissions::Read,  "Read"),
    scl2_pair(Permissions::Write, "Write"),
    scl2_pair(Permissions::Exec,  "Exec")
)

int main() {
    Permissions p = Permissions::Read | Permissions::Write;
    std::cout << scl2::to_string(p).value_or("") << std::endl;
    // 输出: Read | Write
    return 0;
}
```

## 宏定义

### scl2_enum_size

将 `scl2_enum_size` 作为连续枚举（从 0 开始）的最后一个枚举项插入，以允许自动检测大小。不适用于位标志枚举。

```cpp
enum class MyEnum {
    ValueA,
    ValueB,
    ValueC,
    scl2_enum_size  // 必须是最后一个；不是实际的枚举值
};
// MyEnum 现在有 3 个有效值 (0, 1, 2)，大小为 3
```

> [!WARNING]
> 前面的所有枚举项必须从 0 开始且连续。不要手动赋值。

### scl2_enum_bitop(NAME)

为指定枚举类型生成 `|`、`&`、`^`、`~` 运算符。不提供复合赋值运算符（`|=`、`&=`、`^=`）——请改用 `e = e | FlagA`。

### scl2_enum_bitopex(NAME)

`scl2_enum_bitop` 的扩展版本，额外生成 `|=`、`&=`、`^=` 复合赋值运算符。

### scl2_enum_bitop_inclass(NAME)

与 `scl2_enum_bitop` 相同，但生成 `friend` 函数，用于在类/结构体作用域内使用。

### scl2_bitenum_traits(NAME, MIN, MAX)

为位标志枚举特化 `bitenum_traits_lookup`，指定有效的位索引范围 `[MIN, MAX)`。这被 `bitenum_iterator` 和 `bitstrenum` 用于确定要遍历哪些位。

```cpp
scl2_bitenum_traits(MyFlags, 0, 4)  // 有效位索引: 0, 1, 2, 3
```

### scl2_pair(K, V)

包裹一个键值对，用于 `scl2_strenum` 或 `scl2_bitstrenum`。此宏是必需的，因为 C/C++ 预处理器不会将花括号 `{}` 视为嵌套分隔符——大括号内的逗号会错误地拆分可变参数。

```cpp
scl2_pair(Color::Red, "Red")  // 展开为 {Color::Red, "Red"}
```

### scl2_strenum(NAME, FALLBACK, ...)

便捷宏，用于创建 `strenum` 映射并注册到 ADL 查找，以配合 `to_string` 使用。

```cpp
scl2_strenum(MyEnum, strenum_fallback_value,
    scl2_pair(MyEnum::A, "ValueA"),
    scl2_pair(MyEnum::B, "ValueB")
)
```

### scl2_bitstrenum(NAME, FALLBACK, ...)

便捷宏，用于创建 `bitstrenum` 映射并注册到 ADL 查找，以配合 `to_string` 使用。

```cpp
scl2_bitstrenum(MyFlags, bitstrenum_fallback_partial,
    scl2_pair(MyFlags::FlagA, "FlagA"),
    scl2_pair(MyFlags::FlagB, "FlagB")
)
```

## 核心 API

### 迭代器函数

#### bitenum_iterator

```cpp
template<typename E>
requires std::is_enum_v<E>
std::generator<E> bitenum_iterator(E value);
```

遍历位标志枚举值中已设置的位。使用 `bitenum_traits_lookup<E>` 确定有效位范围。返回一个 C++23 `std::generator`，逐个产生每个已设置的标志位。

**用法：**
```cpp
for (auto bit : scl2::bitenum_iterator(flags)) {
    // 处理每个已设置的标志
}
```

#### bitenum_ranged_iterator

```cpp
template<typename E>
requires std::is_enum_v<E>
std::generator<E> bitenum_ranged_iterator(E value, size_t minVal, size_t maxVal);
```

与 `bitenum_iterator` 相同，但由调用者提供位索引范围 `[minVal, maxVal)`。不依赖 traits。

### 字符串映射类

#### strenum

```cpp
template<typename E, typename CharT = char>
class strenum : public std::map<E, std::basic_string<CharT>>;
```

将枚举值映射为字符串。用于非位标志枚举。

**构造函数：**
```cpp
strenum(
    std::initializer_list<std::pair<E, std::basic_string<CharT>>> list,
    strenum_fallback_type fallback = strenum_fallback_none
);
```

**方法：**

- `to_string(E value)` — 返回 `std::optional<string_type>`，包含映射的字符串或回退值。

**回退类型（`strenum_fallback_type`）：**

| 值 | 行为 |
|-------|----------|
| `strenum_fallback_value` | 返回原始整数值的字符串形式 |
| `strenum_fallback_empty` | 返回空字符串 |
| `strenum_fallback_none` | 返回 `std::nullopt` |

#### bitstrenum

```cpp
template<typename E, typename CharT = char>
class bitstrenum : public std::map<E, std::basic_string<CharT>>;
```

将位标志枚举值映射为字符串。遍历已设置的位，用 ` | ` 连接已知的标志名称。

**构造函数：**
```cpp
bitstrenum(
    std::initializer_list<std::pair<E, std::basic_string<CharT>>> list,
    bitstrenum_fallback_type fallback = bitstrenum_fallback_none
);
```

**回退类型（`bitstrenum_fallback_type`）：**

| 值 | 行为 |
|-------|----------|
| `bitstrenum_fallback_value` | 返回原始整数值 |
| `bitstrenum_fallback_empty` | 返回空字符串 |
| `bitstrenum_fallback_none` | 返回 `std::nullopt` |
| `bitstrenum_fallback_partial` | 仅返回已知位的字符串，忽略未知位 |
| `bitstrenum_fallback_bitset` | 返回 `0b...` 形式的位集字符串 |
| `bitstrenum_fallback_combined` | 返回 `已知名称 | 0b...` 组合已知和未知位 |
| `bitstrenum_fallback_combinedm` | 返回 `已知名称 | 1 << n` 组合已知和未知位 |

### 自由函数

#### to_string

```cpp
template<typename E, typename CharT = char>
requires std::is_enum_v<E>
inline std::optional<std::basic_string<CharT>> to_string(E value);
```

通过注册的 `strenum` 或 `bitstrenum` 映射（使用 ADL 查找），将枚举值转换为其字符串表示。

```cpp
auto s = scl2::to_string(myValue).value_or("?");
```

### 枚举转换器

#### enum_cvrt

```cpp
template<typename E1, typename E2>
class enum_cvrt;
```

用于在两个拥有相同逻辑值集合但底层表示不同的枚举类型之间进行双向转换。

**构造函数：**
```cpp
enum_cvrt(std::initializer_list<std::pair<E1, E2>> list);
```

**方法：**

- `E2 convert(E1 value)` — 从 `E1` 转换为 `E2`。未找到时抛出 `std::out_of_range`。
- `E1 convert(E2 value)` — 从 `E2` 转换为 `E1`。未找到时抛出 `std::out_of_range`。

**用法：**
```cpp
enum class ApiError { Ok, NotFound, Permission };
enum class DbError  { Success, Missing, Forbidden };

scl2::enum_cvrt<ApiError, DbError> cvrt({
    {ApiError::Ok,         DbError::Success},
    {ApiError::NotFound,   DbError::Missing},
    {ApiError::Permission, DbError::Forbidden},
});

ApiError e = cvrt.convert(DbError::Missing);  // ApiError::NotFound
