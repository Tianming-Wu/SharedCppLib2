# pipe - 命名管道

+ Name: pipe
+ Namespace: `scl2::pipe`
+ 文档版本: `1.2.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `pipe` |
| Dependencies | `basic` |

引入方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::pipe)
```

```cpp
#include <SharedCppLib2/pipe.hpp>
```

## 简介

命名管道是本机两个进程之间的连接，用名字寻址，而不是地址加端口。不开放端口，从网络上
也访问不到，名字就是全部地址。

一个 `server` 持有这个名字，并为每个连接发下一个 `server_client`；`client` 连接到这个
名字。两端都是 `scl2::basic_iostream`，所以服务端和客户端用同一套读写调用。

管道传输的可以是纯字节流，也可以是内核替你保住边界的消息：在消息模式下，一次 `write()`
就是一条消息，`readAll()` 返回的正好是这条消息，不会是它的一半。

本模块仅限 Windows，因为它是基于命名管道实现的。在其他平台上，头文件不定义任何内容，
在构建时给出提示，并把 `SCL2_PIPE_SUPPORTED` 留为 0——可移植代码就检查这个宏：

```cpp
#include <SharedCppLib2/pipe.hpp>

#if SCL2_PIPE_SUPPORTED
    scl2::pipe::server server("my_app");
    ...
#endif
```

## 快速上手

服务端：
```cpp
#include <SharedCppLib2/pipe.hpp>

scl2::pipe::server server("my_app", scl2::pipe::permission_preset::Everyone);
server.setPipeMode(scl2::pipe::mode::Message);
server.setClientLimit(2);

if (!server.start()) {
    std::fprintf(stderr, "cannot create the pipe\n");
    return;
}

while (server.active()) {
    if (!server.waitForNextConnection(std::chrono::seconds(5)))
        continue;

    auto client = server.queryNextConnection();          // 已连接，并且重新开始监听
    if (!client.valid())
        continue;

    scl2::bytearray request = client.readAll();          // 一整条消息
    client.write(scl2::bytearray("Hello from server!"));

    client.waitForFinished();                            // 对端已经拿到了，现在断不会丢
    client.close();                                      // 这个连接结束
}
```

客户端：
```cpp
scl2::pipe::client client("my_app");

if (!client.connect(std::chrono::seconds(5))) {
    std::fprintf(stderr, "no server at that name\n");
    return;
}

client.write(scl2::bytearray("Hello from client!"));

if (client.waitForReadyRead(std::chrono::seconds(5)))
    scl2::bytearray reply = client.readAll();

client.waitForFinished();
client.close();
```

## 模型

| 部分 | 职责 |
|---------|---------|
| `server` | 持有名字、监听、接受连接 |
| `server_client` | 服务端侧的一个已接受连接。只能移动 |
| `client` | 发起连接的一端。只能移动 |
| `permissions` | 服务端创建管道时使用的安全描述符 |

连接是逐个接受的：`queryNextConnection()` 返回当前正等待的那个客户端，并立刻开始监听下一个，
所以接受动作从不会等已经在服务的那些连接。

这也就是“一连接一线程”不需要库做任何事的原因：每个已接受的 `server_client` 自己拥有句柄和缓冲
区，两个之间什么都不共享，服务端也不保留副本——返回出来的那个对象**就是**这条连接，而且它只能
移动，所以可以直接交给工作线程。必须留在接受线程上的是 `server` 本身：`queryNextConnection()`、
`stop()`、`cleanup()` 都会动它的成员。

## 参考

### server

| 成员 | 说明 |
|---------|---------|
| `server(name)` | 为该名字准备一个服务端。此时还没有创建任何东西 |
| `server(name, permissions)` | 同上，但显式给出安全描述符 |
| `start()` | 创建管道并开始监听。失败返回 `false` |
| `stop()` / `cleanup()` | 停止监听（同时释放它发出去的连接的等待），并关闭句柄 |
| `active()` | 是否正在监听 |
| `stopped()` | 是否已经停止 |
| `queryNextConnection()` | 等待并取走下一个客户端。没有时返回无效的 `server_client`。这个调用会阻塞，所以先用 `hasPendingConnection()` / `waitForNextConnection(timeout)` 判断是否值得调用 |
| `hasPendingConnection()` | 此刻是否有连接在等待 |
| `waitForNextConnection(timeout = 5s)` | 等到有为止 |
| `setBufferSize(size)` / `bufferSize()` | 每连接的 I/O 缓冲，默认 4 KiB |
| `setPermissions(p)` / `getPermissions()` | 新连接以哪个安全描述符创建 |
| `setPipeMode(m)` / `getPipeMode()` | 管道传输什么：新客户端会探测到的值 |
| `setClientLimit(n)` / `clientLimit()` | 同时允许多少个客户端连接 |
| `maximumClientLimit` | 254，`setClientLimit()` 能接受的最大值 |

`setBufferSize()`、`setPermissions()`、`setPipeMode()`、`setClientLimit()` 在服务端处于活动
状态时会抛出 `std::runtime_error`：这些值在创建管道时就定下了，只能在 `start()` 之前设置。

### server_client

| 成员 | 说明 |
|---------|---------|
| `valid()` | 连接是否仍然打开 |
| `broken()` | 是否已经有操作发现这条连接没了 |
| `setThrowOnBroken(enabled)` / `throwOnBroken()` | 失败时是否抛 `connection_broken`。默认关闭 |
| `cancel()` / `cancelled()` | 释放这条连接上的等待，以及是否被如此要求过 |
| `readyRead()` | 此刻是否有可读内容 |
| `waitForReadyRead(timeout = 5s)` | 等到有为止 |
| `available()` | 有多少字节在等待 |
| `read(bytes)` | 最多 `bytes` 字节。在 `mode::Message` 下返回空 |
| `readAll()` | 一整条消息；大于缓冲区时会自动重组 |
| `write(data)` | 一次写，或一条消息。返回写入的字节数 |
| `waitForFinished(timeout = 5s)` | 等到对端把目前已写出的内容全部读走，这样之后断开就不会丢东西。超时给负值表示一直等 |
| `close()` | 结束这个连接；本来就没连接时返回 `false`。对象之后仍可用，只是变成一个无效客户端 |
| `reset()` | 流接口里用来丢弃连接的名字：释放句柄、回到无效状态。本来就没东西可释放也返回 `true` |
| `bufferSize()` | 该连接使用的缓冲区；跟随服务端，不可单独设置 |
| `nativeHandle()` | 这条连接背后的原生句柄，以 `void*` 给出。句柄归连接所有：只能用来查看对端，不要关闭它。`GetNamedPipeClientProcessId()` 读它，`ImpersonateNamedPipeClient()` 借它的用户身份行事、记得配对 `RevertToSelf()` |

### client

| 成员 | 说明 |
|---------|---------|
| `connect(timeout = 5s)` | 连接到服务端，最多等待 `timeout`。名字还不存在、或所有实例都忙时会重试 |
| `waitForConnection(timeout = 5s)` | 与 `connect(timeout)` 相同；当服务端稍后才启动时，这个名字更能表达意图 |
| `serverExists()` | 此刻该名字上是否有东西在监听。只有名字不存在时才返回 `false` |
| `valid()` / `broken()` | 同 `server_client` |
| `setThrowOnBroken(enabled)` / `throwOnBroken()` | 同 `server_client` |
| `cancel()` / `cancelled()` | 同 `server_client`；客户端没有服务端，只有它自己的 cancel |
| `readyRead()` / `waitForReadyRead(timeout = 5s)` / `available()` | 同 `server_client` |
| `read(bytes)` / `readAll()` / `write(data)` | 同 `server_client` |
| `waitForFinished(timeout = 5s)` | 同 `server_client` |
| `pipeMode()` | 服务端声明的模式，由 `connect()` 填入 |
| `setPipeMode(m)` | 覆盖它，例如把消息管道按块读取 |
| `close()` | 关闭连接；本来就没连接时返回 `false` |
| `reset()` | 同 `server_client` |

### mode

| 取值 | 一次读取得到什么 |
|---------|---------|
| `Byte` | 字节流，按它实际到达的样子。`read(n)` 和 `readAll()` 都有意义 |
| `Message` | `readAll()` 返回的正好是一次 `write()`。`read()` 不返回任何内容 |
| `MessageChunk` | 内核模式和 `Message` 相同，但 `read(n)` 一次给出一块，最多一个缓冲区大小，重组由调用者负责。`readAll()` 仍然返回一整条消息 |

消息大于缓冲区本身不是问题：`readAll()` 会一直读并重组，直到消息结束。只有
`MessageChunk` 配合 `read()` 时，大消息剩下的部分才要你自己处理。

### permission_preset

| 取值 | 谁可以连接 |
|---------|---------|
| `None` | 不给显式描述符 |
| `Default` | 与 `None` 相同：交给 Windows 为进程设定的默认描述符，通常意味着管理员 |
| `Everyone` | 任何能到达这个名字的进程 |
| `SameSID` | 服务端运行所处的用户 |
| `Administrators` | 仅管理员与 LocalSystem，用 `D:(A;;GA;;;BA)(A;;GA;;;SY)`。用于普通通道所服务的那些进程不该够得着的特权通道 |

## 一个名字只允许一个服务端

`start()` 创建第一个管道实例时带上 `FILE_FLAG_FIRST_PIPE_INSTANCE`：如果这个名字已经被
别人服务着（同一进程里的另一个 server，或另一个进程），它就会失败 —— `start()` 返回
`false`，`GetLastError()` 为 `ERROR_ACCESS_DENIED` (5)。

之后为下一条连接创建的实例不带这个标志：那时名字已经是它的了。两个进程服务同一个名字，
意味着由操作系统决定客户端连到哪一个，这不是任何一边想要的。

## 连接断了

对端随时可能消失：它可能崩溃，而管道没有任何办法提前告诉你。每一个能察觉到的操作都会记下这件事，
而且一旦记下就不再清除：

| 调用 | 对端已消失时它返回什么 |
|---------|---------|
| `waitForReadyRead()` | `false`，和超时一模一样 |
| `read()` / `readAll()` | 空，或者断开前收到的那一部分 |
| `write()` | 0 |
| `waitForFinished()` | `false` |
| `available()` / `readyRead()` | 缓冲区里还剩多少；**只是看一眼也会记录状态** |
| `valid()` | 一旦 `broken()` 置位就是 `false` |

所以光看返回值分不出"还没有"和"再也没有了"：`broken()` 是区分它们的东西，值得在**失败之后**问一次
——而不是每次调用都问：

```cpp
if (!client.waitForReadyRead(std::chrono::seconds(5))) {
    if (client.broken()) {
        log("client is gone");            // 它崩了，或者自己关了
    } else {
        continue;                          // 只是超时
    }
}
```

最要紧的是被截断的消息：`readAll()` 会在返回已读到的内容**之前**就把状态记下，所以半条消息永远
不会被当成完整消息。

不想写任何检查的话，可以告诉这条连接改用抛异常：

```cpp
client.setThrowOnBroken(true);

try {
    while (running) {
        if (!client.waitForReadyRead(std::chrono::seconds(5)))
            continue;

        handle(client.readAll());
    }
} catch (const scl2::pipe::connection_broken& e) {
    log(e.what());                             // 里面带着 Win32 原文
}
```

`connection_broken` 派生自 `std::runtime_error`，默认关闭，而且是**按连接**设置的。默认关闭是因为
"对端死了"不是你代码的错误：没接住它的程序会死在本该处理这个失败的那一刻。确认自己就是想要
"handler 外面包一个 try"的场合，再把它打开。

## 取消等待

已经卡在 `waitForReadyRead()` 里的线程没法被通知退出——除非这个等待同时还在盯别的东西，`cancel()`
就是为此：

```cpp
client.cancel();                              // 它上面任何等待立刻返回 false
```

`server::stop()` 会对它发出去的每一条连接做这件事，于是关停不必等超时、也不必杀线程：

```cpp
// 接受循环，或者任何别的线程
server.stop();          // 每个工作线程的等待都结束，server.stopped() 变 true
```

cancel 是粘性的：连接会一直处于已取消状态，直到 `reset()`。它不关闭任何东西、从不抛异常，也是唯一
一个就是设计成给别的线程调用的接口。`waitForFinished()` 也会因为 cancel 而停止等待，但它那个
`FlushFileBuffers` 已经发出去了，Windows 没有取消它的手段——辅助线程会在对端读走或连接关闭后自己结束。

## 说明

> [!NOTE]
> **连接用 `close()` 来结束，而 `close()` 不会 flush。** 它会断开管道，而断开会把对端还没读
> 走的内容丢掉，所以“写完就 close”可能丢掉最后一条消息。析构函数做的也是同一件事。让断开变
> 得安全的是 `waitForFinished()`。

> [!NOTE]
> **`waitForFinished()` 是管道自己没给你的那个握手。** 它只在对方把目前已写出的内容全部读走后
> 返回，内核层面就是 `FlushFileBuffers` 对句柄做的事：谁读到了什么是内核在记，所以既不用往
> 数据流里塞任何东西，也不需要对方配合。

> [!WARNING]
> **两端同时 flush 会死锁。** 每次 flush 都在等对端读取，所以如果两边都还欠对方一次读取就开始
> flush，两边都不会返回。先把自己要读的读完，再 flush，最后 close。

> [!NOTE]
> **`broken()` 是惰性标记。** 它由失败的操作设置，且永不清除，所以只有在某次操作失败之后
> 它才有意义。不能用它去探测连接状态。

> [!NOTE]
> **flush 返回 `false` 不等于连接坏了。** 给了正超时时可能只是等到了时间（`waitForFinished()`
> 返回 `false`，连接照旧可用）；而在对端已经关掉的连接上 flush 会失败，并把它标记为 broken。

> [!NOTE]
> **`acknowledge()`、`waitForAcknowledged()` 和 `acknowledge_token` 已弃用。** 它们往管道里塞一
> 个标记字节，字节模式的读端会在自己的数据里看到它。由内核来说明“对端已经读走”，既简单又
> 不留痕迹。

> [!WARNING]
> **上限是 254 个客户端，不是 255。** Windows 限制同一个管道名的实例数为 255，而
> `queryNextConnection()` 在发下一个客户端、同时重新监听的那一瞬间会短暂持有两个实例。
> 所以 `maximumClientLimit` 是 254，这也是 `setClientLimit()` 能接受的最大值。

> [!NOTE]
> **`stop()` 也会释放它发出去的连接。** 它们各自持有服务端 stop 事件的一个副本，并在等待时盯着它，
> 所以卡在 `waitForReadyRead()` 里的工作线程会立刻返回，而不是等自己的超时；`waitForNextConnection()`
> 同时返回 `false`。这一步不关闭任何东西：怎么处理那条连接仍由工作线程决定。

> [!NOTE]
> **名字是全机器可见的，不是按用户隔离的。** 两个进程只有在名字完全一致时才能碰头；服务端
> 的身份不构成名字的一部分。谁可以连接由 `permissions` 决定。

## 相关模块

- [bytearray](bytearray.md) — `read()` 和 `write()` 搬运的内容
- [process](process.md) — 匿名管道，以及同一套 `scl2::basic_iostream` 模型，用于子进程
- [unixsocket](unixsocket.md) — 在 POSIX 上对应的事物：本地套接字而不是命名管道
