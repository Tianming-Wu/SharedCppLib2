# stringlist - Enhanced String List Library

+ Name: StringList  
+ Namespace: `std`  
+ Document Version: `3.22.5`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `basic` (contains stringlist) |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::basic)
```

## Description

StringList is a powerful extension of `std::vector<std::string>` that provides Qt-inspired string manipulation capabilities. It inherits all standard vector operations while adding convenient methods for string processing, parsing, and transformation.

> [!TIP]
> If you're familiar with Qt's `QStringList`, you'll find StringList provides similar functionality for standard C++ strings.

## Quick Start

### Basic Usage
```cpp
#include <SharedCppLib2/stringlist.hpp>

// Create from initializer list
std::stringlist names = {"Alice", "Bob", "Charlie"};

// Join with separator
std::cout << names.join(", ") << std::endl;
// Output: Alice, Bob, Charlie

// Split string into list
std::stringlist words = std::stringlist::split("hello world from cpp", " ");
```

### Basic Construction
```cpp
int main(int argc, char** argv) {
    std::stringlist args(argc, argv);
    
    std::cout << "Program: " << args.vat(0) << std::endl;
    std::cout << "Arguments: " << args.subarr(1).join(" ") << std::endl;
    
    return 0;
}
```

> [!TIP]
> For advanced command-line argument parsing with type safety and automatic validation, see [`arguments`](arguments.md).

## Core Features

### 🔗 String Joining & Splitting
Advanced methods for converting between strings and string lists.

### 🔍 Search & Filter
Powerful search capabilities and data cleaning operations.

### 📦 Data Conversion
Multiple constructors for different data sources.

### 🛠️ Utility Operations
Convenience methods for common string list operations.

## Function Reference

### String Conversion

#### join
```cpp
string join(const string &separator = " ") const;
string join(size_t begin, size_t size = -1, const string &separator = " ") const;
```
Joins list elements into a single string.

**Examples:**
```cpp
std::stringlist sl = {"a", "b", "c"};
sl.join();           // "a b c"
sl.join(", ");       // "a, b, c"
sl.join(1, 2, "-");  // "b-c" (from index 1, 2 elements)
```

#### xjoin
```cpp
string xjoin(const string &separator = " ", const char binding = '\"') const;
```
Joins with automatic quoting of elements containing the separator.

**Example:**
```cpp
std::stringlist sl = {"file", "path with spaces"};
sl.xjoin(" ");  // "file \"path with spaces\""
```

#### dbgjoin
```cpp
string dbgjoin(string delimiter = "'") const;
```
Joins with delimiters for debugging visibility.

**Example:**
```cpp
sl.dbgjoin("|");  // "|a|b|c|"
```

#### split
```cpp
static stringlist split(const string &s, const string &delimiter);
static stringlist split(const string &s, const stringlist &delimiters);
```
Splits a string into a list using single or multiple delimiters.

**Examples:**
```cpp
// Single delimiter
std::stringlist::split("a,b,c", ",");  // {"a", "b", "c"}

// Multiple delimiters  
std::stringlist::split("a,b c\td", {",", " ", "\t"});  // {"a", "b", "c", "d"}
```

> [!NOTE]
> Use `remove_empty()` after splitting to clean up empty elements from consecutive delimiters.

#### xsplit & exsplit
```cpp
static stringlist xsplit(const string &s, const string &delim, 
                        const string &begin_bind, string end_bind = "", 
                        bool remove_binding = true);

static stringlist exsplit(const string &s, const string &delim,
                         const string &begin_bind, string end_bind = "",
                         bool remove_binding = false, bool strict = false);
```
Advanced splitting with quote/bracket awareness.

**Example:**
```cpp
// Handle quoted sections
std::stringlist::xsplit("cmd arg1 \"quoted arg\" arg3", " ", "\"");
// {"cmd", "arg1", "quoted arg", "arg3"}
```

### Search Operations

#### find & find_last
```cpp
size_t find(const std::string &value, size_t start = 0) const;
size_t find_last(const std::string &value) const;
```
Finds the first/last occurrence of a string.

**Returns:** Index or `stringlist::npos` if not found.

#### find_inside
```cpp
point find_inside(const std::string &substring, size_t start = 0, 
                 size_t start_inside = 0) const;
```
Finds substring within any list element.

**Returns:** `pair<element_index, position_in_element>` or `npoint`.

#### contains
```cpp
bool contains(const std::string &value) const;
```
Checks if any element contains the value.

### Data Management

#### vat
```cpp
string vat(size_t index, const string &default_value = "") const;
```
Safe element access with default value.

#### subarr
```cpp
std::stringlist subarr(size_t start, size_t length = 0) const;
```
Extracts a subrange from the list.

#### remove_empty
```cpp
void remove_empty();
```
Removes all empty strings from the list.

#### unique
```cpp
stringlist unique();
```
Removes duplicate strings (preserves order).

### Functional Programming

#### exec_foreach
```cpp
void exec_foreach(function<void(size_t, string&)> callback);
```
Applies a function to each element.

**Example:**
```cpp
sl.exec_foreach([](size_t index, std::string& value) {
    value = std::to_string(index) + ":" + value;
});
```

## Constructor Reference

### From C-style Arguments
```cpp
stringlist(int argc, char** argv, int start = 0, int end = -1);
```
Converts C-style arguments to stringlist. For advanced argument parsing, see [`arguments`](arguments.md).

### From Initializer List
```cpp
stringlist(initializer_list<string> elements);
```
```cpp
std::stringlist fruits = {"apple", "banana", "orange"};
```

### From String with Splitting
```cpp
stringlist(const string &text, const string &delimiter);
stringlist(const string &text, const stringlist &delimiters);
```
```cpp
std::stringlist words("hello world from cpp", " ");
```

### From Single String
```cpp
explicit stringlist(const string &single_element);
```
Creates a list with one element.

## Advanced Usage

### Pack/Unpack for Serialization
```cpp
std::stringlist data = {"normal", "text with spaces"};
std::string packed = data.pack();  // Auto-quotes spaces
std::stringlist restored = std::stringlist::unpack(packed);
```

### Stream Integration
```cpp
std::stringlist items;
std::cin >> stringist_split(",", items);  // Parse CSV input
```

### Performance Tips

1. **Use `vat()`** for safe element access instead of bounds checking
2. **Pre-allocate** when possible for large lists
3. **Chain operations** efficiently:
   ```cpp
   auto result = std::stringlist::split(input, " ")
                 .remove_empty()
                 .unique();
   ```

## Real-World Examples

### Configuration Parsing
```cpp
std::stringlist config_lines = std::stringlist::split(config_text, "\n")
                               .remove_empty();

for (const auto& line : config_lines) {
    if (line.starts_with("#")) continue;  // Skip comments
    auto parts = std::stringlist::split(line, "=");
    if (parts.size() == 2) {
        config[parts[0]] = parts[1];
    }
}
```

### Command Builder
```cpp
std::string build_command(const std::string& program, 
                         const std::vector<std::string>& args) {
    std::stringlist cmd = {program};
    cmd.append(args);  // add the entire vector
    return cmd.xjoin(" ");  // automatically manage args with space(s).
}

// or single line version：
std::string build_command(const std::string& program, 
                         const std::vector<std::string>& args) {
    return std::stringlist{program}.append(args).xjoin(" ");
}

```

## Common Patterns

## Search & Filter
Powerful search capabilities and data cleaning operations. For advanced regular expression-based filtering, refer to the [regexfilter library](../regexfilter.md#core-classes).

## Common Patterns

### Filtering
```cpp
std::stringlist files = /* ... */;

// Method 1: Using exec_foreach and remove_empty
files.exec_foreach([](size_t i, std::string& file) {
    if (!file.ends_with(".cpp")) {
        file.clear();  // Mark for removal
    }
});
files.remove_empty();

// Method 2: Using regexfilter for advanced pattern matching
#include <SharedCppLib2/regexfilter.hpp>
rf::whitelist cpp_files({".*\\.cpp", ".*\\.hpp"});
cpp_files.apply(files);

// Method 3: Combining stringlist and regexfilter
auto result = std::stringlist::split(input, "\n")
              .remove_empty()
              .apply_filter([](const std::string& s) {
                  return s.contains("important");
              });
```

## Advanced Filtering

While stringlist provides basic filtering capabilities, for complex pattern matching requirements, it's recommended to use the dedicated [regexfilter library](../regexfilter.md#core-classes).

### Simple Filtering (Built-in)
```cpp
// Remove empty strings
list.remove_empty();

// Filter using lambda
list.exec_foreach([](size_t i, std::string& item) {
    if (item.length() < 3) item.clear();
});
list.remove_empty();
```

### Advanced Filtering (Using regexfilter)
```cpp
#include <SharedCppLib2/regexfilter.hpp>

// Create whitelist filter
rf::whitelist valid_extensions({".*\\.txt", ".*\\.md", ".*\\.cpp"});
valid_extensions.apply(files);

// Create blacklist filter  
rf::blacklist exclude_patterns({"temp.*", ".*\\.tmp", "backup.*"});
exclude_patterns.apply(files);
```

For more filtering options, refer to the [regexfilter complete documentation](../regexfilter.md#advanced-usage).

### Transformation
```cpp
std::stringlist paths = {"dir1/file1", "dir2/file2"};
paths.exec_foreach([](size_t i, std::string& path) {
    path = "/usr/local/" + path;
});
```

This revised documentation provides better organization, practical examples, and addresses the actual functionality found in your source code.