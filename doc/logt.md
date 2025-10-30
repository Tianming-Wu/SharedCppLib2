# logt - Asynchronous Logging Library

+ Name: logt  
+ Namespace: none  
+ Document Version: `1.0.0`  

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

Logt stands for "log threaded" - a high-performance asynchronous logging library for C++23. It performs all log writing operations in a separate thread to prevent blocking the main application thread, making it ideal for performance-critical applications.

> [!WARNING]
> This library uses heavy wrapping and may be difficult to understand from source code alone. Please refer to this documentation for proper usage.

## Key Features

- **Zero-blocking design**: Log operations never block the calling thread
- **Automatic thread identification**: Each log message includes thread context
- **Flexible output**: Support for files, std::cout, and custom streams
- **Signature system**: Class-level and module-level log identification
- **High-precision timestamps**: Optional microsecond-level timing
- **Extensible**: Custom preprocessors for formatting and coloring

## Quick Start

### Basic Usage in Main Function

```cpp
#include <SharedCppLib2/logt.hpp>

LOGT_MODULE("Main");

int main() {
    logt::stdcout();  // Output to console
    logt::claim("MainThread");  // Set thread name
    
    logt.info() << "Application started";
    logt.warn() << "This is a warning";
    logt.error() << "Error occurred: " << error_code;
    
    logt::shutdown();  // Always call before exit
    return 0;
}
```

### Class-Level Logging

```cpp
class DatabaseManager {
    LOGT_DECLARE
    
public:
    void connect() {
        logt.info() << "Connecting to database...";
        logt.debug() << "Connection parameters: " << params;
    }
};

// In implementation file:
LOGT_DEFINE(DatabaseManager, "Database");
```

### Global Function Logging

```cpp
// network.cpp
LOGT_MODULE("Network");

void send_data() {
    logt.info() << "Sending data packet";
    logt.debug() << "Packet size: " << size << " bytes";
}
```

## Core Functions

### Configuration Methods

#### file
```cpp
static void file(const std::filesystem::path& filename);
```
Sets log output to specified file. The file will be opened in append mode.

#### setostream
```cpp
static void setostream(std::ostream& os);
```
Redirects log output to a custom output stream.

#### stdcout
```cpp
static void stdcout();
```
Sets log output to standard output (default behavior).

#### claim
```cpp
static void claim(const std::string& name);
```
Sets the name for the current thread. This name will appear in all log messages from this thread.

#### setFilterLevel
```cpp
static void setFilterLevel(LogLevel level);
```
Sets the minimum log level to output. Messages below this level will be silently ignored.

Available levels: `l_DEBUG`, `l_INFO`, `l_WARN`, `l_ERROR`, `l_FATAL`, `l_QUIET`

#### enableSuperTimestamp
```cpp
static void enableSuperTimestamp(bool enabled);
```
Enables high-precision timestamps with microsecond resolution when set to `true`.

#### install_preprocessor
```cpp
static void install_preprocessor(preprocessor_t preprocessor);
```
Installs a custom preprocessor function to modify log messages before output. Commonly used with `logc` for colored output.

#### shutdown
```cpp
static void shutdown();
```
**Must be called** before program exit to ensure all log messages are processed and threads are properly joined.

### Logging Methods (via logt_sig)

#### info
```cpp
logt_sso info() const;
```
Creates an INFO level log message.

#### warn
```cpp
logt_sso warn() const;
```
Creates a WARN level log message.

#### error
```cpp
logt_sso error() const;
```
Creates an ERROR level log message.

#### fatal
```cpp
logt_sso fatal() const;
```
Creates a FATAL level log message.

#### debug
```cpp
logt_sso debug() const;
```
Creates a DEBUG level log message.

## Macros

### LOGT_DECLARE
Declares a static logt signature within a class. Must be placed in the private section of the class declaration.

### LOGT_DEFINE(Class, Name)
Defines the logt signature for a class. Must be placed in the implementation file (.cpp).

### LOGT_MODULE(Name)
Creates a module-level logt signature for global functions or main program.

### LOGT_LOCAL(Name)
Creates a function-local logt signature for temporary use within a function.

### LOGT_TEMP(Name)
Creates a temporary logt signature for one-time use.

## Log Level Constants

- `l_QUIET` = -1 (suppress all logging)
- `l_DEBUG` = 0
- `l_INFO` = 1  
- `l_WARN` = 2
- `l_ERROR` = 3
- `l_FATAL` = 4

## Complete Example

```cpp
#include <SharedCppLib2/logt.hpp>
#include <SharedCppLib2/logc.hpp>
#include <thread>

class DataProcessor {
    LOGT_DECLARE
public:
    void process() {
        logt.info() << "Starting data processing";
        logt.debug() << "Input size: " << data_size;
    }
};

LOGT_DEFINE(DataProcessor, "DataProcessor");
LOGT_MODULE("MainApp");

int main() {
    // Basic configuration
    logt::file("app.log");
    logt::claim("Main");
    logt::setFilterLevel(LogLevel::l_DEBUG);
    logt::enableSuperTimestamp(true);
    
    // Enable colored output (optional)
    logt::install_preprocessor(logc::logPreprocessor);
    
    // Log messages
    logt.info() << "Application starting";
    logt.warn() << "Configuration file not found, using defaults";
    
    // Class usage
    DataProcessor processor;
    processor.process();
    
    // Multi-threaded example
    std::thread worker([](){
        logt::claim("Worker");
        LOGT_LOCAL("Worker");
        for(int i = 0; i < 5; i++) {
            logt.info() << "Working on task " << i;
        }
    });
    
    worker.join();
    logt.info() << "Application completed";
    
    // Critical: shutdown before exit
    logt::shutdown();
    return 0;
}
```

## Output Format

With default settings, log messages follow this format:
```
[YYYY/MM/DD HH:MM:SS] [LEVEL] [THREAD] [SIGNATURE] Message content
```

With super timestamp enabled:
```
[YYYY/MM/DD HH:MM:SS.MMM.UUU] [LEVEL] [THREAD] [SIGNATURE] Message content
```

Where:
- `MMM` = milliseconds (3 digits)
- `UUU` = microseconds (3 digits)

## Performance Notes

- Log operations are completely non-blocking
- Heavy formatting should be done before logging to minimize queue time
- Consider using `setFilterLevel(LogLevel::l_WARN)` in production to reduce log volume
- Always call `shutdown()` to prevent log loss on program exit

## Submodule Integration

See [`logc`](logc.md) for colored output support through preprocessor installation.