# xstring - Multi-Type String Variant

+ Name: xstring
+ Namespace: `scl2`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `basic` (contains xstring) |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::basic)
```

```cpp
#include <SharedCppLib2/xstring.hpp>
```

## Description

`xstring` is a variant-based string class that can hold any of five string types — `scl2::string`, `scl2::wstring`, `std::u8string`, `std::u16string`, or `std::u32string` — plus a null state. It provides:

- **Automatic type conversion** on access (`.a()`, `.w()`, `.str()`, etc.)
- **Zero-copy views** when the requested type matches the stored type
- **Passthrough operations** (`substr`, `find`, `contains`, etc.) that work regardless of the stored type
- **Implicit conversion** to `scl2::string` and `scl2::wstring` for seamless integration with standard library and SharedCppLib2 APIs

The core design principle: **minimize unnecessary conversions**. Data stays in its original encoding until explicitly requested in a different form.

> [!WARNING]
> When both `scl2::string` and `scl2::wstring` implicit conversion paths are viable (e.g., passing `xstring` to `std::fstream` or `std::filesystem::path`), the compiler may report ambiguity. Use `.a()` or `.w()` to explicitly select the desired type.

## Quick Start

```cpp
#include <SharedCppLib2/xstring.hpp>

// Construct from any string type
scl2::xstring a = "hello";          // ansi (scl2::string)
scl2::xstring w = L"world";         // wchar (scl2::wstring)

// Cross-type comparison
assert(a == wstr_to_str(L"hello"));  // automatic conversion

// Passthrough operations
scl2::xstring s = "hello world";
scl2::xstring sub = s.substr(0, 5);  // "hello"
size_t pos = s.find(scl2::xstring("world"));  // 6

// Implicit conversion to standard types
std::string stdStr = a;  // via operator scl2::string() → std::string
void takesString(const std::string& s);
takesString(a);          // works directly
```

## Type System

### xstype Enum

```cpp
enum class scl2::xstype {
    null,      // no string stored
    ansi,      // scl2::string (UTF-8 char)
    wchar,     // scl2::wstring (UTF-16 on Windows, UTF-32 on Unix)
    u8char,    // std::u8string (UTF-8 char8_t)
    u16char,   // std::u16string (UTF-16 char16_t)
    u32char    // std::u32string (UTF-32 char32_t)
};
```

The stored type is exposed via `type()`:

```cpp
scl2::xstring s = "hello";
s.type() == scl2::xstype::ansi;  // true
```

### Null State

A default-constructed `xstring` is in the null state (`xstype::null`):

- `empty()` returns `true`
- `size()` returns `0`
- All view accessors return empty views
- **Passthrough operations** (`substr`, `find`, etc.) throw `std::runtime_error`
- `operator==` with another null returns `true`; with non-null returns `false`

Use `reset()` to return to null state. Use `clear()` to empty the content while preserving the type.

## Constructors

```cpp
xstring();                                    // null
xstring(const scl2::string& s);              // ansi
xstring(const scl2::wstring& s);             // wchar
xstring(const std::u8string& s);             // u8char
xstring(const std::u16string& s);            // u16char
xstring(const std::u32string& s);            // u32char
```

> [!NOTE]
> `const char*` and `const wchar_t*` literals work directly — they convert to `scl2::string` / `scl2::wstring` via inherited `std::string` constructors.

## Typed Access

### Copy Access

```cpp
scl2::string a() const;       // to ansi string
scl2::wstring w() const;      // to wide string
std::u8string u8() const;     // to UTF-8 string
std::u16string u16() const;   // to UTF-16 string
std::u32string u32() const;   // to UTF-32 string
```

Also available as shorter aliases: `str()`, `wstr()`, `u8str()`, `u16str()`, `u32str()`.

When the requested type matches the stored type, the value is returned directly (no conversion). Otherwise, automatic conversion is performed:

| Stored Type | Converted To | Method |
|-------------|-------------|--------|
| wchar → ansi | UTF-8 string | `platform::wstringToString` (Win) / UTF-32→UTF-8 (Unix) |
| ansi → wchar | Wide string | `platform::stringToWstring` (Win) / UTF-8→UTF-32 (Unix) |
| u8char → ansi | Direct reinterpret | Both UTF-8 |
| u16/u32 → other | — | Not yet implemented (returns empty) |

### View Access (Zero-Copy)

```cpp
std::string_view a_view() const;
std::wstring_view w_view() const;
std::u8string_view u8_view() const;
std::u16string_view u16_view() const;
std::u32string_view u32_view() const;
```

Return a view into the stored data when the type matches. Return an empty view otherwise. No allocation, no conversion.

## Conversions

### Implicit Conversion Operators

```cpp
operator scl2::string() const;    // implicit
operator scl2::wstring() const;   // implicit
operator std::u8string() const;   // implicit
operator std::u16string() const;   // implicit
operator std::u32string() const;   // implicit
```

Since `scl2::string` IS-A `std::string`, an `xstring` can be passed to any function accepting `const std::string&`. The same applies to `scl2::wstring` and `std::wstring`.

### Explicit Conversion Operators

```cpp
explicit operator std::string() const;
explicit operator std::wstring() const;
```

For direct initialization: `std::string s(xstr);` uses the explicit operator.

### Type Conversion

```cpp
xstring& convert(xstype target);          // in-place
xstring converted(xstype target) const;   // new object
```

## Assignment

### Switch-Type Assignment (operator=)

```cpp
xstring& operator=(const scl2::string& s);
xstring& operator=(const scl2::wstring& s);
xstring& operator=(const std::u8string& s);
xstring& operator=(const std::u16string& s);
xstring& operator=(const std::u32string& s);
```

The stored type changes to match the assigned value.

```cpp
scl2::xstring xs;
xs = "hello";       // ansi
xs = L"world";      // now wchar (type switched)
```

### Keep-Type Assignment

```cpp
xstring& assign(const xstring& s);
```

Converts the source to the current stored type without changing the type.

```cpp
scl2::xstring a = "hello";
scl2::xstring w = L"world";
a.assign(w);  // a stays ansi, content becomes "world"
```

## Passthrough Operations

These methods work regardless of the stored type by dispatching through the variant. For methods that take a string argument (like `find`), the argument is automatically converted to match the stored type.

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
> All passthrough operations throw `std::runtime_error` when the xstring is in the null state, since operations like `substr` are semantically invalid on a non-existent string.

## Comparison

```cpp
bool operator==(const xstring& other) const;
bool operator!=(const xstring& other) const;
```

Cross-type comparison is supported: the right-hand side is automatically converted to the left-hand side's type before comparing.

```cpp
scl2::xstring a = "hello";
scl2::xstring w = L"hello";
assert(a == w);  // true — cross-type comparison
```

## Free Function: to_xstring

```cpp
template<typename T, xstype prefType = xstype::ansi>
xstring to_xstring(const T& val);
```

Converts numeric types to `xstring` via `std::to_string` / `std::to_wstring`. The preferred type is a compile-time template parameter for zero-overhead dispatch.

```cpp
auto s = scl2::to_xstring(42);                        // "42" (ansi)
auto w = scl2::to_xstring<int, scl2::xstype::wchar>(42);  // L"42" (wchar)
```

## See Also

- [`string`](string.md) — The underlying string wrapper with split/join methods
- [`stringlist`](stringlist.md) — String list with advanced splitting
- [`platform`](platform.md) — Platform abstraction (used internally for string conversion)
