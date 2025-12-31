# arguments - Type-Safe Command-Line Argument Parser

+ Name: arguments  
+ Namespace: `std`  
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `arguments` (independent library) |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::arguments)
```

> [!NOTE]
> The `arguments` library has public linkage to `basic`, so you don't need to link `basic` separately.

## Description

**arguments** is a modern C++ command-line argument parser inspired by Python's `argparse`. It provides type-safe, template-based parameter parsing with automatic type conversion and validation.

Built on top of [`stringlist`](stringlist.md), it supports multiple parsing styles (GNU, POSIX, Windows) and offers flexible policy controls for robust argument handling.

## Quick Start

### Basic Example
```cpp
#include <SharedCppLib2/arguments.hpp>

int main(int argc, char** argv) {
    std::arguments args(argc, argv);
    
    // Parse string parameter
    std::string name;
    args.addParameter("--name", name, "default");
    
    // Parse integer with base support
    int count;
    args.addParameter("--count", count, 10);
    
    // Parse boolean flag
    bool verbose;
    args.addFlag("--verbose", verbose);
    
    std::cout << "Name: " << name << ", Count: " << count << std::endl;
    return 0;
}
```

**Usage:**
```bash
./program --name Alice --count 42 --verbose
# Name: Alice, Count: 42
```

## Core Features

### 🎯 Type Safety
Template-based design with automatic type deduction and conversion.

### 🔧 Multiple Argument Styles
- **GNU Style**: `--option value` (default)
- **POSIX Style**: `-o value`
- **Windows Style**: `/option:value`

### 📦 Rich Type Support
Supports strings, integers, floats, booleans, enums, and custom types.

### 🛡️ Policy Controls
Configurable validation and error handling strategies.

### ℹ️ Program Name Handling
Automatically extracts and skips `argv[0]` (program name) during parsing.

## Supported Types

### String Parameters
```cpp
std::string output;
args.addParameter("--output", output, "default.txt");
```

### Integer Parameters
```cpp
int port;
args.addParameter("--port", port, 8080);

// Support different bases (binary, octal, hex, etc.)
int flags;
args.addParameter("--flags", flags, 0, 16);  // Parse as hexadecimal
```

### Floating-Point Parameters
```cpp
double threshold;
args.addParameter("--threshold", threshold, 0.5);

float ratio;
args.addParameter("--ratio", ratio, 1.0f);
```

### Boolean Parameters
```cpp
// Boolean with value
bool debug;
args.addParameter("--debug", debug, false);

// Boolean flag (presence = true)
bool quiet;
args.addFlag("--quiet", quiet);
```

### Enum Parameters
```cpp
enum class LogLevel { Debug = 0, Info = 1, Warning = 2, Error = 3 };

int level;
std::map<std::string, int> log_levels = {
    {"debug", 0}, {"info", 1}, {"warning", 2}, {"error", 3}
};
args.addEnum("--log-level", level, log_levels, 1);
```

### Custom Deserializable Types
Any class with a `deserialize(string)` or `deserialise(string)` method:

```cpp
class Config {
public:
    void deserialize(const std::string& s) {
        // Parse configuration from string
        // e.g., "key1=value1;key2=value2"
    }
};

Config config;
args.addParameter("--config", config);
```

## Argument Styles

### GNU Style (Default)
```bash
# Space-separated (recommended)
--option value
--flag

# Equals syntax (optional with AllowEqualSign policy)
--option=value
```

### POSIX Style
```bash
# Short options
-o value
-f

# Combined flags (not yet implemented)
-abc  # equivalent to -a -b -c
```

### Windows Style
```bash
/option:value
/flag
```

## Parse Policies

Control argument validation and behavior:

```cpp
enum parse_policy {
    Null                = 0,       // No special handling
    FailIfEmptyValue    = 1 << 0,  // Error on empty values
    FailIfUnknown       = 1 << 1,  // Error on unknown options
    AllowEqualSign      = 1 << 2   // Allow --option=value syntax
};
```

### Example with Policies
```cpp
// Strict mode: fail on unknown options and empty values
std::arguments args(argc, argv, 
    FailIfEmptyValue | FailIfUnknown);

// Allow equals syntax
std::arguments args(argc, argv, 
    AllowEqualSign);
```

## Function Reference

### Constructor
```cpp
basic_arguments(int argc, CharT** argv);
basic_arguments(int argc, CharT** argv, parse_policy policy);
basic_arguments(int argc, CharT** argv, parse_policy policy, argument_style style);
```

### name
```cpp
string_type name() const;
```
Returns the program name (`argv[0]`). This is automatically extracted and not treated as an option.

### empty
```cpp
bool empty() const;
```
Returns `true` if there are no arguments (only `argv[0]` or less).

### addParameter (String)
```cpp
void addParameter(const string_type& name, 
                  string_type& value, 
                  const string_type& default_value = string_type());
```

### addParameter (Integral)
```cpp
template<typename T>
requires(std::is_integral_v<T> && !std::is_same_v<T, bool>)
void addParameter(const string_type& name, 
                  T& value, 
                  T default_value = 0, 
                  int base = 10);
```
Supports any base from 2 to 36.

### addParameter (Floating-Point)
```cpp
template<typename T>
requires(std::is_floating_point_v<T>)
void addParameter(const string_type& name, 
                  T& value, 
                  std::optional<T> default_value = std::nullopt);
```

### addParameter (Boolean)
```cpp
void addParameter(const string_type& name, 
                  bool& value, 
                  bool default_value = false);
```
Accepts: `true/false`, `yes/no`, `on/off`, `1/0`

### addFlag
```cpp
void addFlag(const string_type& name, 
             bool& value, 
             bool default_value = false);
```
Sets to `true` if present, ignores value.

### addEnum
```cpp
void addEnum(const string_type& name, 
             int& value, 
             const std::map<string_type, int>& options, 
             int default_value = 0);
```

### addParameter (Custom Types)
```cpp
template<typename T>
requires requires(T& t, const string_type& s) {
    requires std::is_class_v<T>;
    requires requires { t.deserialize(s); } || requires { t.deserialise(s); };
}
void addParameter(const string_type& name, 
                  T& value, 
                  std::optional<T> default_value = std::nullopt);
```

## Advanced Usage

### Hexadecimal Numbers
```cpp
int flags;
args.addParameter("--flags", flags, 0, 16);

// Usage: --flags 0xFF or --flags FF
```

### Binary Numbers
```cpp
int mask;
args.addParameter("--mask", mask, 0, 2);

// Usage: --mask 10110101
```

### Custom Deserializers
```cpp
class Point {
    int x, y;
public:
    void deserialize(const std::string& s) {
        auto pos = s.find(',');
        if (pos == std::string::npos)
            throw std::invalid_argument("Expected format: x,y");
        x = std::stoi(s.substr(0, pos));
        y = std::stoi(s.substr(pos + 1));
    }
};

Point position;
args.addParameter("--pos", position);
// Usage: --pos 100,200
```

### Wide Character Support
```cpp
std::warguments args(argc, argv);

std::wstring name;
args.addParameter(L"--name", name, L"default");
```

## Error Handling

All parameter parsing can throw `parameter_error`:

```cpp
try {
    std::arguments args(argc, argv, FailIfUnknown);
    
    int port;
    args.addParameter("--port", port, 8080);
    
} catch (const parameter_error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
}
```

Common errors:
- Invalid numeric format
- Unknown option (with `FailIfUnknown` policy)
- Empty value (with `FailIfEmptyValue` policy)
- Invalid enum value
- Custom deserializer exceptions

## Best Practices

### 1. Use Descriptive Names
```cpp
// Good
args.addParameter("--output-file", output);

// Less clear
args.addParameter("-o", output);
```

### 2. Provide Sensible Defaults
```cpp
int timeout;
args.addParameter("--timeout", timeout, 30);  // 30 seconds default
```

### 3. Use Flags for Booleans
```cpp
// Prefer addFlag for simple presence checks
bool verbose;
args.addFlag("--verbose", verbose);

// Use addParameter when explicit true/false needed
bool enable;
args.addParameter("--enable-feature", enable, false);
```

### 4. Group Related Policies
```cpp
const auto strict_policy = FailIfEmptyValue | FailIfUnknown;
std::arguments args(argc, argv, strict_policy);
```

## Implementation Notes

- Uses `std::from_chars` for high-performance number parsing
- Zero runtime overhead for type checking (concepts + `if constexpr`)
- Template functions are header-only
- Supports both US (`deserialize`) and UK (`deserialise`) spelling
- Built on `std::basic_stringlist<CharT>` for efficient string handling

## See Also

- [`stringlist`](stringlist.md) - Underlying string list implementation
- [`bytearray`](bytearray.md) - For binary configuration formats

## Version History

- **v1.0.0** - Initial release with full template support
  - Integral types with arbitrary base
  - Floating-point types via `std::from_chars`
  - Custom deserializable types
  - Multiple parsing styles and policies
