# pipe - Named Pipes

+ Name: pipe
+ Namespace: `scl2::pipe`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `pipe` |
| Dependencies | `basic` |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::pipe)
```

```cpp
#include <SharedCppLib2/pipe.hpp>
```

## Description

A named pipe is a connection between two processes of the same machine, addressed by a name
instead of an address and a port. No port is opened, nothing is reachable from the network,
and the name is the whole address.

One `server` owns the name and hands out a `server_client` for every connection; a `client`
connects to that name. Both ends are `scl2::basic_iostream`, so the same read and write calls
are used on the server side and on the client side.

A pipe can carry a plain byte stream, or messages whose boundaries the kernel keeps: in
message mode one `write()` is one message, and `readAll()` returns exactly that message and
never half of it.

The module is Windows only, because it is built on named pipes. On any other platform the
header defines nothing, says so during the build, and leaves `SCL2_PIPE_SUPPORTED` at 0 —
which is the macro portable code tests:

```cpp
#include <SharedCppLib2/pipe.hpp>

#if SCL2_PIPE_SUPPORTED
    scl2::pipe::server server("my_app");
    ...
#endif
```

## Quick Start

The server:
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

    auto client = server.queryNextConnection();          // connected, and listening again
    if (!client.valid())
        continue;

    scl2::bytearray request = client.readAll();          // one whole message
    client.write(scl2::bytearray("Hello from server!"));

    client.close();                                      // the connection is done
}
```

The client:
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

## The Model

| Piece | Role |
|---------|---------|
| `server` | Owns the name, listens, and accepts connections |
| `server_client` | One accepted connection, on the server side. Move-only |
| `client` | The connecting end. Move-only |
| `permissions` | The security descriptor the server creates the pipe with |

Connections are accepted one at a time: `queryNextConnection()` returns the client that is
waiting now, and immediately starts listening for the next one. A server that wants to serve
several clients at once puts each returned `server_client` on its own thread — different
`server_client` objects are independent.

## Reference

### server

| Member | Description |
|---------|---------|
| `server(name)` | Prepare a server for that name. Nothing is created yet |
| `server(name, permissions)` | The same, with an explicit security descriptor |
| `start()` | Create the pipe and begin listening. `false` on failure |
| `stop()` / `cleanup()` | Stop listening, drop the clients and close the handles |
| `active()` | Whether it is listening |
| `stopped()` | Whether it has been stopped |
| `clientCount()` | How many clients are held right now |
| `queryNextConnection()` | Wait for, and take, the next client. Returns an invalid `server_client` when there is none. This call blocks, so `hasPendingConnection()` / `waitForNextConnection(timeout)` tell you when it is worth making |
| `hasPendingConnection()` | Whether a connection is waiting right now |
| `waitForNextConnection(timeout = 5s)` | Wait until one is |
| `setBufferSize(size)` / `bufferSize()` | Per-connection I/O buffer, 4 KiB by default |
| `setPermissions(p)` / `getPermissions()` | Which security descriptor new connections are created with |
| `setPipeMode(m)` / `getPipeMode()` | What the pipe carries: the value new clients detect |
| `setMaxClients(n)` / `getMaxClients()` | How many clients may be connected at once |
| `unlimitedClients` | 254, the largest value `setMaxClients()` accepts |

`setBufferSize()`, `setPermissions()`, `setPipeMode()` and `setMaxClients()` throw
`std::runtime_error` while the server is active: these are decided when the pipe is created,
so they can only be set before `start()`.

### server_client

| Member | Description |
|---------|---------|
| `valid()` | Whether the connection is still open |
| `broken()` | Whether an operation has failed on a connection that is gone |
| `readyRead()` | Whether there is something to read now |
| `waitForReadyRead(timeout = 5s)` | Wait until there is |
| `available()` | How many bytes are waiting |
| `read(bytes)` | Up to `bytes` bytes. Empty in `mode::Message` |
| `readAll()` | One whole message, reassembled when it is larger than the buffer |
| `write(data)` | One write, or one message. The number of bytes written |
| `acknowledge()` | Send the acknowledgement token |
| `waitForAcknowledged(timeout = 5s)` | Wait for the peer's acknowledgement token |
| `close()` | End this connection. The object stays usable as an invalid client |
| `cleanup()` | The same as `close()` |
| `bufferSize()` | The buffer this connection uses; it follows the server, and is not settable |

### client

| Member | Description |
|---------|---------|
| `connect(timeout = 5s)` | Connect to the server, waiting at most `timeout`. Retries while the name does not exist yet and while every instance is busy |
| `waitForConnection(timeout = 5s)` | The same as `connect(timeout)`; the clearer name when the server is expected to start later |
| `serverExists()` | Whether something is listening on the name now. `false` only when the name does not exist |
| `valid()` / `broken()` | As on `server_client` |
| `readyRead()` / `waitForReadyRead(timeout = 5s)` / `available()` | As on `server_client` |
| `read(bytes)` / `readAll()` / `write(data)` | As on `server_client` |
| `acknowledge()` / `waitForAcknowledged(timeout = 5s)` | As on `server_client` |
| `pipeMode()` | The mode the server announced, filled in by `connect()` |
| `setPipeMode(m)` | Override it, for example to read a message pipe chunk by chunk |
| `close()` / `cleanup()` | Close the connection. The same as each other |

### mode

| Value | What a read returns |
|---------|---------|
| `Byte` | The byte stream, as it happens to arrive. `read(n)` and `readAll()` are both useful |
| `Message` | `readAll()` returns exactly one `write()`. `read()` returns nothing |
| `MessageChunk` | The kernel mode of `Message`, but `read(n)` hands out one chunk at a time, up to the buffer size, and reassembling is the caller's job. `readAll()` still returns a whole message |

A message larger than the buffer is not a problem by itself: `readAll()` keeps reading and
reassembling until the message ends. Only `MessageChunk` with `read()` leaves the rest of a
large message to you.

### permission_preset

| Value | Who may connect |
|---------|---------|
| `None` | No explicit descriptor |
| `Default` | The same as `None`: Windows' default descriptor for the process, which usually means administrators |
| `Everyone` | Anyone who can reach the name |
| `SameSID` | The user the server runs as |

## Notes

> [!NOTE]
> **A connection is closed with `close()`.** It disconnects the pipe, so the other end sees
> the connection end instead of a write that never arrives. Dropping the object does the same
> through the destructor, but closing explicitly says when.

> [!NOTE]
> **`acknowledge()` exists because a pipe does not flush.** The kernel buffers a write until
> the other end reads it, and a client that writes and immediately disappears can take its
> last message with it. Sending the acknowledgement token, and waiting for it on the other
> side, makes a sender-outlives-its-data race impossible: the client knows the server has
> read the message before it closes.

> [!NOTE]
> **`broken()` is a lazy flag.** It is set by a failed operation and is never cleared, so it
> is only meaningful after an operation has failed. There is no way to probe the connection
> with it.

> [!WARNING]
> **254 clients, not 255.** Windows caps the instances of one pipe name at 255, and
> `queryNextConnection()` briefly holds two at once while it hands one out and listens for
> the next. `unlimitedClients` is therefore 254, and that is the largest value
> `setMaxClients()` takes.

> [!NOTE]
> **The name is machine-wide, not per-user.** Two processes only meet if they agree on the
> name exactly; nothing about the server's identity is part of it. Use `permissions` to
> decide who may connect.

## See Also

- [bytearray](bytearray.md) — what `read()` and `write()` move
- [process](process.md) — anonymous pipes, and the same `scl2::basic_iostream` model, for a child process
- [unixsocket](unixsocket.md) — the POSIX counterpart: local sockets instead of named pipes
