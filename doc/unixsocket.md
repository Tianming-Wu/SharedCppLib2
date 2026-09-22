# unixsocket - Local (AF_UNIX) Sockets

+ Name: unixsocket
+ Namespace: `scl2::posix`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `unixsocket` |
| Dependencies | `basic` |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::unixsocket)
```

```cpp
#include <SharedCppLib2/unixsocket.hpp>
```

## Description

A local socket is addressed by a name instead of an address and a port, so no address
handling or name resolution is involved: the name is the whole address. Two processes on the
same machine can talk through one without a port, and without a network stack in the way.

`unix_stream` is a `scl2::transport_interface`, so a protocol layer that takes a transport
(the HTTP client, for example) runs over a local socket unchanged. `unix_server` is the
listening side, and hands out `unix_stream`s.

The module is POSIX only: Linux, Android, macOS and the BSDs. On Windows and on any platform
the header does not recognise, it defines nothing, says so during the build, and leaves
`SCL2_UNIXSOCKET_SUPPORTED` at 0 — which is the macro portable code tests:

```cpp
#include <SharedCppLib2/unixsocket.hpp>

#if SCL2_UNIXSOCKET_SUPPORTED
    scl2::posix::unix_stream stream;
    ...
#endif
```

## Quick Start

The server:
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
// the connection ends when `client` goes out of scope
```

The client:
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

## Names

| Name | What it is |
|---------|---------|
| `/run/app.sock` | An ordinary path. The kernel creates a socket file there, and permissions on that file decide who may connect |
| `@app` | The abstract namespace (Linux and Android): no file, no permissions, and the name is gone when the process is. A name that begins with a `'\0'` means the same thing |

A path has to fit in `sockaddr_un::sun_path` (108 bytes on Linux), and an abstract name loses
one byte to the marker. A name that is too long reports `ENAMETOOLONG` and is not truncated.

## API

### unix_stream

| Member | Description |
|---------|---------|
| `connect(name)` | Connect to a path or to `@name`. `false` on failure, with the reason in `lastError()` |
| `connect(address, port)` | The `transport_interface` form: the name goes in `address`, and the port is not used |
| `disconnect()` | Shut down and close. Does nothing when there is nothing to close |
| `is_connected()` / `valid()` | Whether a descriptor is held. A peer that closed its end is not noticed by this |
| `readyRead()` | Whether there is something to read now: data, or a closed peer |
| `waitForReadyRead(timeout = 5s)` | Wait until there is |
| `available()` | How many bytes are waiting (`FIONREAD`) |
| `read(bytes)` | Up to `bytes` bytes; empty when nothing arrived. An empty result is not an error |
| `readAll()` | Everything that is available now |
| `write(data)` | One `send`; the number of bytes sent, 0 when none were |
| `reset()` | Drop the connection, and answer `true` |
| `native_handle()` | The descriptor, or `-1`, for the caller's own `poll()` / `select()` |
| `lastError()` | The system error code of the last failed call, 0 after a successful one |

### unix_server

| Member | Description |
|---------|---------|
| `listen(name, backlog = 8, mode = 0)` | Create, bind and listen. `mode` is the permission given to the socket file (an Android service usually wants `0660`); 0 leaves the umask in charge. Not used for an abstract name |
| `close()` | Stop listening, and remove the socket file when this server is the one that made it |
| `is_listening()` | Whether it is listening |
| `accept(timeout = 5s)` | One connection, waiting at most `timeout`. `nullptr` on timeout and on failure |
| `tryAccept()` | The same, without waiting |
| `name()` | The name it was opened with, empty when it is not listening |
| `native_handle()` / `lastError()` | As on the stream |

## Notes

> [!NOTE]
> **A stale socket file is removed, a live one is not.** `listen()` tries to connect to the
> path first: if something answers, that is a server that is still running, so the call fails
> with `EADDRINUSE` and the file stays. If nothing answers, the file was left behind by a
> process that is gone, and it is removed. `close()` removes only a socket, so a path that has
> been replaced by something else in the meantime is left alone.

> [!NOTE]
> **A timeout is not an error.** `accept()` returns `nullptr` with `lastError()` at
> `ETIMEDOUT` when nobody showed up. `tryAccept()` is the clearer way to say "only if someone
> is waiting".

> [!NOTE]
> **A connection is a stream of bytes, not of messages.** Two writes can come out of one read
> and the other way round; a protocol with messages has to frame them itself.

> [!WARNING]
> **A write to a peer that has gone does not raise SIGPIPE here.** `send()` goes out with
> `MSG_NOSIGNAL` where that exists, and `SO_NOSIGPIPE` is set where it does not, so the write
> reports the failure instead of killing the process.

> [!NOTE]
> **Waiting is done with `poll()`, and the descriptor is exposed.** A server that wants to
> serve many connections from one thread can `poll()` the `native_handle()`s itself; this
> module starts no threads and does not do that for you.

## See Also

- [bytearray](bytearray.md) — what `read()` and `write()` move
- [process](process.md) — the other POSIX-only module: pipes and child processes
