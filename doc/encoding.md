# encoding - The Encoding Boundary

+ Name: encoding
+ Namespace: `scl2`
+ Header: `<SharedCppLib2/encoding.hpp>`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `encoding` |

Include usage:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::encoding)
```

```cpp
#include <SharedCppLib2/encoding.hpp>
```

`encoding` depends on nothing but the standard library and the conversion API of the platform, so
it is a [standalone module](standalone_module.md).

## The contract

**Text inside the library is UTF-8. Conversion happens at the edges, and an invalid sequence
becomes `U+FFFD` instead of being passed along or dropped.**

Concretely:

- A `std::string` in SharedCppLib2 holds UTF-8 bytes; a `std::wstring` holds UTF-16 on Windows and
  UTF-32 elsewhere. Nothing else counts as text.
- Text from outside is converted **once**, at the boundary where it enters or leaves: the console,
  a path, a system call, a file that somebody else wrote in another encoding.
- A sequence that is not valid in the encoding it is supposed to be in becomes `U+FFFD` (the
  replacement character). Cutting the text instead would silently take everything after it with it.
- Where a conversion cannot keep everything, it says so rather than pretending: a character the
  target cannot hold becomes its replacement character (`?` in most code pages), which is why
  `to_codepage()` belongs as the last step before the call that needs it.

## The boundaries

| Boundary | What goes wrong when it is ignored | What to call |
|---------|------------------------------------|--------------|
| the console | UTF-8 written to it is decoded in whatever code page the console is in — mojibake on Windows | [`platform::windows::enable_utf8_console()`](#enable_utf8_console) |
| a path on its way to a person, a log or another UTF-8 file | `path.string()` converts through the ANSI code page, so a path that code page cannot hold comes out mangled or throws | [`scl2::path_to_utf8()`](#path_to_utf8) |
| text that is not UTF-8 at all (a GBK file, a system call) | there is no reliable way to detect an encoding, so guessing gets it wrong | [`scl2::from_codepage()`](#from_codepage) / [`scl2::from_ansi()`](#from_ansi) |
| a call that only takes ANSI (a `*A` Win32 call, a C library) | the text does not fit | [`scl2::to_codepage()`](#to_codepage) / [`scl2::to_ansi()`](#to_ansi) |
| UTF-8 ↔ wide text | — | [`scl2::str_to_wstr()` / `wstr_to_str()`](string.md) in `string` |
| `argv` and the environment | the narrow forms are ANSI encoded | `platform::windows::wargProvider`, then `from_ansi()` |
| the first bytes of a file | a byte order mark is not text | [`scl2::strip_bom()`](string.md) in `string` |

This module holds the two code page conversions and the path one; the console belongs to `platform`
and the two text-level ones to `string`, next to the rest of the Win32 and string work.

## Quick Start

```cpp
#include <SharedCppLib2/encoding.hpp>

// The console first, or everything below shows up as mojibake on Windows.
const auto saved = platform::windows::enable_utf8_console();

// A path in a message: path_to_utf8(), not path.string().
std::filesystem::path file = L"数据/配置.toml";
std::string message = "reading " + scl2::path_to_utf8(file);

// Text that is not UTF-8 — a GBK file, an ANSI system call.
std::string utf8 = scl2::from_codepage(936, gbk_bytes);

// ... and out again, for a call that only takes ANSI.
SetWindowTextA(hwnd, scl2::to_ansi("标题").c_str());

platform::windows::restore_console_code_pages(saved);
```

## Function Reference

### is_valid_utf8

```cpp
bool is_valid_utf8(std::string_view text) noexcept;
```

Whether the whole text is UTF-8: no truncated sequence, no stray continuation byte, no overlong
form, no surrogate half, nothing above `U+10FFFF`. This is the check to make when the answer
decides what to do with the text — a conversion is not the way to test whether one is needed.

### path_to_utf8

```cpp
std::string path_to_utf8(const std::filesystem::path& path);
```

The path as UTF-8 text, which is what `std::filesystem::path::u8string()` gives. Use it wherever a
path is shown to a person, written to a log, put into an error message or stored in a UTF-8 file;
keep the `fs::path` itself for the filesystem.

For a Chinese path on a Chinese system, `path.string()` gives the bytes the ANSI code page holds
(`D6 D0 CE C4 ...`) while this gives UTF-8 (`E4 B8 AD E6 96 87 ...`) — the same path, two
encodings, and only one of them is what the rest of the library expects.

### from_codepage

```cpp
std::string from_codepage(unsigned int codepage, std::string_view text);
```

Text that is in `codepage` into UTF-8. The code page is the caller's statement of what the bytes
are (`936` GBK, `65001` UTF-8, `20127` US-ASCII, `0` the system's ANSI code page); the library does
not guess. Bytes the code page cannot map become the replacement character.

Throws `std::runtime_error` when the system does not know the code page.

Outside Windows there is one encoding and the text is returned unchanged.

### to_codepage

```cpp
std::string to_codepage(unsigned int codepage, std::string_view text);
```

UTF-8 into `codepage`, for a call that needs it. This is the lossy direction: a character the code
page cannot hold becomes its replacement character. Keep the result local — convert at the call,
not earlier, so nothing else has to work with text that has already lost something.

Throws `std::runtime_error` when the system does not know the code page, and outside Windows
returns the text unchanged.

### from_ansi / to_ansi

```cpp
std::string from_ansi(std::string_view text);
std::string to_ansi(std::string_view text);
```

The same two conversions in the code page of the system. `from_ansi()` is what `argv`, the
environment and a `*A` call hand you; `to_ansi()` is what they expect back.

### enable_utf8_console

```cpp
namespace platform::windows {

struct console_code_pages {
    unsigned int output = 0;
    unsigned int input = 0;
};

console_code_pages enable_utf8_console();
void restore_console_code_pages(const console_code_pages& pages);

}
```

`enable_utf8_console()` puts the console in `CP_UTF8` (both the output and the input code page)
and returns the code pages it was in before, so `restore_console_code_pages()` can put them back.
Both fields are `0` when there is no console attached — a windowless process, or output that has
been redirected.

Without it, a UTF-8 string written with `std::cout` is decoded by the console in whatever code page
it is currently in, which on Windows means mojibake. A program that is going to print non-ASCII
text should call it once at startup, and put the code pages back on the way out if it shares the
console.

## Writing a new module

The rule is short enough to follow without thinking about it:

- Keep text as `std::string` in UTF-8, and say so in the documentation of anything that takes or
  returns it.
- Convert where the text crosses the boundary, and use the helpers above rather than a conversion
  of your own — a module that is standalone keeps its own few lines and says so.
- Never let an invalid sequence through unmentioned: replace it with `U+FFFD`, or reject the input,
  but do not silently cut the text.

## See Also

- [`string`](string.md) — `str_to_wstr` / `wstr_to_str`, and `strip_bom` for a byte order mark
- [`standalone_module`](standalone_module.md) — what a standalone module is, and how one is marked
