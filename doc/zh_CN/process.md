# process - 子进程管理

+ 名称: process
+ 命名空间: `scl2::process`
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `process` |

> [!NOTE]
> `process` 接口参考 Qt 的 `QProcess`，并按本库"同步、无事件循环"的风格做了取舍：
> 没有信号、没有回调，因此**搬运数据的是那些阻塞函数**（`waitForFinished` /
> `waitForReadyRead`），而不是事件循环。

包含方式：

```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::process)
```

```cpp
#include <SharedCppLib2/process.hpp>
```

## 描述

`process` 启动外部程序，并通过管道读写它的标准流。

+ 头文件：`process.hpp`；源文件：`process.cpp`
+ `scl2::process` 自身继承 `scl2::basic_iostream`：读走"当前读通道"（默认 stdout），写走 stdin
+ 每条管道由一个 `scl2::process_stream` 承载，可以**撕下来**自己持有
+ 平台：Windows 与 Unix（Linux / macOS）
+ 依赖：`basic`（`stringlist` / `bytearray` / `str_to_wstr`）；Windows 下另需 `platform`（`TranslateErrorW`）

> [!WARNING]
> Unix 侧与 Windows 侧是一起写完的，但**尚未在真机上验证**。API 与文档描述的行为两边一致；
> 在验证完成之前，请把两侧有差异的地方（`terminate`、`detach`、`startDetached`）当作暂定行为。

## 快速开始

```cpp
#include <SharedCppLib2/process.hpp>

scl2::process p("git", {"rev-parse", "HEAD"});

if (!p.start()) {
    std::fprintf(stderr, "启动失败: %s\n", p.errorString().c_str());
    return;
}

if (!p.waitForFinished(std::chrono::seconds(10))) {
    std::fprintf(stderr, "git 没有在限定时间内结束\n");
    return;
}

scl2::bytearray raw = p.readAllStandardOutput();
std::string hash(reinterpret_cast<const char*>(raw.data()), raw.size());
```

## 模型

### process 就是一个流

`scl2::process` 继承 `scl2::basic_iostream`，因此可以直接交给任何接受流的接口，不需要适配器：

| 操作 | 去向 |
|---|---|
| `read()`、`readAll()`、`available()`、`readyRead()` | 子进程的**当前读通道**（默认 stdout） |
| `write()` | 子进程的 **stdin** |

用 `setReadChannel()` 可以把读侧切到 stderr —— 这就是"用继承来的读函数读 stderr"的方式，
它和专用访问器（`readAllStandardError()` 之类）是两条路，见下面「缓冲」一节。

### 两个输出通道

`channel_mode` 决定子进程的 stdout / stderr 如何接线：

| 模式 | 含义 |
|---|---|
| `channel_mode::separate` | 各占一条管道。**这是默认值** |
| `channel_mode::merged` | stderr 被并入 stdout 管道。子进程自己的写入顺序被保留，但两个通道再也无法区分：`readAllStandardError()` 恒为空，`stderrStream()` 为 `nullptr` |

`setChannelMode()` 在子进程运行期间不生效。

### process_stream

每条管道交给一个 `process_stream`，它是一个只绑定**一个**原生句柄的 `basic_iostream`：
只读端（stdout / stderr）或只写端（stdin）。对只写流 `read()`、对只读流 `write()`，都
返回零字节而不是报错。

通道流可以**撕下来**：`detachStdError()`（以及 stdout / stdin 的同名函数）把 `shared_ptr`
交给你，此后 process 不再拥有该通道，具体表现是：

+ 不再被 `pump()` 抽空；
+ `waitForFinished()` / `waitForReadyRead()` 不再等待它；
+ 也不再由我们关闭。

process 侧只保留一个 `weak_ptr`，因此你一旦丢掉它，`stderrStream()` 就会返回 `nullptr`；
`attached(channel)` 回答的是"process 是否仍在管这条通道"。

## 参考

### 配置

| 成员 | 说明 |
|---|---|
| `setPath(path)` / `path()` | 程序路径。**裸名字**走 `PATH` 查找；带目录部分时要求文件存在 |
| `setArguments(stringlist)` / `arguments()` | 参数，一个元素一个参数 |
| `setWorkingDirectory(path)` / `workingDirectory()` | 子进程的工作目录。留空表示"继承我们的" |
| `setChannelMode(mode)` / `channelMode()` | 见上；运行期间调用无效 |

### 生命周期

| 成员 | 说明 |
|---|---|
| `start()` | 启动。失败返回 `false` 并写入 `error()` |
| `static startDetached(program, args, working_dir)` | 不接任何管道地启动并撒手，子进程活得比调用更长 |
| `detach()` | 释放我们的句柄、让子进程继续跑（管道被关闭，子进程会看到 EOF） |
| `terminate()` | 请求子进程停止，并确保它真的停。Windows：`TerminateProcess`。Unix：`SIGTERM`，几秒后升级为 `SIGKILL` |
| `kill()` | 不给清理机会地终止子进程。Unix：`SIGKILL`。Windows：与 `terminate()` 相同 |
| `running()` / `finished()` | 状态查询，二者都是 `const` |
| `exitcode()` | 退出码；未退出返回 `-1`。Unix 上被信号杀死的子进程同样返回 `-1` |
| `processId()` | pid；从未启动返回 `0` |
| `error()` / `errorString()` | 最近一次 `start()` 或等待函数失败的原因 |
| `reset()` | 必要时终止子进程，然后关闭一切、清空缓冲：对象回到刚构造的状态，可以再次 `start()`。幂等，且**不清除** `error()` |

子进程还活着时调用 `start()` 会以 `error_code::already_running` 失败，先
`waitForFinished()` 或 `reset()`。`process` 只可移动；析构时若子进程仍在运行会被杀掉。

### 读

| 成员 | 说明 |
|---|---|
| `setReadChannel(channel)` / `readChannel()` | 继承来的读函数使用哪条通道。`merged` 模式下无效 |
| `readyRead()` / `available()` / `read(bytes)` / `readAll()` | 作用于当前读通道 |
| `readyReadStandardOutput()` / `readyReadStandardError()` | 按通道查询 |
| `availableStandardOutput()` / `availableStandardError()` | 按通道查询 |
| `readAllStandardOutput()` / `readAllStandardError()` | 按通道取走 |

### 写

| 成员 | 说明 |
|---|---|
| `write(bytearray)` | 写入子进程 stdin；返回实际写入的字节数 |
| `closeWriteChannel()` | 关闭子进程 stdin，让它看到 EOF |

### 通道流

| 成员 | 说明 |
|---|---|
| `stdinStream()` / `stdoutStream()` / `stderrStream()` | 该通道的 `shared_ptr`；不存在（stderr 被合并）或已被撕下时为 `nullptr` |
| `detachStdInput()` / `detachStdOutput()` / `detachStdError()` | 接管一条通道 |
| `attached(channel)` | process 是否仍拥有并抽空该通道 |

### 等待

| 成员 | 说明 |
|---|---|
| `waitForFinished(timeout = 5s)` | 等子进程退出，同时把它的输出搬进我们的缓冲 |
| `waitForReadyRead(timeout = 5s)` | 等当前读通道有数据 |

两者在等待期间都会**同时排空两条输出通道**，所以话多的子进程不会因为管道写满而死锁。
没有事件循环也没有信号，只能靠这些函数或 `readyRead()` 轮询。

## 缓冲

一条管道只有约 64KB。假如 `waitForFinished()` 只是干等子进程，那么任何输出超过这个量的程序
都会阻塞在写满的管道上、永远不结束 —— 所以 process 在等待期间把两条通道都抽进各自的缓冲，
读函数再从缓冲里取。

需要知道的副作用是：**process 拥有某条通道时，它同时也在缓冲这条通道。** 因此手工从
`stdoutStream()` 读，会和 `readAllStandardOutput()` 抢同一批字节。二选一：

+ 用 process 层的读函数；或
+ 先 `detachStdOutput()`，再自己驱动那个流。

`available()` 与 `readyRead()` 会先把管道里的数据抽进缓冲，所以不调用等待函数也能轮询。

## 错误处理

`start()` 与等待函数通过 `error()` 报告失败，不抛异常：

| `error_code` | 含义 |
|---|---|
| `no_error` | 尚无失败 |
| `failed_to_start` | 程序没能启动（文件不存在、`PATH` 里找不到、没有权限……） |
| `already_running` | 子进程还在运行时又调用了 `start()` |
| `timed_out` | 等待函数超时 |
| `write_error` | 无法写入（或只能部分写入）子进程 stdin |
| `read_error` | 读取子进程输出失败 |
| `unknown_error` | 其它 |

`errorString()` 里是平台自己的错误文本（Windows 上是 Win32 错误消息，Unix 上是
`strerror` 的结果）。

## 平台差异

| 操作 | Windows | Unix |
|---|---|---|
| 管道 | 匿名管道，`PeekNamedPipe` / `WaitForMultipleObjects` | `pipe()` + `FIONREAD` / `poll()` |
| `terminate()` | `TerminateProcess`，无法被拒绝 | `SIGTERM`，几秒后升级 `SIGKILL` |
| `kill()` | 与 `terminate()` 相同 | `SIGKILL` |
| 程序查找 | `CreateProcessW`，裸名字走 `PATH` | `execvp`，规则相同 |
| 参数传递 | 用 `stringlist::pack()` 压成一条命令行（逆操作是 `unpack()`） | 直接传真正的 `argv` 数组，完全不涉及引号 |
| `startDetached()` | 不继承任何管道，子进程继续共用我们的控制台 | 双重 fork + `setsid()`，子进程被 init 收养 |
| `detach()` | 关掉我们的句柄，子进程继续跑 | 此后没人 reap，子进程退出后会变僵尸 —— 需要的话自己拿 `processId()` 去 wait |
| SIGPIPE | 不存在 | 每次写入都屏蔽，子进程关掉 stdin 后不会把我们一并带走 |

## 注意事项

+ 子进程 stdin 管道写满（约 64KB）时 `write()` 会阻塞。没有异步写入：不读输入的子进程会拖住调用方。
+ `detach()` 之后，子进程既不能等、不能读、也不能终止，退出码也拿不到了。
+ process 层的 `readAll()` 只取当前读通道 —— 这正是那些按通道访问器存在的原因。
+ 被移动过的 `process` 源对象是惰性的：没有 pid、既不 running 也不 finished，析构时也不会碰子进程。

## 限制与路线图

+ 没有回调/信号；没有事件循环可以投递它们。
+ 没有 `setEnvironment()`，不能把输出重定向到文件，也不支持通道转发（让子进程直接共用我们的控制台）。
+ `exitcode()` 区分不了"被信号杀死"和"尚未退出" —— Unix 上两者都是 `-1`。
+ Unix 侧还等着第一次真机验证。

## 相关模块

- [bytearray](bytearray.md) —— `read()` / `write()` 搬运的东西
- [stringlist](stringlist.md) —— 参数列表，以及 Windows 下拼命令行用的 `pack()` / `unpack()`
- [standalone_module](standalone_module.md) —— 独立模块是什么；`process` 不是
