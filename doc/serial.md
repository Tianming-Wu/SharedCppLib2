# serial - serial ports

+ Name: serial
+ Namespace: `scl2`
+ Documentation version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `serial` |
| Dependencies | `basic` (Windows also needs `platform` and `advapi32`, both private) |

How to use it:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::serial)
```

```cpp
#include <SharedCppLib2/serial.hpp>
```

## Introduction

`scl2::serial` is one open serial port, exposed as a `scl2::basic_iostream`. That means it
takes part in everything the rest of the library does with streams: `read()` takes bytes,
`write()` sends them, `available()` says how many are waiting, and `waitForReadyRead()` blocks
until data arrives.

A serial port differs from a pipe or a socket in one way that shapes the whole API: there is
no peer and no message boundary. The port *is* the connection, so there is one object rather
than a server and a client, and a read is a number of bytes rather than a message.

Windows (the COMM API) and POSIX (termios) are both implemented behind the same header. The
header names no platform type - the native handle travels as an opaque `std::intptr_t`, the
same way `process.hpp` does it - so portable code just includes it and links the target.

## Quick start

```cpp
#include <SharedCppLib2/serial.hpp>

scl2::serial port;

if (!port.open("COM3")) {                       // 115200 8N1, no flow control
    std::fprintf(stderr, "cannot open: %s\n", port.lastError().c_str());
    return;
}

port.write(scl2::bytearray("AT\r\n"));

if (port.waitForReadyRead(std::chrono::seconds(1))) {
    scl2::bytearray reply = port.readAll();
}
```

On POSIX the name is a device node: `port.open("/dev/ttyUSB0")`.

## Opening and closing

| Call | Meaning |
|------|---------|
| `open(name, settings)` | Claim the port with these line settings; replaces whatever the object held |
| `close()` / `reset()` | Release it; idempotent, always answers `true` |
| `isOpen()` / `valid()` | Whether a port is currently held |
| `portName()` | The name it was opened with, in the form it was given |
| `nativeHandle()` | The `HANDLE` or file descriptor behind it |

Windows opens the port with no sharing, so a port another program holds cannot be opened.
The name is kept even when opening fails, so `portName()` and `lastError()` say what was
tried and why it did not work.

## Line settings

```cpp
scl2::serial::settings config;
config.baud_rate = 921600;
config.data_bits = 8;
config.parity_bit = scl2::serial::parity::Even;
config.stop = scl2::serial::stop_bits::One;
config.flow = scl2::serial::flow_control::None;

port.open("COM4", config);
port.applySettings(config);          // change them later, keeping the same open port
scl2::serial::settings current = port.currentSettings();
```

| Field | Values | Notes |
|-------|--------|-------|
| `baud_rate` | any positive rate | POSIX accepts only the rates its constants name |
| `data_bits` | 5 .. 8 | |
| `parity_bit` | `None`, `Odd`, `Even`, `Mark`, `Space` | mark / space need Windows or Linux |
| `stop` | `One`, `OnePointFive`, `Two` | `OnePointFive` is Windows only |
| `flow` | `None`, `Hardware` (RTS/CTS), `Software` (XON/XOFF), `Both` | |

`applySettings()` answers `false` and fills `lastError()` when the platform cannot express a
setting - 1.5 stop bits or mark parity on POSIX, for example. The port keeps the settings it
had.

## Timeouts

| Read timeout | `read()` waits |
|--------------|----------------|
| positive (default 1 s) | that long for the bytes it was asked for, then returns what arrived |
| `0` | not at all: takes whatever is already queued |
| negative | without a limit |

| Write timeout | `write()` waits (Windows) | (POSIX) |
|---------------|---------------------------|---------|
| positive (default 1 s) | that long | that long |
| `0` / negative | no limit | no wait / no limit |

The asymmetry is the driver's: the Windows COMM API has no way to ask for a write that
returns at once, so a zero there can only mean "no timeouts".

A write that runs out of time returns the bytes the driver took - possibly none - and leaves
`lastError()` alone, because nothing failed. Compare the result with the size you passed in to
find out where to continue.

## Reading and writing

| Call | Behaviour |
|------|-----------|
| `read(n)` | Waits up to the read timeout and returns what arrived, at most `n` bytes |
| `readAll()` | Takes everything queued right now, without waiting |
| `write(data)` | Returns the bytes the driver took; a partial write says where to continue |
| `waitForReadyRead(t)` | `true` once bytes are waiting, or once the port has gone away |
| `waitForBytesWritten(t)` | `true` once the driver's output queue is empty |

A serial port has no waitable handle, so `waitForReadyRead()` polls the driver every 2 ms. It
is the one place where the wait is not as precise as the timeout suggests.

`waitForReadyRead()` promises that *something* arrived, not that everything you expect has.
A loopback test that writes 4096 bytes gets `true` as soon as the first byte comes back;
ask for what you need with `read(n)`, or loop until `available()` has what you are waiting
for.

## Lines

| Call | Meaning |
|------|---------|
| `purge(purge_target::Receive/Transmit/Both)` | Throw away what the driver has queued |
| `bytesToWrite()` | How much is still queued for transmission (0 when unknown) |
| `modem()` | The input lines as a `modem_lines`: `cts`, `dsr`, `ri`, `dcd` |
| `setDtr(bool)` / `setRts(bool)` | Drive the output lines by hand |
| `sendBreak(duration)` | Hold the break condition for a moment |
| `waitForBytesWritten(t)` | Wait until nothing is left in the output queue |

On Windows hardware flow control drives RTS itself, so `setRts()` is undone while
`flow_control::Hardware` or `Both` is in effect. `sendBreak()` honours the duration on
Windows; POSIX sends the system's own break and ignores it.

## Port names and enumeration

| Platform | Name | `availablePorts()` |
|----------|------|--------------------|
| Windows | `COM3` (the `\\.\` prefix that COM10 and up need is added for you) | the registry's device map |
| POSIX | `/dev/ttyUSB0` | the usual device names under `/dev` |

`serial::availablePorts()` is a static call and returns a naturally sorted list (`COM2` before
`COM10`). `serial::portExists(name)` answers whether the name exists without opening - and so
without claiming - the port.

## Failures

Nothing throws. A failed call answers `false` (or a short read, or zero written bytes) and
records why:

```cpp
if (!port.open("COM9")) {
    std::fprintf(stderr, "%u: %s\n", port.lastErrorCode(), port.lastError().c_str());
}
```

| Call | Meaning |
|------|---------|
| `lastError()` | What went wrong, in the system's own words; empty when nothing has |
| `lastErrorCode()` | The Win32 error or `errno`; 0 when the failure was this module's own |
| `broken()` | Whether an operation has found the port gone |

`broken()` is the answer to "was that a timeout or did the device disappear". A USB adapter
that is unplugged cannot be waited on or retried, so once an operation has seen it,
`readable()` keeps answering `true` and reads come back empty - the caller gets to notice
instead of waiting for ever. `open()` clears the flag.

On Windows an unplugged adapter shows up as `ACCESS_DENIED` (5) on the next operation, and
`availablePorts()` stops listing it; every call that touches the port after that answers
`false` (or nothing) with the same error until the port is closed.

## Platform differences

| Topic | Windows | POSIX |
|-------|---------|-------|
| Backend | COMM API (`CreateFile`, `DCB`, `COMMTIMEOUTS`) | `termios`, `poll()`, `ioctl` |
| Wait for data | polls `ClearCommError` | `poll(POLLIN)` |
| Write with no limit | `0` and negative both | negative only |
| 1.5 stop bits | supported | rejected by `applySettings()` |
| Mark / space parity | supported | Linux only (`CMSPAR`) |
| `sendBreak` duration | honoured | ignored |
| Port enumeration | registry | directory scan |

## Limitations

- **The POSIX branch has not been compiled or tried on a real port yet.** It is written, but
  this module was developed and built on Windows; treat `termios` behaviour as unverified
  until it has been run.
- `waitForReadyRead()` polls, so it costs a wake-up every 2 ms while it waits.
- There is no flush that forces bytes onto the wire; `waitForBytesWritten()` waits for the
  driver's queue to empty instead.
- Modem event notifications (`WaitCommEvent`) are not exposed; nothing here runs a thread of
  its own, which is the same shape `pipe` and `process` have.
