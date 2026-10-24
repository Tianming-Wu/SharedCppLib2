# ws - WebSocket Client

+ Name: ws
+ Namespace: `network::ws`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `network_ws` |
| Dependencies | `basic`, `sha1`; plain TCP through `network_tcp` |

To include:

```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::network_ws)
```

```cpp
#include <SharedCppLib2/wsclient.hpp>   // network::ws::client
#include <SharedCppLib2/ws.hpp>         // opcode, message, receive_status, close_code
```

> [!NOTE]
> `network_ws` is part of the `network` aggregate target. A program that already links
> `SharedCppLib2::network` does not need to add it separately.

## Description

`network::ws` is a client for WebSocket (RFC 6455). One object is one connection.

The interface is **message oriented**, not byte-stream oriented: frames, masking, fragmentation
and control frames are the module's business, and the caller sees complete messages. There is
no event loop, so the style is the same as the rest of the socket layer: a call returns when it
has its answer, and a timeout is a return value.

- `ws.hpp` — `opcode`, `message`, `receive_status`, `close_code`
- `wsclient.hpp` — `network::ws::client`
- plain `ws://` runs on the default `network::tcp::client`; `wss://` works by passing a TLS
  transport to the constructor (the SCL2Ext OpenSSL integration)

## Quick Start

```cpp
#include <SharedCppLib2/wsclient.hpp>

network::ws::client socket;

if (!socket.connect("example.com", 8080, "/ws?room=1", std::chrono::seconds(5))) {
    std::fprintf(stderr, "connect failed: %s\n", socket.lastError().c_str());
    return;
}

if (!socket.send_text("hello")) {
    std::fprintf(stderr, "send failed: %s\n", socket.lastError().c_str());
    return;
}

network::ws::message message;
for (;;) {
    switch (socket.receive(message, std::chrono::milliseconds(500))) {
    case network::ws::receive_status::message:
        if (message.type == network::ws::opcode::text) {
            std::printf("text: %s\n", message.payload.toStdString().c_str());
        }
        break;
    case network::ws::receive_status::timeout:
        continue;                       // nothing yet; the connection is still open
    case network::ws::receive_status::closed:
        return;                         // the session ended
    case network::ws::receive_status::failed:
        std::fprintf(stderr, "failed: %s\n", socket.lastError().c_str());
        return;
    }
}
```

## The Model

### One connection, one object

`connect()` closes any session the object already had, resolves the host through the transport,
opens the connection and performs the opening handshake. `close()` is idempotent. The
destructor sends a close frame when a session is still open.

`close()` sends the close frame and drops the connection immediately; it does not wait for the
peer to answer. A close frame received from the peer is echoed automatically before the
connection is dropped.

### Messages, not frames

`receive()` returns data messages only:

| `message.type` | Meaning |
|---------|---------|
| `opcode::text` | a UTF-8 text message, fragments already joined |
| `opcode::binary` | a binary message, fragments already joined |

Control frames never reach the caller. A ping is answered with a pong that carries the same
payload; a pong is consumed silently; a close ends the session.

### Four outcomes

`receive()` writes its result into a `receive_status`:

| Status | Meaning | What to do |
|---------|---------|---------|
| `message` | one complete message was written to the output argument | handle it, then call again |
| `timeout` | nothing complete arrived within the time limit | call again |
| `closed` | the connection is not open | stop |
| `failed` | protocol violation or transport failure | stop; `lastError()` says why |

`timeout` is negative to wait forever, and zero to take only what has already arrived. The
default is 5 seconds.

### Handshake

`connect(host, port, path, timeout)` sends a standard RFC 6455 opening handshake and requires:

- status code `101`
- `Upgrade` containing `websocket`
- `Connection` containing `Upgrade`
- `Sec-WebSocket-Accept` matching the key that was sent
- no `Sec-WebSocket-Extensions` or `Sec-WebSocket-Protocol` that the client did not ask for

The request target defaults to `/` and keeps the query string. `Host` is `host:port`, with an
IPv6 literal in brackets. Extra request headers can be added with `set_header()`; `clear_headers()`
removes them.

`timeout` covers name resolution, the connection and the handshake together. The response
header block is limited to 64 KiB; a longer one fails the connection.

### Strictness

The client enforces the protocol rather than guessing:

| Violation | Result |
|---------|---------|
| reserved bits set | `failed`, close code 1002 |
| a masked frame from the server | `failed`, close code 1002 |
| a fragmented or oversized control frame | `failed`, close code 1002 |
| an orphan continuation frame | `failed`, close code 1002 |
| an unknown opcode | `failed`, close code 1002 |
| invalid UTF-8 in a text message | `failed`, close code 1007 |
| a message larger than the limit | `failed`, close code 1009 |

The limit is `set_max_message_size()`, 32 MiB by default, and applies to the reassembled
message. The declared frame length is checked against the limit before the payload is
buffered, so an oversized message is refused without waiting for it. Control frame payloads
are limited to 125 bytes by the protocol.

### wss

The constructor that takes a `std::unique_ptr<scl2::transport_interface>` replaces the default
TCP transport. A TLS transport makes the same code speak `wss://` — the handshake, framing and
everything above them do not change.

```cpp
#include <SharedCppLib2/wsclient.hpp>
#include <tls.hpp>                       // SCL2Ext OpenSSL integration

network::ws::client socket(std::make_unique<scl2ext::tls_client>());
socket.connect("example.com", 443, "/ws");
```

Link `SCL2Ext::openssl` alongside `SharedCppLib2::network_ws` in that case.

> [!NOTE]
> The transport must be connectable: `connect()` is called on it by `ws::client`. Ownership of
> the transport is transferred to the client.

### Threading

One object is driven by one thread at a time. There is no internal thread and no locking, and
`receive()` blocks for up to its timeout.

## Reference

### `client`

| Member | Contract |
|---------|---------|
| `connect(host, port, path = "/", timeout = 10s)` | connects and performs the handshake; false on failure |
| `close(code = 1000, reason = {})` | sends a close frame and drops the connection; idempotent; `reason` is truncated to 123 bytes |
| `is_open()` | the handshake succeeded and the connection is still there |
| `is_connected()` | the transport says it is connected |
| `send_text(text)` | sends one text message; false when `text` is not valid UTF-8 or the session is gone |
| `send_binary(data)` | sends one binary message |
| `ping(payload = {})` | sends a ping; at most 125 bytes |
| `receive(out, timeout = 5s)` | waits for one message; see the status table above |
| `close_code()` | the close code sent or received; 0 when there was none |
| `close_reason()` | the close reason sent or received |
| `lastError()` | why the last operation failed; empty when there is nothing to say |
| `set_max_message_size(bytes)` / `max_message_size()` | the message limit, 32 MiB by default |
| `set_header(name, value)` / `clear_headers()` | extra headers for the opening handshake |

### `close_code`

The module names the codes it uses, as `close_code`:

| Value | Number |
|---------|---------|
| `normal` | 1000 |
| `going_away` | 1001 |
| `protocol_error` | 1002 |
| `unsupported_data` | 1003 |
| `invalid_payload` | 1007 |
| `policy_violation` | 1008 |
| `message_too_big` | 1009 |
| `internal_error` | 1011 |

`close_code()` and the code argument of `close()` are plain `std::uint16_t`, so any code the
application wants can be sent.

### What is not here

- no subprotocol negotiation (a server that selects one is rejected)
- no extensions, including compression (a server that selects one is rejected)
- no automatic reconnection, and no retry
- no fragmentation on send: `send_text()` / `send_binary()` always send one final frame
- no event/callback form; that belongs to a layer built on top

## See Also

- [network](network.md) — addresses, DNS and the TCP client underneath
- [http](http.md) — the HTTP protocol layer, built the same way on a transport
- [bytearray](bytearray.md) — the payload type
