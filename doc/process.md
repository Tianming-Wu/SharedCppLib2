# Process

`scl2::process` starts an external program and talks to its standard streams through
pipes. The interface follows Qt's `QProcess`, adapted to this library's synchronous,
event-loop-free style.

* header: `process.hpp` &nbsp;·&nbsp; source: `process.cpp`
* link target: `SharedCppLib2::process`
* namespaces: `scl2::process`, `scl2::process_stream`
* platforms: Windows and Unix (Linux / macOS)

```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::process)
```

```cpp
#include <SharedCppLib2/process.hpp>
```

> **Note:** the Unix side was written together with the Windows one but has not been
> verified on real hardware yet. The API and the documented behaviour are the same on
> both; treat places where the two diverge (`terminate`, `detach`, `startDetached`) as
> provisional until that check has happened.

---

## Quick start

```cpp
#include <SharedCppLib2/process.hpp>

scl2::process p("git", {"rev-parse", "HEAD"});

if (!p.start()) {
    std::fprintf(stderr, "cannot start: %s\n", p.errorString().c_str());
    return;
}

if (!p.waitForFinished(std::chrono::seconds(10))) {
    std::fprintf(stderr, "git did not finish in time\n");
    return;
}

scl2::bytearray raw = p.readAllStandardOutput();
std::string hash(reinterpret_cast<const char*>(raw.data()), raw.size());
```

---

## The model

### `process` *is* a stream

`scl2::process` inherits `scl2::basic_iostream`, so it can be passed to anything that
takes a stream, without an adapter:

| operation | goes to |
|---|---|
| `read()`, `readAll()`, `available()`, `readyRead()` | the child's **current read channel** (stdout by default) |
| `write()` | the child's **stdin** |

`setReadChannel()` moves the read side to stderr, which is how you read stderr through
the inherited functions instead of the dedicated accessors.

### The two output channels

`channel_mode` decides how the child's stdout and stderr are wired:

| mode | meaning |
|---|---|
| `channel_mode::separate` | each gets its own pipe. **This is the default.** |
| `channel_mode::merged` | stderr is redirected into the stdout pipe. The child's own write order is preserved, but the two channels can no longer be told apart: `readAllStandardError()` returns nothing and `stderrStream()` is `nullptr`. |

`setChannelMode()` has no effect while the child is running.

### `process_stream`

Every pipe is wrapped in a `process_stream`, a `basic_iostream` bound to exactly one
native handle: a read-only end (stdout / stderr) or a write-only end (stdin). Reading a
write-only stream, or writing a read-only one, reports zero bytes instead of failing.

A channel stream can be **torn off**: `detachStdError()` (and the stdout / stdin twins)
hands the `shared_ptr` over to you. The process then stops owning that channel, which
means it is

* no longer drained by `pump()`,
* no longer waited on by `waitForFinished()` / `waitForReadyRead()`,
* no longer closed by us.

The process keeps a `weak_ptr`, so `stderrStream()` reports `nullptr` once you drop it,
and `attached(channel)` answers whether the process is still in charge of a channel.

---

## Reference

### Configuration

| member | description |
|---|---|
| `setPath(path)` / `path()` | the program. A bare name is looked up in `PATH`; a path with a directory part must exist |
| `setArguments(stringlist)` / `arguments()` | arguments, one per element |
| `setWorkingDirectory(path)` / `workingDirectory()` | the child's working directory. Empty means "inherit ours" |
| `setChannelMode(mode)` / `channelMode()` | see above; ignored while running |

### Lifecycle

| member | description |
|---|---|
| `start()` | launch. Returns `false` and sets `error()` on failure |
| `static startDetached(program, args, working_dir)` | launch without any pipe and forget about it. The child outlives the call |
| `detach()` | release our handles and leave the child running (pipes are closed, which the child sees as EOF) |
| `terminate()` | ask the child to stop, and make sure it does. Windows: `TerminateProcess`. Unix: `SIGTERM`, escalated to `SIGKILL` after a few seconds |
| `kill()` | take the child down without giving it a chance to clean up. Unix: `SIGKILL`. Windows: same as `terminate()` |
| `running()` / `finished()` | state. Both are `const` |
| `exitcode()` | the exit code, or `-1` if the child has not exited. On Unix a child killed by a signal also reports `-1` |
| `processId()` | pid, or `0` if it was never started |
| `error()` / `errorString()` | why the last `start()` or wait function failed |
| `reset()` | terminate if needed, close everything, drop all buffers: the object goes back to its freshly constructed state and can be `start()`ed again. Idempotent, and does not clear `error()` |

`start()` fails with `error_code::already_running` when a child is still alive; call
`waitForFinished()` or `reset()` first. `process` is move-only, and the destructor kills
the child if it is still running.

### Reading

| member | description |
|---|---|
| `setReadChannel(channel)` / `readChannel()` | which channel the inherited read functions use. Ignored in `merged` mode |
| `readyRead()` / `available()` / `read(bytes)` / `readAll()` | the current read channel |
| `readyReadStandardOutput()` / `readyReadStandardError()` | per channel |
| `availableStandardOutput()` / `availableStandardError()` | per channel |
| `readAllStandardOutput()` / `readAllStandardError()` | per channel |

### Writing

| member | description |
|---|---|
| `write(bytearray)` | push data into the child's stdin; returns the number of bytes actually written |
| `closeWriteChannel()` | close the child's stdin, so that it sees EOF |

### Channel streams

| member | description |
|---|---|
| `stdinStream()` / `stdoutStream()` / `stderrStream()` | the channel's `shared_ptr`, or `nullptr` when it does not exist (merged stderr) or has been torn off |
| `detachStdInput()` / `detachStdOutput()` / `detachStdError()` | take ownership of a channel |
| `attached(channel)` | whether the process still owns and drains that channel |

### Waiting

| member | description |
|---|---|
| `waitForFinished(timeout = 5s)` | wait for the child to exit, draining its output into our buffers |
| `waitForReadyRead(timeout = 5s)` | wait until the current read channel has data |

Both drain **both** output channels while they wait, so a chatty child can never deadlock
on a full pipe. There is no event loop and no signal: poll with these, or with
`readyRead()`.

---

## Buffering

A pipe only holds about 64KB. If `waitForFinished()` merely waited for the child, any
program that writes more than that would block on a full pipe and never finish — so the
process drains both channels into per-channel buffers while it waits, and the read
functions serve from those buffers.

The consequence worth knowing: **while the process owns a channel it also buffers it.**
Reading from `stdoutStream()` by hand therefore competes with `readAllStandardOutput()`
for the same bytes. Pick one:

* use the process-level read functions, or
* `detachStdOutput()` first, and then drive the stream yourself.

`available()` and `readyRead()` pull from the pipes into the buffer first, so plain
polling works without calling a wait function.

---

## Error handling

`start()` and the wait functions report failure through `error()` rather than exceptions:

| `error_code` | meaning |
|---|---|
| `no_error` | nothing has failed |
| `failed_to_start` | the program could not be launched (missing file, no `PATH` hit, no permission…) |
| `already_running` | `start()` was called while a child was still alive |
| `timed_out` | a wait function hit its timeout |
| `write_error` | nothing (or only part of the data) could be written to the child's stdin |
| `read_error` | reading from the child's output failed |
| `unknown_error` | everything else |

`errorString()` carries the platform's own message (a Win32 error text on Windows,
`strerror` text on Unix).

---

## Platform notes

| operation | Windows | Unix |
|---|---|---|
| pipes | anonymous pipes, `PeekNamedPipe` / `WaitForMultipleObjects` | `pipe()` + `FIONREAD` / `poll()` |
| `terminate()` | `TerminateProcess`, cannot be refused | `SIGTERM`, then `SIGKILL` after a few seconds |
| `kill()` | identical to `terminate()` | `SIGKILL` |
| program lookup | `CreateProcessW`, so a bare name goes through `PATH` | `execvp`, same rule |
| arguments | packed into one command line with `stringlist::pack()` (`unpack()` is its inverse) | passed as a real `argv` array, so quoting is never involved |
| `startDetached()` | no pipe is inherited, the child keeps sharing our console | double fork + `setsid()`, so the child is reparented to init |
| `detach()` | closing our handles leaves the child running | nobody reaps the child any more, so it becomes a zombie once it exits — keep `processId()` and wait for it yourself if that matters |
| SIGPIPE | does not exist | blocked around every write, so a child that closed its stdin cannot kill us |

---

## Things to watch out for

* `write()` blocks when the child's stdin pipe is full (about 64KB). There is no
  asynchronous write; a child that never reads its input will stall the caller.
* After `detach()` the child can no longer be waited for, read from or terminated.
  Its exit code is out of reach.
* `readAll()` on the process drains the current read channel only — that is why the
  per-channel accessors exist.
* The moved-from object of a moved `process` is inert: it reports no pid, is neither
  running nor finished, and will not touch the child when it is destroyed.

---

## Limits / roadmap

* No callbacks or signals; there is no event loop to deliver them from.
* No `setEnvironment()`, no output redirection to a file, no channel forwarding
  (a child sharing our console/stdout verbatim).
* `exitcode()` does not distinguish "killed by a signal" from "not exited yet" — both
  are `-1` on Unix.
* Unix support still needs its first run on real hardware.

---

## Standalone usage

`process` is a normal library target, not a `[SCL_STANDALONE_MODULE]`: it depends on
`basic` (`stringlist`, `bytearray`, `str_to_wstr`) and, on Windows, on `platform` for
`TranslateErrorW`. Copy `process.hpp` / `process.cpp` into a project that already has
those, or just link the target.
