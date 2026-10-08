# resourced - 程序的资源放在哪里

+ 名称: resourced
+ 命名空间: `scl2`
+ 头文件: `<SharedCppLib2/resourced.hpp>`
+ 文档版本: `1.1.0`

## CMake 配置信息

| 项 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库 | `resourced` |
| 依赖 | `basic`、`orderedmap` |

引入方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::resourced)
```

```cpp
#include <SharedCppLib2/resourced.hpp>
```

`find_package(SharedCppLib2)` 同时提供 `scl2_add_resources()`，一个资源目录就是靠它变成二进制
里的包的。见[打包](#打包)。

## 描述

**资源**是程序需要、但不是它算出来的具名字节块 —— 图标、语言包、着色器、模板、安装器载荷。这个
模块只回答一个问题：给定一个名字，字节从哪里来。这些字节是什么意思，是调用方的事。

来源是**分层的**。管理器持有一串已挂载的来源，每个都有自己的名字，查找会依次问过去，直到有一个
手里有这个资源。后挂载的优先被查，所以放在程序旁边的包会压过程序自带的，最后挂上去的目录又压过
那两者。`info()` 会告诉你最后是哪个来源回答的，所以一次查找可以被解释，而不是靠猜。

包**不压缩、不加密**。加密 API 和压缩 provider 收的都是 `bytearray`，而一个包就是 `bytearray`，
所以需要其中任何一项的调用方都能自己施加，无需本模块参与。

## 快速开始

```cpp
#include <SharedCppLib2/resourced.hpp>
#include "myapp_resources.hpp"    // 由 scl2_add_resources() 写出，见下文

scl2::ResourceManager res;

// 1. 二进制里的那一份：在只读内存里就地解析，不拷出去。
res.mount("builtin", *scl2::ResourceView::parse(myapp_resources::resources()));

// 2. 放在程序旁边的包文件（如果有）。优先于 "builtin"。
res.mount_pack_file("sidecar", "app.pack");

// 3. 散文件的目录，开发时用。优先于前两者。
res.mount_directory("dev", "res/");

// 按名字取。get() 对每一种来源都可用。
if (const auto bytes = res.get(myapp_resources::k_i18n_zh_CN_toml))
    load_language(*bytes);

// view() 是不拷贝的同一份字节，只在该来源把它们放在内存里时成立。
if (const auto bytes = res.view(myapp_resources::k_app_ico))
    scl2::set_window_icon(hwnd, scl2::to_hicon(*bytes, 32, 32));
```

## 模型

### 来源

`ResourceType` 说明一个来源是怎么存放字节的。它是**挂载方式**的属性，不是包的属性：同一个包，字节
在二进制里时是 `Embedded`，在文件里时是 `ExternalPacked`。

| `ResourceType` | 挂载方式 | 字节怎么到手上 |
|---------|---------|--------------|
| `Embedded` | `mount(name, ResourcePack)` 或 `mount(name, ResourceView)` | 来自内存；`view()` 给出指向它的指针 |
| `ExternalFile` | `mount_directory(name, dir)` | 每次查找都从磁盘读 |
| `ExternalPacked` | `mount_pack_file(name, file)` | 挂载时只读目录，条目按需从文件读 |
| `PlatformEmbedded` | — | 保留；尚无任何东西产生它 |
| `SystemResource` | — | 保留；尚无任何东西产生它 |
| `Null` | — | 未知，或还没查过 |

挂载不需要任何预处理。运行时挂上的来源与编译进程序的来源没有区别：没有任何东西需要事先注册，程序
构建之后才出现的文件，`mount_directory()` 一样能找到。

一个来源名只能挂载一次。名字已被占用、目录不存在、或文件读不成包时，`mount*()` 返回 `false`。

### 包格式

所有整数都是小端，所以在一台机器上写出的包在另一台上能读。

| 偏移 | 长度 | 字段 |
|---------|---------|-------|
| 0 | 8 | 魔数：`SCL2RES\0` |
| 8 | 4 | 格式版本：`1` |
| 12 | 4 | 资源个数 |
| 16 | — | 目录：`count` 项，每项为 `名字长度 (4)`、`名字字节`、`长度 (8)` |
| — | — | 载荷：每个条目的数据，顺序同目录 |

目录**就是**索引。条目偏移不存：一个条目的起点就是前一个的终点，于是写出只需一遍，目录也保持很
小。需要偏移的地方从它前面的长度累加，而一次查找是直接跳到那些字节，不会走过载荷。

资源名是文件在资源目录内的路径，用正斜杠 —— `res/i18n/zh_CN.toml` 对应 `i18n/zh_CN.toml`。没有任
何东西需要翻译它。

## 参考

### scl2_add_resources

由 `find_package(SharedCppLib2)` 提供。把一个目录打成包，并把结果挂到目标上。

```cmake
scl2_add_resources(<target> DIR <资源目录>
                   [NAMESPACE <ns>] [STEM <name>] [OUT_DIR <dir>] [LINK <库> ...])

scl2_add_resources(<target> GENERATED <已生成目录> [STEM <name>] [LINK <库> ...])
```

`DIR` 打包该目录并挂上生成的源码。`STEM` 与 `NAMESPACE` 都默认取目标名，所以
`scl2_add_resources(myapp DIR res/)` 写出 `myapp.hpp` / `myapp.cpp`，常量在 `namespace myapp` 里。

生成的头文件里，每个资源一个常量，外加一个访问器：

```cpp
namespace myapp {
inline constexpr std::string_view k_i18n_zh_CN_toml = "i18n/zh_CN.toml";
std::span<const std::byte> resources();
}
```

名字从生成头里拿，正是让打错的资源名变成编译错误的那一步。生成的源码只依赖 `<cstddef>`、
`<span>` 和 `<string_view>`。

`GENERATED` 只挂上 `<已生成目录>/<stem>.cpp` 与 `.hpp`，自己不跑任何东西，对应"构建过程跑不了那个
工具"的情况。见[限制与路线图](#限制与路线图)。

### ResourcePack

自有的容器。用来建包、写出包，或持有一个读进来的包。

| 成员 | 说明 |
|---------|--------------|
| `add(name, data)` | 追加一个资源；名字被占用返回 `false`，名字为空或过长抛 `std::invalid_argument` |
| `replace(name, data)` | 覆盖，名字是新的就追加；已存在的条目不会移动位置 |
| `erase(name)` | 删除一个；本来就不在返回 `false` |
| `contains(name)` / `size()` / `empty()` | 查询 |
| `names()` | 所有名字，按包内顺序 |
| `view(name)` / `get(name)` | 取字节，分别是指向包内的视图 / 一份拷贝 |
| `info(name)` | 名字、长度，以及它在包里的偏移 |
| `dump()` / `dump_to(out)` | 序列化成 `bytearray`，或写进任意 `std::ostream` |
| `load(data)` / `load(stream)` | 解析；任何格式不对抛 `std::runtime_error` |

对 `bytearray` 的 `load()` 从它的读游标开始读，读完把游标留在包的末尾之后。对流的 `load()` 恰好读
到包的结尾。`load()` 失败时包保持原样。

### ResourceView

给"已经在内存里的包"用的读取器。它解析目录，什么都不拷贝：`view()` 返回指向那块内存的
`bytearray_view`。

```cpp
std::optional<scl2::ResourceView> ResourceView::parse(std::span<const std::byte> blob) noexcept;
```

| 成员 | 说明 |
|---------|--------------|
| `parse(blob)` | 不是包的块、或目录越过块尾，返回空 |
| `contains(name)` / `size()` / `empty()` | 查询 |
| `names()` | 所有名字，按包内顺序 |
| `view(name)` | 字节，指向那块内存 |
| `get(name)` | 字节，拷出来 |
| `info(name)` | 名字、长度、偏移 |
| `blob()` | 解析所依据的那段内存 |

### ResourceManager

分层查找。管理器在自己活着的时候注册自己，所以下面这些调用是静态的，都指向那一个管理器；写在
对象上的 `res.mount(...)` 到达的是同一个地方。

| 调用 | 说明 |
|---------|--------------|
| `mount(source, pack)` | 挂载一个自有 `ResourcePack` |
| `mount(source, view)` | 挂载一个 `ResourceView`；那块内存必须活得比它久 |
| `mount_directory(source, dir, recursive = true)` | 挂载一个装普通文件的目录 |
| `mount_pack_file(source, file)` | 挂载存放在文件里的包 |
| `unmount(source)` | 移除一个来源；本来就没挂返回 `false` |
| `clear()` | 移除全部来源 |
| `contains(name)` | 是否有东西能回答 |
| `view(name)` | 无拷贝取字节；来源给不出视图时是 `std::nullopt` |
| `get(name)` | 拷贝取字节；任何来源都能用 |
| `info(name)` | 名字、来源、类型、长度、偏移 |
| `names()` | 所有名字，每个只报一次，按查找顺序 |
| `sources()` | 已挂载的来源名，按挂载顺序 |

管理器不允许拷贝：拷贝出来的会是第二个注册表，而没有任何东西会转发到它。

### 默认注册表

```cpp
int main() {
    scl2::ResourceManager res;                       // 自己注册自己，活到程序结束
    scl2::res::mount_builtin(myapp::resources());    // 解析并挂载，一行

    // …… 程序里任何别的地方
    if (const auto bytes = scl2::res::get(myapp::k_config_toml))
        use(*bytes);
}
```

`scl2::res` 转发到程序构造的那个管理器：`manager()`、`has_manager()`、
`mount_builtin(pack, source)`、`mount`、`mount_directory`、`mount_pack_file`、`unmount`、
`contains`、`get`、`view`、`info`、`names`、`sources`，以及给文本用的 `text(name)`。

写在 `ResourceManager` 本身上的那些调用到达的是同一个注册表，所以两种写法可以互换。管理器还不
存在时就索取，会抛 `std::logic_error`，而不是回答“没找到” —— `main()` 还没构造就调用，是程序的
错误，不是资源缺失。一个程序一个管理器 —— 这里没有任何东西能到达第二个。

### Windows：字节变成平台对象

```cpp
void* to_hicon(const scl2::bytearray_view& image, int cx = 0, int cy = 0);
bool set_window_icon(void* hwnd, void* icon);
```

句柄用普通指针传递，为的是这个头文件不必引入 `<windows.h>` —— 它把 `small`、`near`、`far` 定
义成宏，引入它的头文件会把这些宏带进每一个使用本模块的翻译单元。`HWND` 或 `HICON` 与普通指针双
向隐式转换，所以调用方照常写真实类型即可。

`to_hicon()` 从内存造出 `HICON` —— 没有 `.rc` 文件、没有资源节、没有磁盘文件参与，所以图标可以来
自本模块能挂载的任何来源。它既接受完整的 `.ico`，也接受单个图标图像块；给的是多尺寸 `.ico` 时，
它会挑最贴合 `cx`/`cy` 的那一帧，传 `0` 表示"保持原样"。返回值归调用方所有，交给 `DestroyIcon()`。

`set_window_icon()` 会同时设置 `ICON_BIG` 与 `ICON_SMALL` 两个槽位，标题栏和任务栏按钮都覆盖到。

可执行文件自身显示的那个图标存在 PE 资源节里，只有资源编译器能写；本模块不碰它。

## 行为说明

- **错误。** 调用方要的东西可能不存在，一律以 `std::optional` 返回。格式不对抛
  `std::runtime_error`；传给 `add()` / `replace()` 的名字为空或过长抛 `std::invalid_argument`。
  `ResourceView::parse()` 从不抛异常 —— 用不了的块就是 `std::nullopt`。
- **`view()` 与 `get()`。** `get()` 永远可用，返回一份拷贝。`view()` 返回属于来源的那些字节，只在
  来源本来就把它们放在内存里时才提供：自有包与内存包给视图，目录与包文件不给。拷贝本身就是实打实
  开销的地方去问 `view()`，拿不到再退回 `get()`。
- **生命周期。** `ResourceView` 借用它所解析的那块内存：那块内存必须活得比它久，且不能被移动。挂在
  某个名字下的来源由管理器持有，只有视图例外 —— 它依据的内存是调用方的。
- **读包文件。** 内存里只留目录，每个条目在被索取时才从文件读出。所以大于内存的包也能用，只要单个
  条目装得下。
- **线程。** `ResourceManager` 不做同步：一个线程在挂载而另一个在查找就是数据竞争。挂载结束之后，
  多线程查找是安全的，只有一处例外 —— 从目录或包文件读条目时，那次调用自己开文件、自己关，没有共
  用的游标。
- **目录来源的资源名。** 名字是相对该目录的路径。想离开该目录的名字 —— 绝对路径，或含 `..` —— 会
  被拒绝，而不是照做。

## 注意事项

- 资源名就是一个普通字符串。这里没有保留任何前缀或语法：查找是一次调用，不是虚拟文件系统，所以名
  字永远不需要被标记成"资源路径"。
- 资源解析出来的就是普通字节 —— `scl2::bytearray` 或 `scl2::bytearray_view`。它们可以交给任何接受
  它的读取器，可以架在 `std::ispanstream` 后面当作 `std::istream` 读，也可以当成文本来查看。
- 包里的条目可以是任意大小；同一个名字可以同时存在于多个已挂载的来源里，由查找顺序决定谁回答。
- `info()` 给出条目在它容器内的偏移 —— 包，或包文件。目录没有可偏移的容器，所以那里给出 `0`。

## 限制与路线图

- **不压缩、不加密、不校验。** 包就是一个朴素容器；这三件事都由本模块之外施加到字节或整个包上。
- **没有平台资源来源。** `ResourceType::PlatformEmbedded` 与 `SystemResource` 是保留值，尚无任何东
  西产生它们。
- **只有图标，没有光标和字体。** `CreateIconFromResourceEx()` 只造图标。
- **资源打包在交叉编译时不可用。** `scl2_add_resources()` 用消费方的编译器编它的工具，所以在交叉
  工具链下那个工具没法在构建机上跑。出口是 `GENERATED` 形态与包里 `bin/` 下的 resgen 可执行文件：
  让工具在它能跑的地方跑，把结果挂上来。这个库本来就不预期用在交叉编译的项目里 —— Android 有自己
  的资源系统，Windows 和 Linux 原生编译很容易 —— 所以这是一条明说的限制，而不是待办。
- **包文件按条目读。** 没有任何一层缓存条目，所以热循环里读一个资源会每次都读磁盘；需要那样的调用
  方应该自己持有那些字节。

## 相关模块

- [encoding](encoding.md) —— 进入资源的文本，以及它抵达控制台时要跨过的边界
- [toml](toml.md) —— 可以放进包里的配置或语言文件
- [standalone_module](standalone_module.md) —— 模块怎么声明它需要的标准
- [autofetch](cmake/autofetch.md) —— 把一个 SharedCppLib2 拉进项目
