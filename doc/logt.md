# logt - Asynchronous Logging Library

+ Name: logt  
+ Namespace: none  
+ Document Version: `1.2.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `logt` |

Include usage:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::logt)
```

## Description

Logt means "log threaded" — a high-performance asynchronous logging library for C++23.
It uses a dedicated logging worker thread for all write operations, so application threads are not blocked by logging I/O.
This design is suitable for performance-sensitive applications.

> [!WARNING]
> This library uses a complex wrapping model. Reading source code alone may not be enough to fully understand intended usage. Please follow this document.

## Main Features

- **Fully asynchronous architecture**: all writes happen in the background worker thread.
- **Thread context capture**: automatically includes claimed thread names (or thread id fallback).
- **Multiple output channels**: stdout, file channels, and custom `std::ostream` channels.
- **Signature-based routing**: class/module/function level signatures.
- **High-precision timestamps**: optional millisecond/microsecond timestamp mode.
- **Extensible preprocessing**: supports message preprocessors (for example color formatting).

## Quick Start

### Basic usage

```cpp
#include <SharedCppLib2/logt.hpp>

// Module signature for this cpp file.
// Not the recommended style; see "Log Signatures" section.
LOGT_MODULE("Main");

int main() {
    logt::stdcout(true, true);
    logt::claim("MainThread");

    logt.info() << "Application initialization finished";
    logt.warn() << "Non-critical issue detected";
    logt.error() << "Operation failed, error code: " << error_code;

    logt::shutdown();
    return 0;
}
```

### Class usage

```cpp
class DatabaseManager {
    LOGT_DECLARE

public:
    void connect() {
        logt.info() << "Opening database connection...";
        logt.debug() << "Connection parameters: " << connection_params;
    }
};

// In implementation file:
LOGT_DEFINE(DatabaseManager, "Database");
```

### Function-level logging

```cpp
// network.cpp

void send_data() {
    LOGT_LOCAL("Network::send_data"); // Recommended signature style.

    logt.info() << "Start packet transmission";
    logt.debug() << "Packet details - size: " << packet_size << " bytes";
}
```

### Enable debug mode
```cpp
#include <SharedCppLib2/logt.hpp>
#include <SharedCppLib2/arguments.hpp>

int main(int argc, char** argv) {
    std::arguments args(argc, argv);

    // ... other log initialization code ...

    if(args.addFlag("debug")) {
        logt::setFilterLevel(LogLevel::Debug);
    }

    LOGT_LOCAL("main");
    logt.debug() << "Debug mode enabled";

    logt::shutdown();
    return 0;
}
```

### Exit when execution cannot continue
```cpp
void some_critical_function() {
    LOGT_LOCAL("CriticalFunction");

    if(!initialize_critical_resource()) {
        logt.fatal() << "Critical resource initialization failed, program will exit";
        logt::exit(EXIT_FAILURE);
    }

    // ...
}
```

### logt_guard - RAII shutdown helper

```cpp
logt_guard guard;  // Automatically calls logt::shutdown() on destruction
```

A convenience RAII class whose destructor calls `logt::shutdown()`. Declaring a `logt_guard` instance at the appropriate scope (e.g. in `main()`) ensures the logging system is properly shut down when it goes out of scope.

> [!NOTE]
> Using `logt_guard` is entirely optional. If you choose not to use it, you **must** call `logt::shutdown()` before program exit, and `shutdown()` must be called from the last exiting thread. Failing to call `shutdown()` will trigger `std::terminate()` when `main()` exits, because the worker thread is still running.

**Example:**

```cpp
#include <SharedCppLib2/logt.hpp>

LOGT_MODULE("Main");

int main() {
    logt_guard guard;  // shutdown() will be called automatically

    logt::stdcout(true, true);
    logt::claim("MainThread");

    logt.info() << "Application initialized";

    // No need to call logt::shutdown() explicitly
    return 0;
}
```

## Core API Reference

### System configuration methods

#### addfile - add file channel
```cpp
static int addfile(const std::filesystem::path& filename, bool default_enable = true);
```
Opens a file channel in append mode and returns channel id. `default_enable` controls whether new signatures enable this channel by default. Returns `-1` if channel slots are exhausted.

#### addostream - add custom stream
```cpp
static int addostream(std::ostream& os, bool default_enable = true);
```
Registers a custom output stream and returns channel id. The provided stream must remain valid during logging. logt does not own it.

#### stdcout - console channel control
```cpp
static void stdcout(bool enable = true, bool default_enable = true);
```
Controls channel 0 (stdout). This channel always exists and can be disabled or removed from default channel set.

#### claim - thread naming
```cpp
static void claim(const std::string& name);
```
Sets a readable name for current thread. If a thread is not claimed, logt falls back to thread id display.

#### setFilterLevel - log filtering
```cpp
static void setFilterLevel(LogLevel level);
```
Sets minimum queued level. Levels: `LogLevel::Debug`, `LogLevel::Info`, `LogLevel::Warn`, `LogLevel::Error`, `LogLevel::Fatal`, special `LogLevel::Quiet`.

#### enableSuperTimestamp - high precision timestamp
```cpp
static void enableSuperTimestamp(bool enabled);
```
When enabled, timestamps include milliseconds and microseconds.

#### install_preprocessor - preprocessor
```cpp
static void install_preprocessor(preprocessor_t preprocessor);
```
Installs preprocessing hook.
Preprocessing currently happens in `write_message()`: stdout/custom streams use processed content (can keep color), while file channels use original unprocessed content (to avoid ANSI codes in files).

#### shutdown - graceful shutdown
```cpp
static void shutdown();
```
Gracefully stops worker thread and flushes pending messages. **Must be called before program exit.** Must be called from the last exiting thread. Failing to call `shutdown()` will trigger `std::terminate()` when `main()` exits.

#### logt_guard - RAII shutdown guard
```cpp
class logt_guard {
public:
    logt_guard() = default;
    ~logt_guard() { logt::shutdown(); }
};
```
A simple RAII wrapper that calls `shutdown()` automatically on destruction. This is a convenience alternative to calling `logt::shutdown()` manually. If you use `logt_guard`, no explicit `shutdown()` call is needed.

#### exit - shutdown + process exit
```cpp
static void exit(int exitcode);
```
Calls `shutdown()` and then exits process.

### Logging methods

Log level methods are provided through `logt_sig`:

#### info
```cpp
logt_sso info() const;
```
Creates INFO level message stream.

#### warn
```cpp
logt_sso warn() const;
```
Creates WARN level message stream.

#### error
```cpp
logt_sso error() const;
```
Creates ERROR level message stream.

#### fatal
```cpp
logt_sso fatal() const;
```
Creates FATAL level message stream.

#### debug
```cpp
logt_sso debug() const;
```
Creates DEBUG level message stream. With default filter (`LogLevel::Info`), debug messages are dropped.

## Macro Reference

Note: due to current implementation constraints, `LOGT_LOCAL` is strongly recommended. See "Log Signatures" section for details.

At main entry points, place `LOGT_LOCAL` after channel initialization, otherwise channel reuse optimization may produce unexpected channel configuration behavior. This area is under refactoring.

### LOGT_DECLARE
Declares class static log signature (private).

### LOGT_PUBLIC_DECLARE
Public version of `LOGT_DECLARE` for external channel adjustment.

### LOGT_DEFINE(Class, Name)
Defines class signature in implementation file.

### LOGT_MODULE(Name)
Defines module-level signature (global / namespace scope).

### LOGT_LOCAL(Name)
Defines function-scope local signature.

### LOGT_TEMP(Name)
Defines one-shot temporary signature object.

### LOGT_LINEINFO
```cpp
#define LOGT_LINEINFO std::format("{}:{} {}", __FILE__, __LINE__, __FUNCTION__)
```
Convenience macro for embedding source location in log messages.

## Channel Management

- **Channel 0**: stdout is always registered; controlled via `stdcout(enable, default_enable)`.
- **Adding channels**: `addfile()` / `addostream()` return ids and define default enable state for new signatures.
- **Per-signature control**: `logt_sig::setChannel(id, enable)` / `setChannels({ids...})` return `bool` and validate registration.
- **Public access**: if external code must modify class channels, use `LOGT_PUBLIC_DECLARE`.
- **Preprocessor scope**: preprocessing is applied in `write_message()`; stdout/custom streams keep processed content, file channels keep original content.

## Log Levels

- `LogLevel::Quiet` = -1 — log nothing.
- `LogLevel::Debug` = 0 — detailed debug information.
- `LogLevel::Info` = 1 — normal runtime information.
- `LogLevel::Warn` = 2 — potentially abnormal conditions.
- `LogLevel::Error` = 3 — error conditions.
- `LogLevel::Fatal` = 4 — unrecoverable failures.

## Complete Example

```cpp
#include <SharedCppLib2/logt.hpp>
#include <SharedCppLib2/logc.hpp>
#include <thread>

class DataProcessor {
public:
    void process() {
        LOGT_LOCAL("DataProcessor::process");

        logt.info() << "Start data processing";
        logt.debug() << "Input size: " << input_data.size();
        // ...
    }
};

int main() {
    int file_channel = logt::addfile("application.log");
    logt::stdcout(true);
    logt::claim("MainThread");
    logt::setFilterLevel(LogLevel::Debug);
    logt::enableSuperTimestamp(true);
    logt.setChannels({0, file_channel});

    LOGT_LOCAL("main");

    // logt::install_preprocessor(logc::logPreprocessor);

    logt.info() << "Main application initialized";
    logt.warn() << "Using default configuration";

    DataProcessor data_processor;
    data_processor.process();

    std::thread worker_thread([](){
        logt::claim("WorkerThread");
        LOGT_LOCAL("Worker");

        for(int task_id = 0; task_id < 5; task_id++) {
            logt.info() << "Execute task: " << task_id;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    worker_thread.join();
    logt.info() << "All tasks completed";

    logt::shutdown();
    return 0;
}
```

## Log Signatures

Log signatures are source labels attached to each message in logt. They can be defined at class/module/function scope to make origin tracing explicit.

However, due to current design limitations, `LOGT_MODULE` and `LOGT_DECLARE` are not recommended for now because their scope is broader and less precise for troubleshooting.
Prefer defining local signatures with `LOGT_LOCAL` inside functions for clearer and more accurate log origins.

## Output Format

Standard format:
```
[YYYY/MM/DD HH:MM:SS] [LEVEL] [THREAD] [SIGNATURE] Message content
```

With high-precision timestamp enabled:
```
[YYYY/MM/DD HH:MM:SS.MMM.UUU] [LEVEL] [THREAD] [SIGNATURE] Message content
```

Field meaning:
- Timestamp: date/time of log generation.
- Level: severity marker (`DEBUG/INFO/WARN/ERROR/FATAL`).
- Thread: claimed thread name (or thread id fallback).
- Signature: source signature (module/class/function).
- Message: actual user log payload.

## Performance Recommendations

- **Async advantage**: logging does not block caller threads, use it to keep critical paths responsive.
- **Preprocessing cost**: complete heavy formatting before enqueueing where possible.
- **Production filter**: consider `setFilterLevel(LogLevel::Warn)` or higher in production.
- **Shutdown discipline**: always call `shutdown()` before process exit to avoid message loss.

## Extension Integration

With preprocessors, logt integrates cleanly with auxiliary modules.
For example, using [`logc`](logc.md) can apply colorized terminal output while keeping file output plain.
Preprocessors can also be used for custom formatting, sensitive data masking, and audit pipelines.