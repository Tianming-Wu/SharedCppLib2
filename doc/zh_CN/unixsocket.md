# unixsocket - 本地（AF_UNIX）套接字

+ 名称: unixsocket
+ 命名空间: `scl2::posix`
+ 文档版本: `1.0.0`

## CMake 信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库 | `unixsocket` |
| 依赖 | `basic` |

引入方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::unixsocket)
```

```cpp
#include <SharedCppLib2/unixsocket.hpp>
```

## 描述

本地套接字用名字寻址，而不是地址加端口，所以完全不涉及地址处理和域名解析：名字本身就是
全部地址。同一台机器上的两个进程可以靠它通信，既不需要端口，也不用把网络栈牵扯进来。

`unix_stream` 是一个 `scl2::transport_interface`，所以任何接受传输层的协议层（比如 HTTP
客户端）不用改动就能跑在本地套接字上。`unix_server` 是监听的一端，负责交出 `unix_stream`。

本模块仅限 POSIX：Linux、Android、macOS 以及各种 BSD。在 Windows 上，以及任何头文件不认识
的平台上，它什么都不定义，编译时给出提示，并把 `SCL2_UNIXSOCKET_SUPPORTED` 留为 0 ——
需要写可移植代码时判断的就是这个宏：

```cpp
#include <SharedCppLib2/unixsocket.hpp>

#if SCL2_UNIXSOCKET_SUPPORTED
    scl2::posix::unix_stream stream;
    ...
#endif
```

## 快速开始

服务端：
```cpp
#include <SharedCppLib2/unixsocket.hpp>

scl2::posix::unix_server server;

if (!server.listen("/run/app.sock", 8, 0660)) {
    std::fprintf(stderr, "listen: %s\n", scl2::posix::errorText(server.lastError()).c_str());
    return;
}

if (auto client = server.accept(std::chrono::seconds(5))) {
    client->write(scl2::bytearray("hello"));

    if (client->waitForReadyRead(std::chrono::seconds(1)))
        scl2::bytearray request = client->readAll();
}
// `client` 离开作用域，连接就结束
```

客户端：
```cpp
scl2::posix::unix_stream stream;

if (!stream.connect("/run/app.sock")) {
    std::fprintf(stderr, "connect: %s\n", scl2::posix::errorText(stream.lastError()).c_str());
    return;
}

stream.write(scl2::bytearray("ping"));

if (stream.waitForReadyRead(std::chrono::seconds(1)))
    scl2::bytearray reply = stream.readAll();
```

## 名字

| 名字 | 是什么 |
|---------|---------|
| `/run/app.sock` | 普通路径。内核会在那里建一个套接字文件，谁能连由这个文件的权限决定 |
| `@app` | 抽象命名空间（Linux 与 Android）：没有文件、不需要权限，进程结束时名字也跟着消失。以 `'\0'` 开头的名字与它等价 |

路径必须放得进 `sockaddr_un::sun_path`（Linux 上 108 字节），抽象名字还要给标记让出一个字节。
名字过长会报 `ENAMETOOLONG`，不会被截断。

## 接口

### unix_stream

| 成员 | 说明 |
|---------|---------|
| `connect(name)` | 连到路径或 `@name`。失败返回 `false`，原因在 `lastError()` |
| `connect(address, port)` | `transport_interface` 的形式：名字放在 `address`，端口不使用 |
| `disconnect()` | 关闭读写并释放描述符；没东西可关时什么都不做 |
| `is_connected()` / `valid()` | 是否持有描述符。对端关掉了它，这里看不出来 |
| `readyRead()` | 现在有没有东西可读：数据，或者对端已关闭 |
| `waitForReadyRead(timeout = 5s)` | 等它变成可读 |
| `available()` | 还有多少字节可读（`FIONREAD`） |
| `read(bytes)` | 最多读 `bytes` 个字节；没读到就是空。空结果不是错误 |
| `readAll()` | 取走当前所有可读的内容 |
| `write(data)` | 一次 `send`；返回实际发出的字节数，一个都没发出时为 0 |
| `reset()` | 断开连接，并返回 `true` |
| `native_handle()` | 描述符，没有时为 `-1`，供调用方自己做 `poll()` / `select()` |
| `lastError()` | 上一次失败调用的系统错误码；成功之后为 0 |

### unix_server

| 成员 | 说明 |
|---------|---------|
| `listen(name, backlog = 8, mode = 0)` | 创建、绑定并开始监听。`mode` 是套接字文件的权限（Android 服务通常要 `0660`），传 0 则由 umask 决定；抽象名字不用它 |
| `close()` | 停止监听，并在文件是本对象创建的时候删掉它 |
| `is_listening()` | 是否在监听 |
| `accept(timeout = 5s)` | 接受一条连接，最多等 `timeout`。超时或失败都返回 `nullptr` |
| `tryAccept()` | 同上，但不等 |
| `name()` | 打开时用的名字，没在监听时为空 |
| `native_handle()` / `lastError()` | 与 stream 上的一样 |

## 注意

> [!NOTE]
> **废弃的套接字文件会被删掉，活着的不会。** `listen()` 会先试着连一下那个路径：如果有人应答，
> 说明服务还在跑，于是返回 `EADDRINUSE` 且文件保留；没人应答，说明是某个已经结束的进程留下的，
> 才会删掉。`close()` 也只删套接字，如果那个路径期间被换成了别的东西，它不会被碰。

> [!NOTE]
> **超时不是错误。** 没人来的时候 `accept()` 返回 `nullptr`，`lastError()` 是 `ETIMEDOUT`。
> 想表达"有人等着才接"，`tryAccept()` 更清楚。

> [!NOTE]
> **一条连接是字节流，不是消息流。** 两次写可能被一次读走，反过来也一样；有消息边界的协议
> 得自己成帧。

> [!WARNING]
> **这里不会因为对端没了而触发 SIGPIPE。** 有 `MSG_NOSIGNAL` 的地方就用它发，没有的地方
> 在建套接字时设 `SO_NOSIGPIPE`，所以写入失败会作为返回值报告，而不是把进程带走。

> [!NOTE]
> **等待用的是 `poll()`，描述符也交出来了。** 想让一个线程服务多条连接，可以自己对
> `native_handle()` 做 `poll()`；本模块不创建线程，也不替你做这件事。

## 相关模块

- [bytearray](bytearray.md) —— `read()` / `write()` 搬运的东西
- [process](process.md) —— 另一个 POSIX 专有模块：管道与子进程
