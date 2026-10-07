# encoding - 编码边界

+ 名称: encoding
+ 命名空间: `scl2`
+ 头文件: `<SharedCppLib2/encoding.hpp>`
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库 | `encoding` |

引入方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::encoding)
```

```cpp
#include <SharedCppLib2/encoding.hpp>
```

`encoding` 只依赖标准库和平台自己的转换 API，因此它是一个[独立模块](standalone_module.md)。

## 约定

**库内部一律是 UTF-8；转换发生在边界上；非法序列变成 `U+FFFD`，既不原样传下去，也不悄悄丢掉。**

具体地说：

- 在 SharedCppLib2 里，`std::string` 装的是 UTF-8 字节，`std::wstring` 在 Windows 上是 UTF-16、
  其它平台上是 UTF-32。除此之外的东西都不算“文本”。
- 外来的文本只在它进出库的那一道边界上转换**一次**：控制台、路径、系统调用、别人用另一种编码
  写出的文件。
- 在它本该遵守的编码里不合法的序列，换成 `U+FFFD`（替换字符）。直接截断会把后面的内容一起
  悄悄带走。
- 转换保不住全部内容时，它明说而不是装作无事：目标编码装不下的字符变成它的替代字符（多数
  代码页里是 `?`）——所以 `to_codepage()` 应该是需要它的那次调用之前的最后一步。

## 边界在哪里

| 边界 | 不管它会怎样 | 该用什么 |
|---------|------------------------------------|--------------|
| 控制台 | 写进去的 UTF-8 会按控制台当前的代码页解码，Windows 上就是乱码 | [`platform::windows::enable_utf8_console()`](#enable_utf8_console) |
| 要给人看、写进日志或另一个 UTF-8 文件的路径 | `path.string()` 走 ANSI 代码页，代码页装不下的路径会变成乱码或直接抛异常 | [`scl2::path_to_utf8()`](#path_to_utf8) |
| 根本不是 UTF-8 的文本（GBK 文件、系统调用给的字节） | 编码没有可靠的探测办法，靠猜一定会猜错 | [`scl2::from_codepage()`](#from_codepage) / [`scl2::from_ansi()`](#from_ansi) |
| 只接受 ANSI 的调用（`*A` 版 Win32 API、C 库） | 文本放不进去 | [`scl2::to_codepage()`](#to_codepage) / [`scl2::to_ansi()`](#to_ansi) |
| UTF-8 与宽字符串之间 | — | `string` 里的 [`scl2::str_to_wstr()` / `wstr_to_str()`](string.md) |
| `argv` 与环境变量 | 窄字符形式是 ANSI 编码的 | `platform::windows::wargProvider`，再用 `from_ansi()` |
| 文件开头的几个字节 | BOM 不是文本 | `string` 里的 [`scl2::strip_bom()`](string.md) |

本模块放着两个代码页转换和路径那个；控制台属于 `platform`，剩下两个文本层面的属于 `string`，
和那里的 Win32 与字符串工作待在一起。

## 快速开始

```cpp
#include <SharedCppLib2/encoding.hpp>

// 先设控制台，否则下面在 Windows 上都是乱码。
const auto saved = platform::windows::enable_utf8_console();

// 消息里的路径：用 path_to_utf8()，不用 path.string()。
std::filesystem::path file = L"数据/配置.toml";
std::string message = "reading " + scl2::path_to_utf8(file);

// 不是 UTF-8 的文本 —— GBK 文件、ANSI 的系统调用。
std::string utf8 = scl2::from_codepage(936, gbk_bytes);

// ……再转出去，给只接受 ANSI 的调用。
SetWindowTextA(hwnd, scl2::to_ansi("标题").c_str());

platform::windows::restore_console_code_pages(saved);
```

## 函数参考

### is_valid_utf8

```cpp
bool is_valid_utf8(std::string_view text) noexcept;
```

整段文本是不是合法的 UTF-8：没有截断的序列、没有杂散的续字节、没有过长编码、没有代理区
码点、没有超过 `U+10FFFF` 的东西。当你需要根据答案决定怎么处理这段文本时用它——
判断“要不要转换”不该靠转换本身。

### path_to_utf8

```cpp
std::string path_to_utf8(const std::filesystem::path& path);
```

路径的 UTF-8 文本形式，也就是 `std::filesystem::path::u8string()` 给出的东西。凡是路径要给人看、
写进日志、放进错误信息或存进 UTF-8 文件，都用它；文件系统操作本身仍然拿 `fs::path`。

在中文系统上，一个中文路径经 `path.string()` 得到的是 ANSI 代码页装得下的那些字节
（`D6 D0 CE C4 ...`），而这里得到的是 UTF-8（`E4 B8 AD E6 96 87 ...`）——同一个路径，两种编码，
而库的其余部分只认其中一种。

### from_codepage

```cpp
std::string from_codepage(unsigned int codepage, std::string_view text);
```

把处在 `codepage` 里的文本转成 UTF-8。代码页是调用方对“这些字节是什么”的声明
（`936` GBK、`65001` UTF-8、`20127` US-ASCII、`0` 系统 ANSI 代码页）；库本身不做猜测。代码页
映射不了的字节变成替换字符。

系统不认识这个代码页时抛 `std::runtime_error`。

非 Windows 平台上只有一种编码，文本原样返回。

### to_codepage

```cpp
std::string to_codepage(unsigned int codepage, std::string_view text);
```

把 UTF-8 转成 `codepage`，给需要它的调用用。这是有损的方向：代码页装不下的字符会变成它的
替换字符。结果只在本地用——在调用处转换，而不是更早，这样就没有别的东西需要拿着一份已经
丢过信息的文本工作。

系统不认识这个代码页时抛 `std::runtime_error`，非 Windows 平台上文本原样返回。

### from_ansi / to_ansi

```cpp
std::string from_ansi(std::string_view text);
std::string to_ansi(std::string_view text);
```

上面的两个转换，代码页取系统的 ANSI 代码页。`argv`、环境变量和 `*A` 版 API 交给你的就是
`from_ansi()` 要处理的东西；它们等着接回去的就是 `to_ansi()` 的结果。

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

`enable_utf8_console()` 把控制台切到 `CP_UTF8`（输出和输入代码页都切），并返回它原来用的
代码页，交给 `restore_console_code_pages()` 换回去。没有控制台时（无窗口的进程，或输出被重定向）
两个字段都是 `0`。

不做这一步，用 `std::cout` 写出的 UTF-8 会被控制台按它当前的代码页解码，在 Windows 上就是乱码。
要打印非 ASCII 文本的程序应该在启动时调用一次；如果控制台是和别人共用的，退出时把代码页换回去。

## 写新模块时

这条约定短到不需要特意记：

- 文本一律保持成 UTF-8 的 `std::string`，并且在接受或返回它的接口文档里写明这一点。
- 只在文本跨过边界的地方转换，并且用上面的辅助函数，而不是自己写一份——独立模块保留自己的
  那几行，并说明自己是独立的。
- 不要让非法序列无声通过：换成 `U+FFFD`，或者拒绝这份输入，但不要悄悄截断它。

## 参见

- [`string`](string.md) —— `str_to_wstr` / `wstr_to_str`，以及去掉 BOM 的 `strip_bom`
- [`standalone_module`](standalone_module.md) —— 什么是独立模块，怎么标记
