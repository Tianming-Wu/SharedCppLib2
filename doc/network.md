# network - Addresses, Name Resolution, TCP and UDP

+ Name: network
+ Namespace: `network`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `network` (aggregate of `network_core`, `network_dns`, `network_tcp`, `network_udp`, `network_http`, `network_io`, `network_ws`) |
| Dependencies | `basic`; on Windows the sockets use Winsock, and `network_core` also uses ICMP (`ws2_32`, `iphlpapi`) |

To include:

```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::network)
```

```cpp
#include <SharedCppLib2/network.hpp>
```

> [!NOTE]
> The whole layer is synchronous and has no event loop or callbacks: a call returns when it
> has its answer, and a timeout is a return value. HTTP ([http](http.md)) and WebSocket
> ([ws](ws.md)) are separate protocol layers built on top of TCP.

## Description

`network` is the socket layer of SharedCppLib2: one address type that accepts host names and
both address families, name resolution, a TCP client and server, a UDP socket, and ICMP echo.

| Header | Target | Provides |
|---------|---------|---------|
| `network.hpp` | `network_core` | `ipv4`, `ipv6`, `network_address`, `ping` |
| `dns.hpp` | `network_dns` | `dns_query`, `setDNSServers` |
| `tcpclient.hpp` | `network_tcp` | `tcp::client` |
| `tcpserver.hpp` | `network_tcp` | `tcp::server`, `tcp::server_client_handler` |
| `udp.hpp` | `network_udp` | `udp::socket` |
| `network_platform.hpp` | — | platform socket types (`socket_t`, `invalid_socket`, `socket_error`) |

`SharedCppLib2::network` links all of them. A program that needs only one unit can link that
unit instead.

`init()` and `cleanup()` exist but are optional: the library initialises the platform when a
socket object is created, and `cleanup()` is for the moment just before the program exits.

## Addresses

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

`octet1` is the most significant octet of `to_uint32()`. `from_string()` accepts dotted-decimal
form only and throws `network_error` when the text is not an address. `valid()` is always true:
every combination of four octets is an address.

### `ipv6`

```cpp
struct ipv6 {
    uint16_t blocks[8];
    uint32_t scope_id;                // zone index for link-local addresses, 0 when unset
    std::array<uint8_t, 16> to_bytes() const noexcept;
    std::string to_string() const;
    std::string to_string_nocompress() const;
    static ipv6 from_string(const std::string& str);
    scl2::bytearray to_bytearray() const noexcept;
    static ipv6 from_bytearray(const scl2::bytearray& ba);
    bool valid() const;
};
```

- `from_string()` accepts `::` compression, a trailing dotted quad and mixed case. A trailing
  `%<index>` is read as `scope_id`; a non-numeric index throws `network_error`.
- `to_string()` returns the canonical compressed form and appends `%<scope_id>` when the zone
  is set.
- `to_bytearray()` / `from_bytearray()` use exactly 16 bytes; any other size throws
  `network_error`.
- `valid()` is false only for the all-zero address `::`.

> [!NOTE]
> `to_string_nocompress()` writes each 16-bit group as a **decimal** number separated by
> colons, not the usual hexadecimal form. Use `to_string()` for the standard textual address.

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

This is the type the network functions take. It converts implicitly from `std::string`, `ipv4`
and `ipv6`, so a name and both literal forms are passed the same way.

- `parse()` never throws: a text that is not an address becomes `kind::hostname`.
- `type` is authoritative. The parsed members are only meaningful for the matching kind.
- `valid()` is true only for an IPv4 or IPv6 literal; a host name is not "valid" until it is
  resolved.
- `to_ipv4()` / `to_ipv6()` throw `network_error` when the object holds another kind.
- `kind::unspecified` is what binding uses when no address was given: it means "any".

`ip_preference` chooses which family `dns_query_result::preferred()` returns first:

| Value | Meaning |
|---------|---------|
| `ip_preference::ipv4_first` | prefer IPv4; fall back to IPv6 (default) |
| `ip_preference::ipv6_first` | prefer IPv6; fall back to IPv4 |

## Name Resolution

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

`addresses` may hold both families. Their order is the resolver's, with duplicates removed.
`ok()` means the lookup answered **and** found at least one address; `status` alone only says
whether the resolver answered.

| `dns_status` | Meaning |
|---------|---------|
| `ok` | the resolver answered |
| `not_found` | the name does not exist |
| `temporary_failure` | the resolver could not answer now; try again later |
| `server_failure` | the resolver answered with a failure |
| `failed` | anything else |

`status_text` carries the platform message and is empty on success.

`preferred()` returns the first address of the preferred family, or the other family when the
preferred one has none; it returns an invalid address when there is nothing to return.

> [!WARNING]
> `dns_query()` does not cache. A loop that resolves the same name repeatedly should keep the
> `network_address` it got and reuse it.

> [!WARNING]
> `setDNSServers()` changes the **system** DNS configuration, not just the current process.
> Administrative rights are usually required.

## TCP Client

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

`connect()` resolves a host name when necessary, trying IPv4 addresses first and the IPv6
candidates when no IPv4 address can be reached. `timeout` covers the connection attempt,
resolution included; a timeout of 0 means "wait as long as it takes". The functions never throw
— a failure is `false`, and `lastError()` says why.

I/O follows `scl2::transport_interface`:

| Call | Contract |
|---------|---------|
| `readable()` | data is waiting, or the connection has ended |
| `available()` | bytes that can be read right now |
| `read(n)` | up to `n` bytes |
| `readAll()` | the bytes available right now, not until end of stream |
| `write(data)` | bytes accepted; 0 when the connection no longer accepts data |
| `waitForReadyRead(timeout)` | waits until `readable()` is true (inherited default) |

> [!NOTE]
> `read()` returns an empty array both when nothing is available and when the connection is
> gone. Use `readable()` on an established connection to tell "nothing yet" from "closed", and
> `is_connected()` to check the state between calls.

## TCP Server

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

- `start()` binds and listens; port 0 is rejected. Failure to create, bind or listen throws
  `network_error`.
- The listening socket and the accepted connections are non-blocking, and the server has no
  thread of its own. **`tick()` is the server loop**: call it repeatedly, as often as the
  program can. It removes connections that have ended and accepts at most one new pending
  connection per call. It returns `1` when a connection was accepted, `0` when there was
  nothing to accept, and `-1` when the server is not running.
- `clients()` returns the current ids. `closeClient(id)` closes and forgets a client and is safe
  for an id that is already gone (it returns `false`).
- `selectClient(id)` returns a `server_client_handler` bound to an existing id; obtain the id
  from `clients()` and keep the handler alive only while that client exists.
- `lock()` takes the server mutex; a handler has its own `lock()` for the connection.

The server listens on **IPv4 only**. `start()` throws when the address given to the constructor
is not an IPv4 address; the default address listens on every IPv4 interface.

`server_client_handler` provides the stream calls as members:

| Call | Contract |
|---------|---------|
| `valid()` | the connection still exists |
| `readable()` / `available()` / `read(n)` | like the client, on this connection |
| `readAll()` | at most 64 KiB per call |
| `write(data)` | bytes accepted; 0 when the connection is gone |
| `lock()` | holds this connection's mutex while alive |

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

A UDP socket has two modes:

| Mode | Calls | Meaning |
|---------|---------|---------|
| unconnected | `sendTo()` / `receiveFrom()` | every datagram carries its own destination or sender |
| connected | `write()` / `read()` / `readAll()` / `send()` | a default destination was set by `connect()` |

`connect()` performs no handshake; it only sets the default destination (and resolves a host
name first when needed). `bind()` binds to all interfaces when given only a port.

Datagram semantics apply to the stream calls:

- `read(n)` reads at most `n` bytes **from one datagram**; the rest of that datagram is
  discarded.
- `readAll()` reads one complete datagram.
- `available()` is the size of the first queued datagram, not the total waiting.
- `receiveFrom()` does not wait; `receiveFrom(timeout)` waits up to `timeout`, and timeout 0
  means "do not wait".
- A received datagram with an empty `data` means nothing arrived.

`sendTo()` / `send()` / `write()` return the bytes sent, or 0 on failure. `send()` and `write()`
need a default destination and return 0 without one.

## Ping

```cpp
bool ping(const network_address& addr,
          std::chrono::milliseconds timeout = std::chrono::seconds(1));
std::optional<std::chrono::milliseconds> ping_rtt(
          const network_address& addr,
          std::chrono::milliseconds timeout = std::chrono::seconds(1));
```

Both take a **literal IPv4 address** and return false / nothing for anything else, including a
host name and an IPv6 address. Resolve a name with `dns_query()` first.

## See Also

- [http](http.md) — HTTP client and server on top of TCP
- [ws](ws.md) — WebSocket client
- [bytearray](bytearray.md) — the buffer type the I/O calls use
