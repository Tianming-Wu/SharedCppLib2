# string - 增强型字符串包装器

+ 名称: string
+ 命名空间: `scl2`
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `basic` (包含 string) |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::basic)
```

```cpp
#include <SharedCppLib2/string.hpp>
```

## 描述

`scl2::string`（以及 `scl2::wstring`）是对 `std::string` / `std::wstring` 的轻量包装，在保持与标准库 API 完全兼容的同时，添加了 split、join 等实用方法。它们公开继承自 `std::basic_string<CharT>`，因此任何接受 `const std::string&` 的函数都可以直接接受 `scl2::string`。

`scl2::string` = `scl2::basic_string<char>`  
`scl2::wstring` = `scl2::basic_string<wchar_t>`

> [!TIP]
> 所有标准 `std::string` 操作（`substr`、`find`、`operator[]`、迭代器等）均可通过继承使用。本文档仅涵盖额外的方法。

## 快速开始

```cpp
#include <SharedCppLib2/string.hpp>

scl2::string s = "hello,world,from,cpp";

// 分割为 stringlist
auto parts = s.split(',');
// parts = {"hello", "world", "from", "cpp"}

// 直接传给标准库函数
std::cout << s.substr(0, 5) << std::endl;  // "hello"

// 宽字符串版本
scl2::wstring ws = L"a,b,c";
auto wparts = ws.split(L',');
```

## 函数参考

### split

```cpp
scl2::stringlist split(CharT delim) const;
scl2::stringlist split(const basic_string<CharT> &delim) const;
scl2::stringlist split(const scl2::basic_stringlist<CharT> &delims) const;
```

按单个字符、子串或一组分隔符分割字符串。

**示例：**
```cpp
scl2::string s = "a,b c\td";

s.split(',');               // {"a", "b c\td"}
s.split(", ");              // {"a", "b", "c\td"}
s.split(scl2::stringlist{",", " ", "\t"});  // {"a", "b", "c", "d"}
```

### xsplit

```cpp
scl2::stringlist xsplit(const basic_string<CharT> &delim,
                        const basic_string<CharT> &begin_bind,
                        basic_string<CharT> end_bind = basic_string<CharT>(),
                        bool remove_binding = true) const;
```

带引号/括号感知的分割。匹配的 `begin_bind`/`end_bind` 对会保护其中的内容不被分割。如果 `end_bind` 为空，默认与 `begin_bind` 相同。

**示例：**
```cpp
scl2::string cmd = R"(cmd arg1 "quoted arg" arg3)";

// 引号感知分割
cmd.xsplit(" ", "\"");
// {"cmd", "arg1", "quoted arg", "arg3"}

// 括号感知：保留括号
cmd.xsplit(" ", "(", ")", false);
```

### exsplit

```cpp
scl2::stringlist exsplit(const basic_string<CharT> &delim,
                         const basic_string<CharT> &begin_bind,
                         basic_string<CharT> end_bind = basic_string<CharT>(),
                         bool remove_binding = false,
                         bool strict = false) const;
```

扩展分割。支持绑定字符出现在字符串中间（而非仅开头），并可选的严格模式。

> [!NOTE]
> 大多数场景下 `xsplit` 已足够。当绑定字符可能出现在元素中间时（如 `a(b)c`），再使用 `exsplit`。

## 字符串转换工具

`scl2` 命名空间中的以下自由函数提供 UTF-8 ↔ 宽字符串转换：

```cpp
std::wstring scl2::str_to_wstr(const std::string& str);
std::string  scl2::wstr_to_str(const std::wstring& wstr);
```

**平台实现：**

| 平台 | 实现方式 |
|------|----------|
| Windows | `MultiByteToWideChar` / `WideCharToMultiByte`，使用 `CP_UTF8` |
| Unix | 手动实现的 UTF-8 ↔ UTF-32（无已弃用的 `<codecvt>` 依赖） |

**示例：**
```cpp
std::string utf8 = "hello";
std::wstring wide = scl2::str_to_wstr(utf8);
std::string back = scl2::wstr_to_str(wide);
```

## 参见

- [`stringlist`](stringlist.md) — 所有 split 方法的返回类型
- [`xstring`](xstring.md) — 支持自动转换的多类型字符串 variant
