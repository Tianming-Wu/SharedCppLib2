# settings - 绑定到一个文件的设置对象

+ 名称: settings
+ 命名空间: `scl2`
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `settings` |
| 依赖 | `json2`、`toml`、`ini`、`xkeydb` |

包含方式:

```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::settings)
```

```cpp
#include <SharedCppLib2/settings.hpp>
```

## 描述

`scl2::settings` 持有一个应用程序的设置，并绑定到一个文件。键是值在文件里的路径——`window/width`
就是对象 `window` 的成员 `width`——文件按扩展名选一种格式写出。

| 后端 | 文件 | 保存时保留什么 |
|---------|---------|---------|
| `json` | JSON 文本 | 成员的顺序，以及每个未改动值的写法 |
| `toml` | TOML 文本 | 同上，另外还有注释、空行与行尾符 |
| `ini` | INI 文本 | 节与键；值是文本，所以类型从写法读出来 |
| `xkeydb` | 二进制 | 带类型的值，可选压缩、校验与加密 |

值是 null、bool、整数、浮点、字符串或字节之一。赋给它什么类型，它就以什么类型保存；INI 例外，
那里一切都是文本。

## 快速开始

```cpp
#include <SharedCppLib2/settings.hpp>

scl2::settings config("config.toml");

int width = config.get("window/width", 800);     // 键不存在时得到 800
config.set("window/width", 1024);
config.set("window/title", "Editor");

config.save();                                    // 析构时也会写一次
```

除非调用过 `set_auto_save(false)`，否则对象析构时文件会被写出，所以只有"文件必须在那之前就存在"
时才需要显式 `save()`。这时文件里是：

```toml
[window]
width = 1024
title = "Editor"
```

## 键

键是用 `/` 分隔的一串名字。首尾的 `/` 可以有，空段会被跳过，所以 `/window/width` 与
`window/width` 是同一个键。

- **全数字的段**寻址数组元素：`servers/1/host`。
- 尚不存在的层，在寻址它的段全为数字时成为**数组**，否则成为**对象**。`set("pinned/0", 3)`
  会把 `pinned` 造成数组。
- 名字里不能含 `/`，那是分隔符。

> [!NOTE]
> INI 是扁平的，只有前两段有意义：第一段是节，其余各段拼成键。只有一段的键归到 `General` 节。

## 值

### 写入

```cpp
config.set("window/width", 1024);                  // 整数
config.set("window/ratio", 1.5);                   // 浮点
config.set("window/title", "Editor");              // 字符串
config.set("window/maximised", true);              // bool
config.set("window/icon", scl2::bytearray(icon));  // 字节
config.set("window/width", nullptr);               // 删除该键
config.set_default("window/height", 600);          // 键还没有值时才写
```

`set()` 接受 bool、任意整数或浮点类型、任意枚举（按底层值保存）、`std::string`、`const char*`、
`scl2::bytearray`、平凡数据类（它的字节就是值），以及支持 `dump()` / `load()` 的类。null 值删除该键。

### 读取

```cpp
int width = config.get("window/width", 800);
std::string title = config.get("window/title", std::string());
bool maximised = config.get("window/maximised", false);
scl2::bytearray icon = config.get("window/icon", scl2::bytearray());

scl2::settings_key value = config.get("window/width");   // 不存在时是 null
if (value.is_int()) { ... }
if (config.contains("window/width")) { ... }
```

键不存在、键指向的是容器、或者存着的值读不成 `T` 时，`get<T>()` 都回答默认值——设置文件是手改的，
值不对或没有值都不算错误。

访问器会做的转换：

| 存着的是 | `get<bool>` | `get<整数>` | `get<double>` | `get<std::string>` |
|---------|---------|---------|---------|---------|
| bool | 原样 | `0` / `1` | `0.0` / `1.0` | `true` / `false` |
| 整数 | `!= 0` | 原样 | 转换 | 写出来 |
| 浮点 | `!= 0.0` | 截断 | 原样 | 写出来 |
| 字符串 | `true` / `false` / `1` / `0` | 内容是数字时 | 内容是数字时 | 原样 |
| 字节 | — | — | — | `base64:…` |
| null | — | — | — | — |

对字符串调用 `get<scl2::bytearray>()` 时，以 `base64:` 开头就解码，否则取文本本身的字节。

`remove()` 回答该键原先在不在；`clear()` 清空整个文档。

## 文件

```cpp
config.path();                       // 对象绑定的文件
config.provider();                   // 选中的格式

config.save();                       // 写出；父目录不存在会建
config.reload();                     // 丢掉内存里的内容，重新读文件

config.set_auto_save(false);         // 析构时不再写
config.set_instant_save(true);       // 每次改动都写
```

格式跟随扩展名：`.toml` 是 TOML，`.ini` 是 INI，`.xkeydb`（以及 `.xkdb`、`.db`）是 `xkeydb`
数据库，其余——包括没有扩展名——都是 JSON。接受 `settings_provider` 或 `settings_options` 的构造函数
可以覆盖这个判断。

不存在的文件、空文件、只有空白的文件都按空文档开始，所以包管理器或 `touch` 造出来的设置文件在写第一笔
之前就能用。本就没有内容的 JSON 文件第一次保存时用标准版式写出；之后保存保留文件自己的原文。

### xkeydb

```cpp
scl2::settings_options options;
options.provider = scl2::settings_provider::xkeydb;
options.secret   = scl2::bytearray("pass phrase");
options.integrity = scl2::xkeydb::integrity_algo::crc32;
options.compress  = scl2::compress_algo::zlib;

scl2::settings secure("secure.xkeydb", options);
```

- `secret` 用来解锁已有的文件，也用来加密新建的文件。缺少或错误的 secret 会抛异常。
- `compress` 与 `integrity` 只在**新建**文件时生效；已有文件的这两项从文件头读出，不受选项影响。
- 键按路径保存（`window/width`），因为数据库本身没有嵌套。

## 参考

### `settings_provider`

| 值 | 含义 |
|---------|---------|
| `automatic` | 由扩展名决定；默认值 |
| `json` | json2 |
| `toml` | TOML |
| `ini` | INI |
| `xkeydb` | `xkeydb` 数据库 |

### `settings_options`

| 字段 | 说明 |
|---------|---------|
| `provider` | 要用的格式；默认 `automatic` |
| `secret` | 仅 `xkeydb`：解锁已有文件，并加密新建文件 |
| `compress` / `integrity` | 仅 `xkeydb`：要新建的文件怎么编码、怎么校验 |

### `settings`

| 成员 | 说明 |
|---------|---------|
| `settings(path, provider)` | 打开或创建该文件 |
| `settings(path, options)` | 同上，带上 `settings_options` 里的选项 |
| `set(key, value)` | 存一个值；null 删除该键 |
| `set_default(key, value)` | 键还没有值时才存 |
| `get(key)` | 以 `settings_key` 给出值；没有时是 null |
| `get(key, default)` | 同上，缺失时给出默认值 |
| `get<T>(key, default)` | 把值读成 `T`；读不出时给出默认值 |
| `contains(key)` | 键是否指向某个东西 |
| `remove(key)` | 删除该键；回答它原先在不在 |
| `clear()` | 清空文档 |
| `setValue()` / `getValue()` | 与 `set()` / `get()` 相同 |
| `path()` / `provider()` | 对象绑定到哪里 |
| `save()` / `reload()` | 写出 / 重新读入文件 |
| `auto_save()` / `set_auto_save()` | 析构时是否写出（默认开） |
| `instant_save()` / `set_instant_save()` | 每次改动是否写出（默认关） |

`settings` 只能移动：一份拷贝就等于第二个对象绑在同一个文件上。

### `settings_key`

| 成员 | 说明 |
|---------|---------|
| `is_null()` / `is_bool()` / `is_int()` / `is_float()` / `is_string()` / `is_bytes()` | 它装着什么 |
| `type_name()` | 同上，文本形式 |
| `as<T>()` | 按上面的转换把值读成 `T` |
| `as_bool()` / `as_int64()` / `as_uint64()` / `as_double()` / `as_string()` / `as_bytearray()` | 同上，按类型取 |
| `as_trivial_class<T>()` / `as_nontrivial_class<T>()` | 把字节还原成类 |

### `globalsettings`

| 成员 | 说明 |
|---------|---------|
| `init(path, provider)` / `init(path, options)` | 创建唯一的实例；已存在时抛异常 |
| `reset()` | 销毁它；自动保存开着就先写一次 |
| `initialized()` | 它是否存在 |
| `instance_ref()` | 该实例；`init()` 之前调用抛 `std::logic_error` |

程序退出时该实例也会被销毁，所以即使从未调过 `reset()`，它的文件也会被写出。

## 行为说明

- **值不对时 `get<T>()` 不抛异常。** 它会给出默认值，所以被改坏的设置文件不会让程序停下。
- **指向对象、数组或表的键不是值。** `get(key)` 对它回答 null，而 `contains(key)` 仍然回答 true。
- **析构时会写文件。** 那里写失败会被吞掉——析构函数没有地方报告；在意的时候自己调 `save()`。
- **文本格式里的字节是文本。** JSON、TOML 与 INI 把 `scl2::bytearray` 存成 `base64:…`，
  `get<bytearray>()` 会解码回来。
- **TOML 没有 null。** null 值删除该键，与其他格式一致。
- **没有任何锁。** 一份对象同一时间只应给一个线程用。

## 尚未实现

- 列举某一层的键（类似目录遍历）。
- 分层设置：一个系统文件加一个用户文件，藏在同一个对象后面。
- 对已有文本的文件重排版：保存保留那段文本，而不是重新缩进。

## 相关模块

- [json2](json2.md) — `json` 后端背后的无损 JSON 文档
- [toml](toml.md) — `toml` 后端背后的 TOML 文档
- [ini](ini.md) — `ini` 后端背后的 INI 文档
- [xkeydb](xkeydb.md) — `xkeydb` 后端背后的键值数据库
- [bytearray](bytearray.md) — 字节值的类型
