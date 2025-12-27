# logt - Asynchronous Logging Library

+ Name: logt  
+ Namespace: none  
+ Document Version: `1.2.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `logt` |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::logt)
```

## Description

Logt stands for "log threaded" - a high-performance asynchronous logging library designed for C++23. It employs a dedicated logging thread to handle all write operations, ensuring that the main application thread is never blocked by logging activities, making it ideal for performance-critical applications.

> [!WARNING]
> This library uses complex wrapping mechanisms that may be difficult to understand from source code alone. Please refer to this documentation for proper usage.

## Key Features

- **Fully Asynchronous Design**: All logging operations are processed in a background thread with zero blocking on calling threads
- **Intelligent Thread Identification**: Automatically captures and displays thread context information in log messages
- **Multiple Output Support**: Flexible support for file output, standard output, and custom streams
- **Modular Signature System**: Provides class-level, module-level, and function-level log identification
- **High-Precision Timing**: Optional millisecond/microsecond timestamp accuracy
- **Powerful Extensibility**: Supports custom preprocessors for formatting and colored output

## Quick Start

### Basic Usage

```cpp
#include <SharedCppLib2/logt.hpp>

LOGT_MODULE("Main");

int main() {
    logt::stdcout();  // Output to console
    logt::claim("MainThread");  // Name current thread
    
    logt.info() << "Application initialization completed";
    logt.warn() << "Non-critical issue detected";
    logt.error() << "Operation failed with error code: " << error_code;
    
    logt::shutdown();  // Must be called before program exit
    return 0;
}
```

### Class-Level Logging

```cpp
class DatabaseManager {
    LOGT_DECLARE
    
public:
    void connect() {
        logt.info() << "Establishing database connection...";
        logt.debug() << "Connection parameters: " << connection_params;
    }
};

// In corresponding implementation file:
LOGT_DEFINE(DatabaseManager, "Database");
```

### Function-Level Logging

```cpp
// network.cpp file
LOGT_MODULE("Network");

void send_data() {
    logt.info() << "Starting data packet transmission";
    logt.debug() << "Packet details - size: " << packet_size << " bytes";
}
```

## Core API Reference

### System Configuration Methods

#### addfile - Add File Channel
```cpp
static int addfile(const std::filesystem::path& filename, bool default_enable = true);
```
Opens (append mode) a file output channel and returns its channel id. `default_enable` controls whether new signatures enable this channel by default. Returns `-1` if channel slots are exhausted.

#### addostream - Add Custom Stream
```cpp
static int addostream(std::ostream& os, bool default_enable = true);
```
Registers a custom stream channel and returns its channel id. The provided stream must outlive logging.

#### stdcout - Console Channel
```cpp
static void stdcout(bool enable = true, bool default_enable = true);
```
Controls channel 0 (stdout). It is always registered; you can disable it or exclude it from defaults.

#### claim - Thread Naming
```cpp
static void claim(const std::string& name);
```
Sets a readable name for the current thread; the name appears in subsequent log lines from this thread.

#### setFilterLevel - Log Filtering
```cpp
static void setFilterLevel(LogLevel level);
```
Sets the minimum level to queue. Levels: `l_DEBUG`, `l_INFO`, `l_WARN`, `l_ERROR`, `l_FATAL`, special `l_QUIET`.

#### enableSuperTimestamp - High-Precision Timestamps
```cpp
static void enableSuperTimestamp(bool enabled);
```
Adds milliseconds/microseconds to timestamps when enabled.

#### install_preprocessor - Preprocessor
```cpp
static void install_preprocessor(preprocessor_t preprocessor);
```
Installs a preprocessing hook. It now runs inside `write_message()`, so you can let it color stdout/custom streams while file channels still see the original unprocessed content (e.g., no ANSI codes in files).

#### shutdown - System Shutdown
```cpp
static void shutdown();
```
Gracefully stops the worker thread and flushes all pending messages. **Call before program exit.**

### Logging Methods

Log level methods provided through logt_sig objects:

#### info - Information Level
```cpp
logt_sso info() const;
```
Creates an INFO-level log message stream for recording normal application operation information.

#### warn - Warning Level
```cpp
logt_sso warn() const;
```
Creates a WARN-level log message stream for recording potentially concerning abnormal conditions.

#### error - Error Level
```cpp
logt_sso error() const;
```
Creates an ERROR-level log message stream for recording error conditions that don't prevent program continuation.

#### fatal - Fatal Level
```cpp
logt_sso fatal() const;
```
Creates a FATAL-level log message stream for recording critical errors that prevent program continuation.

#### debug - Debug Level
```cpp
logt_sso debug() const;
```
Creates a DEBUG-level log message stream for recording detailed debugging information, typically used during development.

## Macro Reference

### LOGT_DECLARE
Declares a static log signature object inside a class declaration. Must be placed in the private access section of the class.

### LOGT_PUBLIC_DECLARE
Public version of `LOGT_DECLARE`, exposing the static log signature for external configuration (e.g., channel changes).

### LOGT_DEFINE(Class, Name)
Defines the log signature for a specific class in the implementation file. The Class parameter specifies the target class name, and the Name parameter defines the log identifier for that class.

### LOGT_MODULE(Name)
Creates a module-level log signature suitable for global functions, namespace scope, or main program entry points.

### LOGT_LOCAL(Name)
Creates a function-scoped local log signature for temporary use within functions.

### LOGT_TEMP(Name)
Creates a temporary log signature object for one-time use scenarios that don't require persistent state.

## Channel Management

- **Channel 0**: stdout is always registered; `stdcout(enable, default_enable)` controls it.
- **Adding channels**: `addfile()` / `addostream()` return channel ids. They set `default_enable` for new signatures.
- **Per-signature control**: `logt_sig::setChannel(id, enable)` / `setChannels({ids...})` return `bool` and validate id registration.
- **Public access**: use `LOGT_PUBLIC_DECLARE` if a class needs external code to adjust its channels.
- **Preprocessor scope**: runs inside `write_message()`; stdout/custom streams use processed content (e.g., colors), file channels use original content (no ANSI codes).

## Log Level Details

- `l_QUIET` = -1 - Complete silent mode, no logging
- `l_DEBUG` = 0 - Debug level, most detailed operational information
- `l_INFO` = 1 - Information level, normal operational status
- `l_WARN` = 2 - Warning level, potential abnormal conditions
- `l_ERROR` = 3 - Error level, error conditions
- `l_FATAL` = 4 - Fatal level, critical errors

## Complete Application Example

```cpp
#include <SharedCppLib2/logt.hpp>
#include <thread>

class DataProcessor {
    LOGT_DECLARE
public:
    void process() {
        logt.info() << "Starting data processing pipeline";
        logt.debug() << "Input dataset size: " << input_data.size();
        // Processing logic...
    }
};

LOGT_DEFINE(DataProcessor, "DataProcessor");
LOGT_MODULE("MainApplication");

int main() {
    // System initialization configuration
    int file_channel = logt::addfile("application.log");  // Output to file
    logt::stdcout(true);              // stdout channel 0
    logt::claim("MainThread");       // Main thread naming
    logt::setFilterLevel(LogLevel::l_DEBUG);  // Set log level
    logt::enableSuperTimestamp(true); // Enable high-precision timestamps
    // Route module logs to stdout + file
    logt.setChannels({0, file_channel});
    
    // Color preprocessor: colors stay on stdout/custom streams, files get plain text
    // logt::install_preprocessor(logc::logPreprocessor);
    
    // Record startup information
    logt.info() << "Main application initialization completed";
    logt.warn() << "Using default configuration parameters";
    
    // Use class logging functionality
    DataProcessor data_processor;
    data_processor.process();
    
    // Multi-threaded logging example
    std::thread worker_thread([](){
        logt::claim("WorkerThread");  // Worker thread naming
        LOGT_LOCAL("Worker");         // Local log signature
        
        for(int task_id = 0; task_id < 5; task_id++) {
            logt.info() << "Executing work task ID: " << task_id;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    
    worker_thread.join();
    logt.info() << "All processing tasks completed";
    
    // Critical step: shutdown logging system
    logt::shutdown();
    return 0;
}
```

## Output Format Specification

Standard output format:
```
[YYYY/MM/DD HH:MM:SS] [LEVEL] [THREAD] [SIGNATURE] Log message content
```

Format with high-precision timestamps enabled:
```
[YYYY/MM/DD HH:MM:SS.MMM.UUU] [LEVEL] [THREAD] [SIGNATURE] Log message content
```

Format explanation:
- Timestamp: Complete date and time information
- Level: Log severity identifier (DEBUG/INFO/WARN/ERROR/FATAL)
- Thread: Name of the thread that produced the log
- Signature: Source module or class identifier of the log message
- Message content: Specific log information provided by the user

## Performance Optimization Recommendations

- **Zero-Blocking Advantage**: Leverage the asynchronous nature - logging operations won't impact main thread performance
- **Preprocessing Optimization**: Complex string concatenation and formatting should be completed before logging calls to reduce processing time in the queue
- **Production Configuration**: In production environments, consider setting `setFilterLevel(LogLevel::l_WARN)` or higher to reduce unnecessary log output
- **Resource Cleanup**: Always call the `shutdown()` method before program exit to prevent log message loss and resource leaks

## Extension Integration

Through the preprocessor mechanism, logt can seamlessly integrate with other functional modules. Particularly when used with the [`logc`](logc.md) color output library, it can add rich color identifiers to terminal logs, enhancing log readability. Preprocessors also support advanced features like custom format conversion, sensitive information filtering, and log auditing.