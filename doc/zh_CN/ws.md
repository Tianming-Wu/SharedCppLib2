# ws - WebSocket 客户端

+ 名称: ws
+ 命名空间: `network::ws`
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `network_ws` |
| 依赖 | `basic`、`sha1`；默认经 `network_tcp` 走普通 TCP |

包含方式：

```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::network_ws)
```

```cpp
#include <SharedCppLib2/wsclient.hpp>   // network::ws::client
#include <SharedCppLib2/ws.hpp>         // opcode、message、receive_status、close_code
```

> [!NOTE]
> `network_ws` 属于 `network` 聚合目标。已经链接 `SharedCppLib2::network` 的程序不必再单独添加。

## 描述

`network::ws` 是 WebSocket（RFC 6455）客户端。一个对象就是一条连接。

接口是**面向消息**的，不是面向字节流的：帧、掩码、分片与控制帧都是模块内部的事，调用方看到
的是完整消息。这里没有事件循环，所以风格与套接字层其余部分一致：调用在拿到结果时返回，超时
是一个返回值。

- `ws.hpp` — `opcode`、`message`、`receive_status`、`close_code`
- `wsclient.hpp` — `network::ws::client`
- 普通 `ws://` 使用默认的 `network::tcp::client`；`wss://` 通过向构造函数传入 TLS transport
  实现（SCL2Ext 的 OpenSSL 集成）

## 快速开始

```cpp
#include <SharedCppLib2/wsclient.hpp>

network::ws::client socket;

if (!socket.connect("example.com", 8080, "/ws?room=1", std::chrono::seconds(5))) {
    std::fprintf(stderr, "连接失败: %s\n", socket.lastError().c_str());
    return;
}

if (!socket.send_text("hello")) {
    std::fprintf(stderr, "发送失败: %s\n", socket.lastError().c_str());
    return;
}

network::ws::message message;
for (;;) {
    switch (socket.receive(message, std::chrono::milliseconds(500))) {
    case network::ws::receive_status::message:
        if (message.type == network::ws::opcode::text) {
            std::printf("文本: %s\n", message.payload.toStdString().c_str());
        }
        break;
    case network::ws::receive_status::timeout:
        continue;                       // 暂时没有消息，连接仍然有效
    case network::ws::receive_status::closed:
        return;                         // 会话结束
    case network::ws::receive_status::failed:
        std::fprintf(stderr, "失败: %s\n", socket.lastError().c_str());
        return;
    }
}
```

## 模型

### 一条连接，一个对象

`connect()` 会关闭该对象已有的会话，通过 transport 解析主机名、建立连接并完成开握手。
`close()` 是幂等的。会话仍然打开时，析构函数会发一个 close 帧。

`close()` 发出 close 帧后立即断开连接，不等待对端回应。从对端收到的 close 帧会在断开连接
之前自动回显。

### 是消息，不是帧

`receive()` 只返回数据消息：

| `message.type` | 含义 |
|---------|---------|
| `opcode::text` | UTF-8 文本消息，分片已合并 |
| `opcode::binary` | 二进制消息，分片已合并 |

控制帧不会交给调用方。ping 会用相同负载回一个 pong；pong 被静默消费；close 结束会话。

### 四种结果

`receive()` 把结果写进 `receive_status`：

| 状态 | 含义 | 该做什么 |
|---------|---------|---------|
| `message` | 一条完整消息已写入输出参数 | 处理它，然后再次调用 |
| `timeout` | 时间限制内没有等到完整消息 | 再次调用 |
| `closed` | 连接已不处于打开状态 | 结束 |
| `failed` | 协议违规或传输失败 | 结束；原因见 `lastError()` |

`timeout` 为负表示一直等，为 0 表示只取已经到达的数据。默认 5 秒。

### 握手

`connect(host, port, path, timeout)` 发出标准的 RFC 6455 开握手，并要求：

- 状态码为 `101`
- `Upgrade` 包含 `websocket`
- `Connection` 包含 `Upgrade`
- `Sec-WebSocket-Accept` 与发出的 key 匹配
- 没有客户端未请求的 `Sec-WebSocket-Extensions` 或 `Sec-WebSocket-Protocol`

请求目标默认是 `/`，并保留查询串。`Host` 是 `host:port`，IPv6 字面量会加方括号。可以用
`set_header()` 追加请求头，`clear_headers()` 清除它们。

`timeout` 同时覆盖域名解析、连接与握手。响应头块限制为 64 KiB，超过即连接失败。

### 严格性

客户端会执行协议检查，而不是猜测：

| 违规 | 结果 |
|---------|---------|
| 设置了保留位 | `failed`，close code 1002 |
| 服务端发来掩码帧 | `failed`，close code 1002 |
| 控制帧被分片或超过长度 | `failed`，close code 1002 |
| 没有起始消息的 continuation 帧 | `failed`，close code 1002 |
| 未知 opcode | `failed`，close code 1002 |
| 文本消息不是合法 UTF-8 | `failed`，close code 1007 |
| 消息超过限制 | `failed`，close code 1009 |

限制由 `set_max_message_size()` 设置，默认 32 MiB，作用于合并后的消息。帧声明的长度会在
负载被缓冲之前就与限制比较，因此超限消息不必等它到达才被拒绝。控制帧负载按协议限制为
125 字节。

### wss

接受 `std::unique_ptr<scl2::transport_interface>` 的构造函数会替换默认 TCP transport。传入
TLS transport 后，同一份代码就能说 `wss://`——握手、分帧及其上的一切都不变。

```cpp
#include <SharedCppLib2/wsclient.hpp>
#include <tls.hpp>                       // SCL2Ext 的 OpenSSL 集成

network::ws::client socket(std::make_unique<scl2ext::tls_client>());
socket.connect("example.com", 443, "/ws");
```

这种用法需要在 `SharedCppLib2::network_ws` 之外同时链接 `SCL2Ext::openssl`。

> [!NOTE]
> transport 必须可连接：`ws::client` 会对它调用 `connect()`。transport 的所有权转移给
> 客户端。

### 线程

一个对象同一时间由一个线程驱动。模块内部没有线程，也没有加锁，`receive()` 最多阻塞到它的
超时时间。

## 函数参考

### `client`

| 成员 | 约定 |
|---------|---------|
| `connect(host, port, path = "/", timeout = 10s)` | 连接并完成握手；失败返回 false |
| `close(code = 1000, reason = {})` | 发送 close 帧并断开；幂等；`reason` 截断到 123 字节 |
| `is_open()` | 握手成功且连接仍然存在 |
| `is_connected()` | transport 报告已连接 |
| `send_text(text)` | 发送一条文本消息；`text` 不是合法 UTF-8 或会话已结束时返回 false |
| `send_binary(data)` | 发送一条二进制消息 |
| `ping(payload = {})` | 发送 ping；负载最多 125 字节 |
| `receive(out, timeout = 5s)` | 等待一条消息；状态见上表 |
| `close_code()` | 发出或收到的 close code；没有时为 0 |
| `close_reason()` | 发出或收到的 close 原因 |
| `lastError()` | 上一次操作失败的原因；没有可说的内容时为空 |
| `set_max_message_size(bytes)` / `max_message_size()` | 消息限制，默认 32 MiB |
| `set_header(name, value)` / `clear_headers()` | 开握手的附加请求头 |

### `close_code`

模块用 `close_code` 给用到的码命名：

| 取值 | 数字 |
|---------|---------|
| `normal` | 1000 |
| `going_away` | 1001 |
| `protocol_error` | 1002 |
| `unsupported_data` | 1003 |
| `invalid_payload` | 1007 |
| `policy_violation` | 1008 |
| `message_too_big` | 1009 |
| `internal_error` | 1011 |

`close_code()` 与 `close()` 的 code 参数都是普通的 `std::uint16_t`，应用可以发送任意码。

### 这里没有的东西

- 没有子协议协商（服务端选择子协议会被拒绝）
- 没有扩展，包括压缩（服务端选择扩展会被拒绝）
- 没有自动重连，也没有重试
- 发送端不分片：`send_text()` / `send_binary()` 总是发送一个 final 帧
- 没有事件/回调形态；那属于建立在其上的另一层

## 相关模块

- [network](network.md) — 地址、DNS 与底层的 TCP 客户端
- [http](http.md) — 同样建立在 transport 之上的 HTTP 协议层
- [bytearray](bytearray.md) — 负载类型
