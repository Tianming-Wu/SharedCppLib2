# pipe - Named Pipes

+ Name: pipe
+ Namespace: `scl2::pipe`
+ Document Version: `1.2.0`

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
server.setClientLimit(2);

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

    client.waitForFinished();                            // the client has it; closing cannot lose it
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

client.waitForFinished();
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
waiting now, and immediately starts listening for the next one, so accepting never waits on
the connections that are already being served.

That is what makes thread-per-connection work without the library doing anything for it. An
accepted `server_client` owns its handle and its buffer, nothing is shared between two of
them, and the server keeps no copy of it: the returned object *is* the connection, and it is
move-only, so it can be handed to a worker thread. The one thing that has to stay on the
accepting thread is the `server` itself — `queryNextConnection()`, `stop()` and `cleanup()`
all touch its members.

## Reference

### server

| Member | Description |
|---------|---------|
| `server(name)` | Prepare a server for that name. Nothing is created yet |
| `server(name, permissions)` | The same, with an explicit security descriptor |
| `start()` | Create the pipe and begin listening. `false` on failure |
| `stop()` / `cleanup()` | Stop listening (releasing the waits of the connections it handed out) and close the handles |
| `active()` | Whether it is listening |
| `stopped()` | Whether it has been stopped |
| `queryNextConnection()` | Wait for, and take, the next client. Returns an invalid `server_client` when there is none. This call blocks, so `hasPendingConnection()` / `waitForNextConnection(timeout)` tell you when it is worth making |
| `hasPendingConnection()` | Whether a connection is waiting right now |
| `waitForNextConnection(timeout = 5s)` | Wait until one is |
| `setBufferSize(size)` / `bufferSize()` | Per-connection I/O buffer, 4 KiB by default |
| `setPermissions(p)` / `getPermissions()` | Which security descriptor new connections are created with |
| `setPipeMode(m)` / `getPipeMode()` | What the pipe carries: the value new clients detect |
| `setClientLimit(n)` / `clientLimit()` | How many clients may be connected at once |
| `maximumClientLimit` | 254, the largest value `setClientLimit()` accepts |

`setBufferSize()`, `setPermissions()`, `setPipeMode()` and `setClientLimit()` throw
`std::runtime_error` while the server is active: these are decided when the pipe is created,
so they can only be set before `start()`.

### server_client

| Member | Description |
|---------|---------|
| `valid()` | Whether the connection is still open |
| `broken()` | Whether an operation has found the connection gone |
| `setThrowOnBroken(enabled)` / `throwOnBroken()` | Whether a failing operation should throw `connection_broken`. Off by default |
| `cancel()` / `cancelled()` | Release the waits on this connection, and whether that was asked for |
| `readyRead()` | Whether there is something to read now |
| `waitForReadyRead(timeout = 5s)` | Wait until there is |
| `available()` | How many bytes are waiting |
| `read(bytes)` | Up to `bytes` bytes. Empty in `mode::Message` |
| `readAll()` | One whole message, reassembled when it is larger than the buffer |
| `write(data)` | One write, or one message. The number of bytes written |
| `waitForFinished(timeout = 5s)` | Wait until the peer has read everything written so far, so that closing cannot discard anything. A negative timeout waits as long as it takes |
| `close()` | End this connection; `false` when there was nothing left to end. The object stays usable as an invalid client |
| `reset()` | The stream interface name for dropping the connection: release the handle and return to an invalid state. Answers `true` even when there was nothing to release |
| `bufferSize()` | The buffer this connection uses; it follows the server, and is not settable |
| `nativeHandle()` | The native handle behind this connection, as a `void*`. The connection owns it: peer inspection only, never close it. `GetNamedPipeClientProcessId()` reads it, `ImpersonateNamedPipeClient()` acts as its user and needs a matching `RevertToSelf()` |

### client

| Member | Description |
|---------|---------|
| `connect(timeout = 5s)` | Connect to the server, waiting at most `timeout`. Retries while the name does not exist yet and while every instance is busy |
| `waitForConnection(timeout = 5s)` | The same as `connect(timeout)`; the clearer name when the server is expected to start later |
| `serverExists()` | Whether something is listening on the name now. `false` only when the name does not exist |
| `valid()` / `broken()` | As on `server_client` |
| `setThrowOnBroken(enabled)` / `throwOnBroken()` | As on `server_client` |
| `cancel()` / `cancelled()` | As on `server_client`; a client has no server, so only its own cancel applies |
| `readyRead()` / `waitForReadyRead(timeout = 5s)` / `available()` | As on `server_client` |
| `read(bytes)` / `readAll()` / `write(data)` | As on `server_client` |
| `waitForFinished(timeout = 5s)` | As on `server_client` |
| `pipeMode()` | The mode the server announced, filled in by `connect()` |
| `setPipeMode(m)` | Override it, for example to read a message pipe chunk by chunk |
| `close()` | Close the connection; `false` when there was nothing left to close |
| `reset()` | As on `server_client` |

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
| `Administrators` | Administrators and LocalSystem, through `D:(A;;GA;;;BA)(A;;GA;;;SY)`. For a privileged channel that the processes the ordinary channel serves must not reach |

## One server per name

`start()` creates its first pipe instance with `FILE_FLAG_FIRST_PIPE_INSTANCE`, so a name that
is already being served - by another server in this process, or by a server in another one -
makes it fail. `start()` answers `false` and `GetLastError()` is `ERROR_ACCESS_DENIED` (5).

The instances created afterwards, for the next connection, do not carry the flag: by then the
name exists because of the server that claimed it. Two processes serving one name would mean
the operating system decides which of them a client reaches, which is not what either side
asked for.

## Broken connections

The peer can go away at any moment: it may crash, and a pipe has no way to warn you first.
Every operation that can notice records it, and once recorded it stays:

| Call | What it answers when the peer is gone |
|---------|---------|
| `waitForReadyRead()` | `false`, exactly like a timeout |
| `read()` / `readAll()` | Empty, or the part that arrived before the break |
| `write()` | 0 |
| `waitForFinished()` | `false` |
| `available()` / `readyRead()` | Whatever is still buffered; looking alone records the state |
| `valid()` | `false`, once `broken()` is set |

So a return value on its own does not say whether `false` or empty means "nothing yet" or
"nothing ever again". `broken()` is what separates the two, and it is worth one check after a
call that failed - not after every call:

```cpp
if (!client.waitForReadyRead(std::chrono::seconds(5))) {
    if (client.broken()) {
        log("client is gone");            // it crashed, or closed its end
    } else {
        continue;                          // it was only a timeout
    }
}
```

A message cut in half is the case that matters most: `readAll()` records the state *before*
handing back what it managed to read, so a partial message is never mistaken for a complete
one.

If you would rather not check anything, a connection can be told to throw instead:

```cpp
client.setThrowOnBroken(true);

try {
    while (running) {
        if (!client.waitForReadyRead(std::chrono::seconds(5)))
            continue;

        handle(client.readAll());
    }
} catch (const scl2::pipe::connection_broken& e) {
    log(e.what());                             // the Win32 text is in there
}
```

`connection_broken` derives from `std::runtime_error`, is off by default, and is set per
connection. It is off by default because a peer dying is not a mistake in your code: a program
that does not catch it dies at the moment it was going to handle the failure. Turn it on where
a `try` around the handler is what you want anyway.

## Cancelling a wait

A thread inside `waitForReadyRead()` cannot be told to stop - unless the wait watches something
else too, which is what `cancel()` is for:

```cpp
client.cancel();                              // any wait on it answers false right away
```

`server::stop()` does the same for every connection the server handed out, which is what makes
a shutdown work without waiting out timeouts and without killing threads:

```cpp
// the accept loop, or any other thread
server.stop();          // every worker's wait ends, and server.stopped() turns true
```

A cancel is sticky: the connection stays cancelled until `reset()`. It closes nothing, it never
throws, and it is the one call that is meant to be made from another thread.
`waitForFinished()` stops waiting on a cancel as well, but its `FlushFileBuffers` has already
been started and Windows offers no way to cancel that call - the helper thread ends on its own
once the peer reads, or once the connection closes.

## Notes

> [!NOTE]
> **A connection is closed with `close()`, and `close()` does not flush.** It disconnects the
> pipe, and a disconnect discards whatever the other end has not read yet, so a write followed
> by a close can lose its last message. Dropping the object does the same through the
> destructor. `waitForFinished()` is what makes closing safe.

> [!NOTE]
> **`waitForFinished()` is the handshake a pipe does not give you.** It returns only after the
> peer has read everything written so far, which is the same thing `FlushFileBuffers` does to
> the handle: the kernel keeps track of who has read what, so nothing is put into the data
> stream and the other end needs no cooperation at all.

> [!WARNING]
> **Both ends flushing at once deadlock.** Each flush waits for the other end to read, so if
> both sides flush while each still owes the other a read, neither ever returns. Read what you
> are waiting for first, then flush, then close.

> [!NOTE]
> **`broken()` is a lazy flag.** It is set by a failed operation and is never cleared, so it
> is only meaningful after an operation has failed. There is no way to probe the connection
> with it.

> [!NOTE]
> **A flush that answers `false` is not always a broken connection.** With a positive timeout
> the wait can simply run out (`waitForFinished()` reports `false` and leaves the connection
> usable); a flush on a connection the peer has already closed fails, and marks it broken.

> [!NOTE]
> **`acknowledge()`, `waitForAcknowledged()` and `acknowledge_token` are deprecated.** They
> put a token byte into the pipe, which a byte-mode reader would find in its data. A promise
> from the kernel that the peer has read is both simpler and invisible.

> [!WARNING]
> **254 clients, not 255.** Windows caps the instances of one pipe name at 255, and
> `queryNextConnection()` briefly holds two at once while it hands one out and listens for
> the next. `maximumClientLimit` is therefore 254, and that is the largest value
> `setClientLimit()` takes.

> [!NOTE]
> **`stop()` releases the connections it handed out, too.** They hold a copy of the server's
> stop event and watch it while they wait, so a worker inside `waitForReadyRead()` comes back
> right away instead of waiting out its timeout; `waitForNextConnection()` returns `false` as
> well. Nothing is closed by this - what to do with the connection stays the worker's decision.

> [!NOTE]
> **The name is machine-wide, not per-user.** Two processes only meet if they agree on the
> name exactly; nothing about the server's identity is part of it. Use `permissions` to
> decide who may connect.

## See Also

- [bytearray](bytearray.md) — what `read()` and `write()` move
- [process](process.md) — anonymous pipes, and the same `scl2::basic_iostream` model, for a child process
- [unixsocket](unixsocket.md) — the POSIX counterpart: local sockets instead of named pipes
