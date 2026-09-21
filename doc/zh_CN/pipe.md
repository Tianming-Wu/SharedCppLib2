# pipe - 命名管道

+ Name: pipe
+ Namespace: `scl2::pipe`
+ Document Version: `1.0.0`

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
server.setMaxClients(2);

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

client.close();
```

## 模型

| 部分 | 职责 |
|---------|---------|
| `server` | 持有名字、监听、接受连接 |
| `server_client` | 服务端侧的一个已接受连接。只能移动 |
| `client` | 发起连接的一端。只能移动 |
| `permissions` | 服务端创建管道时使用的安全描述符 |

连接是逐个接受的：`queryNextConnection()` 返回当前正等待的那个客户端，并立刻开始监听下一个。
想让一个服务端同时服务多个客户端的做法，是把每个返回的 `server_client` 放到各自的线程里——
不同的 `server_client` 之间是互相独立的。

## 参考

### server

| 成员 | 说明 |
|---------|---------|
| `server(name)` | 为该名字准备一个服务端。此时还没有创建任何东西 |
| `server(name, permissions)` | 同上，但显式给出安全描述符 |
| `start()` | 创建管道并开始监听。失败返回 `false` |
| `stop()` / `cleanup()` | 停止监听，丢弃客户端并关闭句柄 |
| `active()` | 是否正在监听 |
| `stopped()` | 是否已经停止 |
| `clientCount()` | 当前持有的客户端数量 |
| `queryNextConnection()` | 等待并取走下一个客户端。没有时返回无效的 `server_client`。这个调用会阻塞，所以先用 `hasPendingConnection()` / `waitForNextConnection(timeout)` 判断是否值得调用 |
| `hasPendingConnection()` | 此刻是否有连接在等待 |
| `waitForNextConnection(timeout = 5s)` | 等到有为止 |
| `setBufferSize(size)` / `bufferSize()` | 每连接的 I/O 缓冲，默认 4 KiB |
| `setPermissions(p)` / `getPermissions()` | 新连接以哪个安全描述符创建 |
| `setPipeMode(m)` / `getPipeMode()` | 管道传输什么：新客户端会探测到的值 |
| `setMaxClients(n)` / `getMaxClients()` | 同时允许多少个客户端连接 |
| `unlimitedClients` | 254，`setMaxClients()` 能接受的最大值 |

`setBufferSize()`、`setPermissions()`、`setPipeMode()`、`setMaxClients()` 在服务端处于活动
状态时会抛出 `std::runtime_error`：这些值在创建管道时就定下了，只能在 `start()` 之前设置。

### server_client

| 成员 | 说明 |
|---------|---------|
| `valid()` | 连接是否仍然打开 |
| `broken()` | 在这个已经断掉的连接上是否发生过失败的操作 |
| `readyRead()` | 此刻是否有可读内容 |
| `waitForReadyRead(timeout = 5s)` | 等到有为止 |
| `available()` | 有多少字节在等待 |
| `read(bytes)` | 最多 `bytes` 字节。在 `mode::Message` 下返回空 |
| `readAll()` | 一整条消息；大于缓冲区时会自动重组 |
| `write(data)` | 一次写，或一条消息。返回写入的字节数 |
| `acknowledge()` | 发送确认标记 |
| `waitForAcknowledged(timeout = 5s)` | 等待对端的确认标记 |
| `close()` | 结束这个连接。对象之后仍可用，只是变成一个无效客户端 |
| `cleanup()` | 与 `close()` 相同 |
| `bufferSize()` | 该连接使用的缓冲区；跟随服务端，不可单独设置 |

### client

| 成员 | 说明 |
|---------|---------|
| `connect(timeout = 5s)` | 连接到服务端，最多等待 `timeout`。名字还不存在、或所有实例都忙时会重试 |
| `waitForConnection(timeout = 5s)` | 与 `connect(timeout)` 相同；当服务端稍后才启动时，这个名字更能表达意图 |
| `serverExists()` | 此刻该名字上是否有东西在监听。只有名字不存在时才返回 `false` |
| `valid()` / `broken()` | 同 `server_client` |
| `readyRead()` / `waitForReadyRead(timeout = 5s)` / `available()` | 同 `server_client` |
| `read(bytes)` / `readAll()` / `write(data)` | 同 `server_client` |
| `acknowledge()` / `waitForAcknowledged(timeout = 5s)` | 同 `server_client` |
| `pipeMode()` | 服务端声明的模式，由 `connect()` 填入 |
| `setPipeMode(m)` | 覆盖它，例如把消息管道按块读取 |
| `close()` / `cleanup()` | 关闭连接。两者相同 |

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

## 说明

> [!NOTE]
> **连接用 `close()` 来结束。** 它会断开管道，于是对端看到的是连接结束，而不是一次永远
> 到不了的写入。析构函数也会做同样的事，但显式关闭说明了时机。

> [!NOTE]
> **`acknowledge()` 存在是因为管道不会 flush。** 内核会缓冲写入，直到对端读取它；一个写
> 完就立刻消失的客户端会把最后一条消息一起带走。发送确认标记并在另一端等待它，就让"发送
> 方活得比数据短"这类竞争不可能发生：客户端在关闭前就知道服务端已经读到了这条消息。

> [!NOTE]
> **`broken()` 是惰性标记。** 它由失败的操作设置，且永不清除，所以只有在某次操作失败之后
> 它才有意义。不能用它去探测连接状态。

> [!WARNING]
> **上限是 254 个客户端，不是 255。** Windows 限制同一个管道名的实例数为 255，而
> `queryNextConnection()` 在发下一个客户端、同时重新监听的那一瞬间会短暂持有两个实例。
> 所以 `unlimitedClients` 是 254，这也是 `setMaxClients()` 能接受的最大值。

> [!NOTE]
> **名字是全机器可见的，不是按用户隔离的。** 两个进程只有在名字完全一致时才能碰头；服务端
> 的身份不构成名字的一部分。谁可以连接由 `permissions` 决定。

## 相关模块

- [bytearray](bytearray.md) — `read()` 和 `write()` 搬运的内容
- [process](process.md) — 匿名管道，以及同一套 `scl2::basic_iostream` 模型，用于子进程
- [unixsocket](unixsocket.md) — 在 POSIX 上对应的事物：本地套接字而不是命名管道
