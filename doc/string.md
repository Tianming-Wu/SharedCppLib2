# string - Enhanced String Wrapper

+ Name: string
+ Namespace: `scl2`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `basic` (contains string) |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::basic)
```

```cpp
#include <SharedCppLib2/string.hpp>
```

## Description

`scl2::string` (and `scl2::wstring`) are thin wrappers around `std::string` / `std::wstring` that add split, join, and utility methods while remaining fully compatible with standard library APIs. They inherit publicly from `std::basic_string<CharT>`, so any function expecting `const std::string&` accepts `scl2::string` directly.

`scl2::string` = `scl2::basic_string<char>`  
`scl2::wstring` = `scl2::basic_string<wchar_t>`

> [!TIP]
> All standard `std::string` operations (`substr`, `find`, `operator[]`, iterators, etc.) are available through inheritance. This document only covers the additional methods.

## Quick Start

```cpp
#include <SharedCppLib2/string.hpp>

scl2::string s = "hello,world,from,cpp";

// Split into stringlist
auto parts = s.split(',');
// parts = {"hello", "world", "from", "cpp"}

// Pass to standard library functions directly
std::cout << s.substr(0, 5) << std::endl;  // "hello"

// Wstring variant
scl2::wstring ws = L"a,b,c";
auto wparts = ws.split(L',');
```

## Function Reference

### split

```cpp
scl2::stringlist split(CharT delim) const;
scl2::stringlist split(const basic_string<CharT> &delim) const;
scl2::stringlist split(const scl2::basic_stringlist<CharT> &delims) const;
```

Splits the string by a single character, a substring, or a set of delimiters.

**Examples:**
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

Splits with quote/bracket awareness. Matching `begin_bind`/`end_bind` pairs protect enclosed content from splitting. If `end_bind` is empty, it defaults to the same as `begin_bind`.

**Examples:**
```cpp
scl2::string cmd = R"(cmd arg1 "quoted arg" arg3)";

// Quote-aware split
cmd.xsplit(" ", "\"");
// {"cmd", "arg1", "quoted arg", "arg3"}

// Bracket-aware: keep the brackets
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

Extended split. Supports binding characters that appear **inside** strings (not just at the start), and an optional strict mode.

> [!NOTE]
> `xsplit` is sufficient for most use cases. Use `exsplit` when bind characters can appear mid-element (e.g., `a(b)c`).

## String Conversion Utilities

These free functions in `scl2` namespace provide UTF-8 ↔ wide string conversion:

```cpp
std::wstring scl2::str_to_wstr(const std::string& str);
std::string  scl2::wstr_to_str(const std::wstring& wstr);
```

**Platform behavior:**

| Platform | Implementation |
|----------|---------------|
| Windows | `MultiByteToWideChar` / `WideCharToMultiByte` with `CP_UTF8` |
| Unix | Hand-rolled UTF-8 ↔ UTF-32 (no deprecated `<codecvt>`) |

**Example:**
```cpp
std::string utf8 = "hello";
std::wstring wide = scl2::str_to_wstr(utf8);
std::string back = scl2::wstr_to_str(wide);
```

## See Also

- [`stringlist`](stringlist.md) — The return type of all split methods
- [`xstring`](xstring.md) — Multi-type string variant with automatic conversion
