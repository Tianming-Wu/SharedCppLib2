# xstring - 多类型字符串 Variant

+ 名称: xstring
+ 命名空间: `scl2`
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `basic` (包含 xstring) |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::basic)
```

```cpp
#include <SharedCppLib2/xstring.hpp>
```

## 描述

`xstring` 是一个基于 variant 的字符串类，可以持有五种字符串类型中的任意一种 — `scl2::string`、`scl2::wstring`、`std::u8string`、`std::u16string` 或 `std::u32string` — 外加一个空（null）状态。它提供：

- **自动类型转换**（`.a()`、`.w()`、`.str()` 等）
- **零拷贝视图**，当请求类型与存储类型匹配时
- **穿透操作**（`substr`、`find`、`contains` 等），无论存储何种类型均可工作
- **隐式转换**到 `scl2::string` 和 `scl2::wstring`，与标准库和 SharedCppLib2 API 无缝集成

核心设计原则：**尽可能减少转换**。数据保持原始编码，直到显式请求其他形式。

> [!WARNING]
> 当 `scl2::string` 和 `scl2::wstring` 两条隐式转换路径都可行时（例如将 `xstring` 传给 `std::fstream` 或 `std::filesystem::path`），编译器可能报歧义错误。请使用 `.a()` 或 `.w()` 显式选择所需类型。

## 快速开始

```cpp
#include <SharedCppLib2/xstring.hpp>

// 从任意字符串类型构造
scl2::xstring a = "hello";          // ansi (scl2::string)
scl2::xstring w = L"world";         // wchar (scl2::wstring)

// 跨类型比较
assert(a == scl2::wstr_to_str(L"hello"));  // 自动转换

// 穿透操作
scl2::xstring s = "hello world";
scl2::xstring sub = s.substr(0, 5);  // "hello"
size_t pos = s.find(scl2::xstring("world"));  // 6

// 隐式转换为标准类型
std::string stdStr = a;  // 通过 operator scl2::string() → std::string
void takesString(const std::string& s);
takesString(a);          // 直接可用
```

## 类型系统

### xstype 枚举

```cpp
enum class scl2::xstype {
    null,      // 未存储任何字符串
    ansi,      // scl2::string (UTF-8 char)
    wchar,     // scl2::wstring (Windows: UTF-16, Unix: UTF-32)
    u8char,    // std::u8string (UTF-8 char8_t)
    u16char,   // std::u16string (UTF-16 char16_t)
    u32char    // std::u32string (UTF-32 char32_t)
};
```

存储类型通过 `type()` 获取：

```cpp
scl2::xstring s = "hello";
s.type() == scl2::xstype::ansi;  // true
```

### Null 状态

默认构造的 `xstring` 处于 null 状态（`xstype::null`）：

- `empty()` 返回 `true`
- `size()` 返回 `0`
- 所有视图访问器返回空视图
- **穿透操作**（`substr`、`find` 等）抛出 `std::runtime_error`
- `operator==` 与另一个 null 比较返回 `true`；与非 null 比较返回 `false`

使用 `reset()` 回到 null 状态。使用 `clear()` 清空内容但保留类型。

## 构造函数

```cpp
xstring();                                    // null
xstring(const scl2::string& s);              // ansi
xstring(const scl2::wstring& s);             // wchar
xstring(const std::u8string& s);             // u8char
xstring(const std::u16string& s);            // u16char
xstring(const std::u32string& s);            // u32char
```

> [!NOTE]
> `const char*` 和 `const wchar_t*` 字面量可直接使用 — 它们通过继承的 `std::string` 构造函数转换为 `scl2::string` / `scl2::wstring`。

## 类型化访问

### 拷贝访问

```cpp
scl2::string a() const;       // 转为 ansi 字符串
scl2::wstring w() const;      // 转为宽字符串
std::u8string u8() const;     // 转为 UTF-8 字符串
std::u16string u16() const;   // 转为 UTF-16 字符串
std::u32string u32() const;   // 转为 UTF-32 字符串
```

同时提供更短的别名：`str()`、`wstr()`、`u8str()`、`u16str()`、`u32str()`。

当请求类型与存储类型匹配时，直接返回值（无转换）。否则执行自动转换：

| 存储类型 | 转换目标 | 方法 |
|---------|---------|------|
| wchar → ansi | UTF-8 字符串 | Win: WideCharToMultiByte / Unix: UTF-32→UTF-8 |
| ansi → wchar | 宽字符串 | Win: MultiByteToWideChar / Unix: UTF-8→UTF-32 |
| u8char → ansi | 直接 reinterpret | 两者均为 UTF-8 |
| u16/u32 → 其他 | — | 尚未实现（返回空） |

### 视图访问（零拷贝）

```cpp
std::string_view a_view() const;
std::wstring_view w_view() const;
std::u8string_view u8_view() const;
std::u16string_view u16_view() const;
std::u32string_view u32_view() const;
```

类型匹配时返回内部数据的视图，不匹配时返回空视图。无内存分配，无转换。

## 类型转换

### 隐式转换运算符

```cpp
operator scl2::string() const;    // 隐式
operator scl2::wstring() const;   // 隐式
operator std::u8string() const;   // 隐式
operator std::u16string() const;  // 隐式
operator std::u32string() const;  // 隐式
```

由于 `scl2::string` IS-A `std::string`，`xstring` 可以传给任何接受 `const std::string&` 的函数。`scl2::wstring` 与 `std::wstring` 同理。

### 显式转换运算符

```cpp
explicit operator std::string() const;
explicit operator std::wstring() const;
```

用于直接初始化：`std::string s(xstr);` 使用显式运算符。

### 类型转换方法

```cpp
xstring& convert(xstype target);          // 原地转换
xstring converted(xstype target) const;   // 返回新对象
```

## 赋值

### 切换类型赋值（operator=）

```cpp
xstring& operator=(const scl2::string& s);
xstring& operator=(const scl2::wstring& s);
xstring& operator=(const std::u8string& s);
xstring& operator=(const std::u16string& s);
xstring& operator=(const std::u32string& s);
```

存储类型会变为与赋值内容一致。

```cpp
scl2::xstring xs;
xs = "hello";       // ansi
xs = L"world";      // 现在变为 wchar（类型已切换）
```

### 保持类型赋值

```cpp
xstring& assign(const xstring& s);
```

将源转换为当前存储类型，不改变类型。

```cpp
scl2::xstring a = "hello";
scl2::xstring w = L"world";
a.assign(w);  // a 保持 ansi，内容变为 "world"
```

## 穿透操作

以下方法无论存储何种类型均可工作，通过 variant 分派实现。对于接受字符串参数的方法（如 `find`），参数会自动转换为当前存储类型。

```cpp
xstring substr(size_t pos, size_t n) const;
xstring substr(size_t pos) const;

size_t find(const xstring& str, size_t pos = 0) const;
size_t rfind(const xstring& str, size_t pos = std::string::npos) const;

bool contains(const xstring& str) const;
bool starts_with(const xstring& prefix) const;
bool ends_with(const xstring& suffix) const;
```

> [!NOTE]
> 当 xstring 处于 null 状态时，所有穿透操作会抛出 `std::runtime_error`，因为 `substr` 等操作在语义上对不存在的字符串无效。

## 比较

```cpp
bool operator==(const xstring& other) const;
bool operator!=(const xstring& other) const;
```

支持跨类型比较：右侧会自动转换为左侧的类型后再比较。

```cpp
scl2::xstring a = "hello";
scl2::xstring w = L"hello";
assert(a == w);  // true — 跨类型比较
```

## 自由函数: to_xstring

```cpp
template<typename T, xstype prefType = xstype::ansi>
xstring to_xstring(const T& val);
```

通过 `std::to_string` / `std::to_wstring` 将数值类型转换为 `xstring`。首选类型为编译期模板参数，零开销分派。

```cpp
auto s = scl2::to_xstring(42);                           // "42" (ansi)
auto w = scl2::to_xstring<int, scl2::xstype::wchar>(42); // L"42" (wchar)
```

## 参见

- [`string`](string.md) — 底层字符串包装器，提供 split/join 方法
- [`stringlist`](stringlist.md) — 带高级分割功能的字符串列表
