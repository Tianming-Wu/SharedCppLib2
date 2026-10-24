# network - 地址、域名解析、TCP 与 UDP

+ 名称: network
+ 命名空间: `network`
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `network`（聚合 `network_core`、`network_dns`、`network_tcp`、`network_udp`、`network_http`、`network_io`、`network_ws`） |
| 依赖 | `basic`；Windows 下套接字使用 Winsock，`network_core` 还使用 ICMP（`ws2_32`、`iphlpapi`） |

包含方式：

```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::network)
```

```cpp
#include <SharedCppLib2/network.hpp>
```

> [!NOTE]
> 这一层完全是同步的，没有事件循环，也没有回调：调用会在拿到结果时返回，超时也是一个
> 返回值。HTTP（[http](http.md)）与 WebSocket 是建立在 TCP 之上的独立协议层。

## 描述

`network` 是 SharedCppLib2 的套接字层：统一的地址类型（同时接受主机名与两种地址族）、
域名解析、TCP 客户端与服务端、UDP 套接字，以及 ICMP 回显。

| 头文件 | 目标 | 提供 |
|---------|---------|---------|
| `network.hpp` | `network_core` | `ipv4`、`ipv6`、`network_address`、`ping` |
| `dns.hpp` | `network_dns` | `dns_query`、`setDNSServers` |
| `tcpclient.hpp` | `network_tcp` | `tcp::client` |
| `tcpserver.hpp` | `network_tcp` | `tcp::server`、`tcp::server_client_handler` |
| `udp.hpp` | `network_udp` | `udp::socket` |
| `network_platform.hpp` | — | 平台套接字类型（`socket_t`、`invalid_socket`、`socket_error`） |

`SharedCppLib2::network` 会链接以上全部目标；只需要其中一个单元的程序可以直接链接该单元。

`init()` 与 `cleanup()` 存在但可选：创建套接字对象时库会自行初始化平台，`cleanup()` 用于
程序即将退出的那一刻。

## 地址

### `ipv4`

```cpp
struct ipv4 {
    uint8_t octet1, octet2, octet3, octet4;
    constexpr uint32_t to_uint32() const noexcept;
    static constexpr ipv4 from_uint32(uint32_t addr) noexcept;
    std::string to_string() const;
    static ipv4 from_string(const std::string& str);
    bool valid() const;
};
```

`octet1` 是 `to_uint32()` 的最高字节。`from_string()` 只接受点分十进制形式，文本不是地址时
抛 `network_error`。`valid()` 恒为 true：四个字节的任意组合都是一个地址。

### `ipv6`

```cpp
struct ipv6 {
    uint16_t blocks[8];
    uint32_t scope_id;                // 链路本地地址的 zone 索引，未设置时为 0
    std::array<uint8_t, 16> to_bytes() const noexcept;
    std::string to_string() const;
    std::string to_string_nocompress() const;
    static ipv6 from_string(const std::string& str);
    scl2::bytearray to_bytearray() const noexcept;
    static ipv6 from_bytearray(const scl2::bytearray& ba);
    bool valid() const;
};
```

- `from_string()` 接受 `::` 压缩、末尾点分四段以及大小写混合。末尾的 `%<索引>` 会读入
  `scope_id`；索引不是数字时抛 `network_error`。
- `to_string()` 返回规范压缩形式；设置了 zone 时追加 `%<scope_id>`。
- `to_bytearray()` / `from_bytearray()` 只接受 16 字节，其它长度抛 `network_error`。
- `valid()` 仅对全零地址 `::` 返回 false。

> [!NOTE]
> `to_string_nocompress()` 把每个 16 位分组写成**十进制**并用冒号连接，不是常见的十六进制
> 形式。需要标准文本地址时用 `to_string()`。

### `network_address`

```cpp
struct network_address {
    enum class kind : std::uint8_t { unspecified, hostname, ipv4, ipv6 };
    kind type;
    std::string address;
    ipv4 ipv4_addr;
    ipv6 ipv6_addr;

    static network_address parse(const std::string& text);
    kind what() const;
    bool is_literal() const;
    bool needs_resolution() const;
    bool valid() const;
    std::string to_string() const;
    ipv4 to_ipv4() const;
    ipv6 to_ipv6() const;
};
using netaddr = network_address;
```

这是各网络函数统一接受的类型。它可以从 `std::string`、`ipv4`、`ipv6` 隐式构造，因此主机名
与两种字面量形式可以用同一种写法传入。

- `parse()` 不抛异常：不是地址的文本会被当作 `kind::hostname`。
- `type` 是权威字段。解析出的成员只在对应 kind 下有意义。
- `valid()` 仅对 IPv4/IPv6 字面量返回 true；主机名在解析之前不算"有效"。
- 对象持有的是另一种 kind 时，`to_ipv4()` / `to_ipv6()` 抛 `network_error`。
- `kind::unspecified` 是绑定时未给出地址的含义："任意地址"。

`ip_preference` 决定 `dns_query_result::preferred()` 优先返回哪个地址族：

| 取值 | 含义 |
|---------|---------|
| `ip_preference::ipv4_first` | 优先 IPv4，没有则回退 IPv6（默认） |
| `ip_preference::ipv6_first` | 优先 IPv6，没有则回退 IPv4 |

## 域名解析

```cpp
namespace network::dns {

struct dns_query_result {
    std::string hostname;
    std::vector<network_address> addresses;
    dns_status status;
    std::string status_text;
    bool ok() const;
    std::vector<network_address> by_family(network_address::kind kind) const;
    network_address preferred(ip_preference preference = ip_preference::ipv4_first) const;
};

dns_query_result dns_query(const std::string& hostname);
dns_query_result dns_query(const std::wstring& hostname);

}
```

`addresses` 可以同时包含两种地址族，顺序就是解析器给出的顺序，重复项已去掉。
`ok()` 表示解析器作出了回答**并且**至少找到一个地址；`status` 只表示解析器是否作出了回答。

| `dns_status` | 含义 |
|---------|---------|
| `ok` | 解析器作出了回答 |
| `not_found` | 域名不存在 |
| `temporary_failure` | 解析器当前无法回答，稍后再试 |
| `server_failure` | 解析器回答了失败 |
| `failed` | 其它情况 |

`status_text` 是平台消息，成功时为空。

`preferred()` 返回首选地址族的第一个地址；首选族没有地址时返回另一族；无地址可返回时给出
一个无效地址。

> [!WARNING]
> `dns_query()` 不做缓存。反复解析同一个名字的循环应当保留拿到的 `network_address` 并复用。

> [!WARNING]
> `setDNSServers()` 修改的是**系统** DNS 配置，而不只是当前进程。通常需要管理员权限。

## TCP 客户端

```cpp
namespace network::tcp {

class client : public scl2::transport_interface {
public:
    bool connect(const std::string& address, uint16_t port);
    bool connect(const std::string& address, uint16_t port,
                 std::chrono::milliseconds timeout);
    bool connect(const network_address& address, uint16_t port);
    bool connect(const network_address& address, uint16_t port,
                 std::chrono::milliseconds timeout);

    void set_connect_timeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds connect_timeout() const;

    void disconnect();
    bool is_connected() const;
    std::string lastError() const;

    bool readable();
    size_t available();
    scl2::bytearray read(size_t bytes);
    scl2::bytearray readAll();
    size_t write(const scl2::bytearray& data);

    network_address server_address() const;
    uint16_t server_port() const;
};

}
```

需要时 `connect()` 会先解析主机名：优先尝试 IPv4 地址，全部不可达时再尝试 IPv6 候选。
`timeout` 覆盖连接尝试（包含解析）；为 0 表示一直等。这些函数从不抛异常——失败返回
`false`，原因由 `lastError()` 给出。

I/O 遵循 `scl2::transport_interface`：

| 调用 | 约定 |
|---------|---------|
| `readable()` | 有数据在等，或连接已结束 |
| `available()` | 当前可读的字节数 |
| `read(n)` | 最多 `n` 字节 |
| `readAll()` | 当前可读的字节，而不是读到流结束 |
| `write(data)` | 已接受的字节数；连接不再接受数据时为 0 |
| `waitForReadyRead(timeout)` | 等到 `readable()` 为 true（继承的默认实现） |

> [!NOTE]
> `read()` 在"暂无数据"和"连接已断开"两种情况下都返回空数组。已建立的连接上可用
> `readable()` 区分二者，调用之间用 `is_connected()` 检查状态。

## TCP 服务端

```cpp
namespace network::tcp {

using client_id = int;

class server {
public:
    server();
    server(uint16_t port);
    server(network_address address, uint16_t port);

    void start();
    void start(uint16_t port);
    void stop();

    int tick();
    std::vector<client_id> clients();
    bool closeClient(client_id id);
    server_client_handler selectClient(client_id& id);

    uint16_t port() const;
    network_address address() const;
    static std::string hostname(size_t max_length = 255);
};

}
```

- `start()` 绑定并监听；端口 0 会被拒绝。创建、绑定或监听失败时抛 `network_error`。
- 监听套接字与已接受的连接都是非阻塞的，服务端没有自己的线程。**`tick()` 就是服务端循环**：
  按程序能承受的频率反复调用。它会清理已经结束的连接，并且每次最多接受一个新连接。接受到
  连接时返回 `1`，没有可接受连接时返回 `0`，服务端未运行时返回 `-1`。
- `clients()` 返回当前 id。`closeClient(id)` 关闭并移除一个客户端；id 已不存在时是安全的
  （返回 `false`）。
- `selectClient(id)` 返回绑定到已有 id 的 `server_client_handler`；id 应当来自 `clients()`，
  并且只在该客户端存在期间使用 handler。
- `lock()` 取得服务端互斥锁；handler 有自己的 `lock()` 用于该连接。

服务端**只监听 IPv4**。构造时给出的地址不是 IPv4 地址时 `start()` 抛异常；默认地址监听
所有 IPv4 接口。

`server_client_handler` 以成员函数提供流式调用：

| 调用 | 约定 |
|---------|---------|
| `valid()` | 连接仍然存在 |
| `readable()` / `available()` / `read(n)` | 与客户端相同，作用于该连接 |
| `readAll()` | 每次调用最多 64 KiB |
| `write(data)` | 已接受的字节数；连接已断开时为 0 |
| `lock()` | 在存活期间持有该连接的互斥锁 |

## UDP

```cpp
namespace network::udp {

struct datagram {
    scl2::bytearray data;
    network_address sender_addr;
    uint16_t sender_port;
};

class socket : public scl2::basic_iostream {
public:
    bool bind(uint16_t port);
    bool bind(const network_address& addr, uint16_t port);
    bool connect(const std::string& address, uint16_t port);
    bool connect(const network_address& addr, uint16_t port);
    void close();

    size_t sendTo(const scl2::bytearray& data,
                  const network_address& dest, uint16_t port);
    size_t send(const scl2::bytearray& data);

    datagram receiveFrom();
    datagram receiveFrom(std::chrono::milliseconds timeout);

    size_t write(const scl2::bytearray& data);

    network_address localAddress() const;
    uint16_t localPort() const;
    bool is_connected() const;
};

}
```

UDP 套接字有两种模式：

| 模式 | 调用 | 含义 |
|---------|---------|---------|
| 未连接 | `sendTo()` / `receiveFrom()` | 每个数据报自带目的地或发送方 |
| 已连接 | `write()` / `read()` / `readAll()` / `send()` | `connect()` 设置了默认目的地 |

`connect()` 不做握手，只设置默认目的地（需要时先解析主机名）。`bind()` 只给端口时绑定到
所有接口。

流式调用遵循数据报语义：

- `read(n)` 从**一个数据报**里最多读 `n` 字节，该数据报剩余部分被丢弃。
- `readAll()` 读取一个完整的数据报。
- `available()` 是队列中第一个数据报的大小，不是等待中的总字节数。
- `receiveFrom()` 不等待；`receiveFrom(timeout)` 最多等 `timeout`，为 0 表示不等待。
- 收到的数据报 `data` 为空表示什么也没收到。

`sendTo()` / `send()` / `write()` 返回已发送字节数，失败返回 0。`send()` 与 `write()` 需要
默认目的地，没有时返回 0。

## Ping

```cpp
bool ping(const network_address& addr,
          std::chrono::milliseconds timeout = std::chrono::seconds(1));
std::optional<std::chrono::milliseconds> ping_rtt(
          const network_address& addr,
          std::chrono::milliseconds timeout = std::chrono::seconds(1));
```

两者都只接受**IPv4 字面量地址**，其它输入（包括主机名与 IPv6 地址）返回 false / 空。
主机名请先用 `dns_query()` 解析。

## 相关模块

- [http](http.md) — 建立在 TCP 之上的 HTTP 客户端与服务端
- [bytearray](bytearray.md) — I/O 调用使用的缓冲类型
