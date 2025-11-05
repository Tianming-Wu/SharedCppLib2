# regexfilter - Regular Expression Filtering Library

+ Name: regexfilter  
+ Namespace: `rf`  
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `regexfilter` |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::regexfilter)
```

## Description

regexfilter is a powerful regular expression-based string filtering library that provides blacklist and whitelist functionality for stringlist operations. It enables advanced pattern matching and filtering capabilities using C++ standard regex.

## Quick Start

### Basic Usage
```cpp
#include <SharedCppLib2/regexfilter.hpp>
#include <SharedCppLib2/stringlist.hpp>

int main() {
    std::stringlist files = {"main.cpp", "temp.txt", "backup.tmp", "header.h"};
    
    // Create blacklist to exclude temporary files
    rf::blacklist exclude_temp({".*\\.tmp", "temp.*"});
    exclude_temp.apply(files);
    
    // files now contains: {"main.cpp", "header.h"}
    
    return 0;
}
```

### Whitelist Example
```cpp
std::stringlist sources = {"main.cpp", "util.cpp", "data.txt", "config.ini"};

// Create whitelist to keep only C++ source files
rf::whitelist include_cpp({".*\\.cpp", ".*\\.hpp"});
include_cpp.apply(sources);

// sources now contains: {"main.cpp", "util.cpp"}
```

## Core Classes

### base_list
The foundation class that implements core regex matching and filtering logic.

#### Constructor
```cpp
base_list(const std::stringlist& patterns = std::stringlist());
```
Creates a filter with the specified regex patterns. Invalid regex patterns are silently ignored.

#### filtered
```cpp
bool filtered(const std::string& s) const;
```
Checks if a string matches any of the regex patterns.

**Returns:** `true` if the string matches any pattern, `false` otherwise.

#### apply
```cpp
int apply(std::stringlist& list, bool reverse) const;
```
Applies filtering to a stringlist.

**Parameters:**
- `list`: The stringlist to filter (modified in-place)
- `reverse`: If `true`, keeps matching items; if `false`, removes matching items

**Returns:** Number of items removed from the list

### blacklist
Excludes strings that match the specified patterns.

#### Constructor
```cpp
blacklist(const std::stringlist& patterns = std::stringlist());
```
Creates a blacklist with the specified exclusion patterns.

#### filtered
```cpp
bool filtered(const std::string& s) const;
```
**Returns:** `true` if the string matches any blacklist pattern (should be excluded)

#### apply
```cpp
bool apply(std::stringlist& list) const;
```
Removes all strings that match the blacklist patterns from the list.

**Returns:** `true` if any items were removed

### whitelist
Includes only strings that match the specified patterns.

#### Constructor
```cpp
whitelist(const std::stringlist& patterns = std::stringlist());
```
Creates a whitelist with the specified inclusion patterns.

#### filtered
```cpp
bool filtered(const std::string& s) const;
```
**Returns:** `true` if the string does NOT match any whitelist pattern (should be excluded)

#### apply
```cpp
bool apply(std::stringlist& list) const;
```
Removes all strings that do NOT match the whitelist patterns from the list.

**Returns:** `true` if any items were removed

## Advanced Usage

### Complex Pattern Matching
```cpp
// Filter emails with specific domains
rf::blacklist email_filter({".*@spam\\.com", ".*@temp\\.org"});

// Filter files with complex patterns
rf::whitelist source_files({
    ".*\\.(cpp|cxx|cc)$", 
    ".*\\.(hpp|hxx|hh)$",
    "CMakeLists\\.txt",
    ".*\\.cmake$"
});
```

### Combining Multiple Filters
```cpp
std::stringlist items = {"file1.cpp", "file2.txt", "temp.cpp", "backup.h"};

// First, exclude temporary files
rf::blacklist temp_filter({"temp.*", "backup.*"});
temp_filter.apply(items);

// Then, keep only source files
rf::whitelist source_filter({".*\\.cpp", ".*\\.h"});
source_filter.apply(items);

// items now contains: {"file1.cpp"}
```

### Error Handling
```cpp
// Invalid regex patterns are automatically ignored
rf::blacklist safe_filter({"valid.*", "[invalid-regex", "another.*"});
// Only "valid.*" and "another.*" will be used
```

## Real-World Examples

### Log File Filtering
```cpp
std::stringlist log_entries = {
    "2024-01-15 INFO: Application started",
    "2024-01-15 DEBUG: Loading configuration",
    "2024-01-15 ERROR: Database connection failed",
    "2024-01-15 WARN: High memory usage"
};

// Filter out DEBUG messages
rf::blacklist debug_filter({".*DEBUG:.*"});
debug_filter.apply(log_entries);

// Keep only ERROR and WARN messages
rf::whitelist error_filter({".*ERROR:.*", ".*WARN:.*"});
error_filter.apply(log_entries);
```

### File System Operations
```cpp
#include <filesystem>
namespace fs = std::filesystem;

std::stringlist get_filtered_files(const fs::path& directory) {
    std::stringlist all_files;
    
    for (const auto& entry : fs::directory_iterator(directory)) {
        all_files.push_back(entry.path().filename().string());
    }
    
    // Exclude system and temporary files
    rf::blacklist system_files({
        "\\..*",           // Hidden files (Unix)
        ".*\\.tmp",        // Temporary files
        "Thumbs\\.db",     // Windows thumbnail cache
        "~\\.*"            // Backup files
    });
    
    system_files.apply(all_files);
    return all_files;
}
```

### Configuration Filtering
```cpp
std::stringlist load_config_with_filter(const std::string& filename) {
    std::ifstream file(filename);
    std::stringlist lines;
    
    // Read all lines
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    
    // Remove comments and empty lines
    rf::blacklist comments({
        "^\\s*#.*",    // Shell/Python style comments
        "^\\s*//.*",   // C++ style comments
        "^\\s*;.*",    // INI style comments
        "^\\s*$"       // Empty lines
    });
    
    comments.apply(lines);
    return lines;
}
```

## Performance Tips

1. **Pre-compile Patterns**: Construct filter objects once and reuse them
2. **Use Specific Patterns**: More specific regex patterns are faster to match
3. **Combine Operations**: Apply multiple filters in sequence rather than creating complex single patterns
4. **Batch Processing**: Filter large lists in batches if memory is a concern

## Pattern Examples

### Common Regex Patterns
```cpp
// File extensions
".*\\.(jpg|jpeg|png|gif)$"           // Image files
".*\\.(cpp|cxx|cc|hpp|hxx|hh)$"      // C++ source files
".*\\.(txt|md|rst)$"                 // Text files

// Log levels
".*\\b(ERROR|FATAL)\\b:.*"           // Error messages
".*\\b(DEBUG|TRACE)\\b:.*"           // Debug messages

// Data formats
"^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"  // Email addresses
"^\\d{4}-\\d{2}-\\d{2}$"                            // Date format
```

## Integration with StringList

regexfilter works seamlessly with stringlist:

```cpp
std::stringlist data = std::stringlist::split(input_text, "\n")
                      .remove_empty();

rf::blacklist unwanted_patterns({"spam", "advertisement"});
unwanted_patterns.apply(data);

// Continue processing cleaned data
```