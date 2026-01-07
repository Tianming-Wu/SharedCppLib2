# logc - Log Coloring Extension

+ Name: logc  
+ Namespace: `logc`  
+ Document Version: `1.0.0`  

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `logc` |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::logc)
```

## Description

logc is a coloring extension for the logt library that provides ANSI color support for console log output. It works by installing a preprocessor function that adds color codes to log messages before they are written to the output.

> [!NOTE]
> This module is designed for console output and should be disabled when logging to files, as color codes will appear as raw text in file logs.

## Integration with logt

logc integrates seamlessly with logt through its preprocessor system:

```cpp
#include <SharedCppLib2/logt.hpp>
#include <SharedCppLib2/logc.hpp>

// Install the color preprocessor
logt::install_preprocessor(logc::logPreprocessor);
```

## Core Components

### Color Standards

logc provides two built-in color schemes:

#### Built-in Scheme
- **DEBUG**: Light Green (144, 238, 144)
- **INFO**: White (255, 255, 255)  
- **WARN**: Orange (255, 165, 0)
- **ERROR**: Tomato Red (255, 99, 71)
- **FATAL**: Firebrick Red (178, 34, 34)

#### VS Code Scheme
- **DEBUG**: Soft Green (106, 185, 112)
- **INFO**: Pure White (255, 255, 255)
- **WARN**: Soft Orange (255, 203, 107)
- **ERROR**: Bright Red (255, 107, 107)
- **FATAL**: Dark Red (204, 62, 68)

## Functions

### logPreprocessor
```cpp
bool logPreprocessor(logt_message& message);
```
The main preprocessor function that adds color codes to log messages based on their level and the current color scheme.

**Returns**: `true` (return value is currently ignored but reserved for future use)

### setColorStandard
```cpp
void setColorStandard(const color_standard& scheme);
```
Changes the active color scheme. Use either `logc::clrstd_buildin` or `logc::clrstd_vscode`.

## Usage Examples

### Basic Coloring Setup

```cpp
#include <SharedCppLib2/logt.hpp>
#include <SharedCppLib2/logc.hpp>

LOGT_MODULE("Main");

int main() {
    // Essential logt configuration
    logt::stdcout();
    logt::claim("Main");
    
    // Install color preprocessor
    logt::install_preprocessor(logc::logPreprocessor);
    
    // Optional: Change color scheme
    logc::setColorStandard(logc::clrstd_vscode);
    
    // Colored log output
    logt.debug() << "Debug information in green";
    logt.info() << "Info message in white";
    logt.warn() << "Warning in orange";
    logt.error() << "Error in red";
    logt.fatal() << "Fatal error in dark red";
    
    logt::shutdown();
    return 0;
}
```

### Conditional Coloring for File/Console Output

```cpp
void setup_logging(bool use_colors) {
    if (use_colors) {
        // Console output with colors
        logt::stdcout();
        logt::install_preprocessor(logc::logPreprocessor);
    } else {
        // File output without colors
        logt::file("application.log");
        // No preprocessor installed for clean file output
    }
}
```

### Custom Color Scheme

```cpp
// Create custom color scheme
logc::color_standard my_scheme {
    .debug = colorctl(100, 200, 100),    // Custom green
    .info = colorctl(200, 200, 255),     // Light blue
    .warn = colorctl(255, 200, 100),     // Gold
    .error = colorctl(255, 100, 100),    // Bright red
    .fatal = colorctl(150, 50, 50)       // Dark red
};

// Apply custom scheme
logc::setColorStandard(my_scheme);
```

## Complete Working Example

```cpp
#include <SharedCppLib2/logt.hpp>
#include <SharedCppLib2/logc.hpp>
#include <thread>

LOGT_MODULE("MainApp");

class NetworkService {
    LOGT_DECLARE
public:
    void start() {
        logt.info() << "Network service starting";
        logt.debug() << "Port: 8080, Protocol: HTTP";
    }
    
    void handle_error() {
        logt.error() << "Connection timeout occurred";
    }
};

LOGT_DEFINE(NetworkService, "Network");

int main() {
    // Configure logging
    logt::stdcout();
    logt::claim("MainThread");
    logt::setFilterLevel(LogLevel::Debug);
    
    // Enable colored output
    logt::install_preprocessor(logc::logPreprocessor);
    
    logt.info() << "Application initialized with colored logging";
    
    // Demonstrate different log levels with colors
    NetworkService service;
    service.start();
    
    std::thread worker([](){
        logt::claim("WorkerThread");
        LOGT_LOCAL("Worker");
        
        logt.debug() << "Worker thread detailed debug info";
        logt.info() << "Worker processing data";
        logt.warn() << "High memory usage detected";
    });
    
    service.handle_error();
    worker.join();
    
    logt.info() << "Application shutting down";
    logt::shutdown();
    return 0;
}
```

## Output Example

With colors enabled, the console output will display:
- **DEBUG** messages in green
- **INFO** messages in white  
- **WARN** messages in orange/yellow
- **ERROR** messages in bright red
- **FATAL** messages in dark red

## Important Notes

1. **File Logging**: Disable the color preprocessor when logging to files to avoid ANSI codes in log files
2. **Terminal Support**: Colors only work in terminals that support ANSI escape codes
3. **Performance**: Color processing happens in the log worker thread, not the calling thread
4. **Customization**: You can create completely custom color schemes using RGB values

## Dependencies

- Requires `logt` library
- Uses `ansiio` for ANSI color code generation
- Built on C++23 standard