# ini - INI configuration parser library

+ Name: ini
+ Namespace: (none)
+ Document Version: `1.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `ini` |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(my_target PRIVATE SharedCppLib2::ini)
```

## Description

`ini` provides a lightweight and easy-to-use INI configuration parser and writer. It supports common types such as strings, integral types, floating-point types, booleans, `std::stringlist`, and `std::bytearray`. To accommodate different usage patterns, `ini` offers three styles of accessors: explicit default value, type-default constructor, and `std::optional<T>`-returning variants.

## Quick Start

### Basic read/write
```cpp
#include <SharedCppLib2/ini.hpp>

int main() {
    ini cfg;
    cfg.loadFromFile("config.ini");

    // read (string)
    std::string name = cfg.getValue("User", "Name", "default_name");

    // modify and save
    cfg.setValue("User", "Name", "alice");
    cfg.saveToFile("config.ini");
}
```

### Integer and floating read
```cpp
int w = cfg.getValueAsInteger<int>("Window", "Width", 800);
double ratio = cfg.getValueAsFloat<double>("Display", "Scale", 1.0);
```

## Three access styles (important)

To suit different coding styles and error-handling needs, `ini` provides three accessor categories:

- Explicit default value (recommended when you want to control fallback behavior)
  - Example: `cfg.getValue("S", "K", "fallback")` or `cfg.getValueAsInteger<int>("S","K", 42)`.
  - Benefit: call site explicitly documents the intended fallback value.

- Type-default constructor (omit default argument)
  - For template functions (integral/float) the default is `T()`; for `getValue(string)` the default is empty string `""`.
  - Example: `int n = cfg.getValueAsInteger<int>("S","K");` // returns `0` when missing or parse fails
  - Benefit: concise call when type default is acceptable.

- `std::optional<T>` variants (when you need to distinguish “not present” from “present but default/parse-failed”)
  - Names typically end with `Optional` or `...Optional`, e.g.:
    - `std::optional<std::string> getValueOptional(const std::string& section, const std::string& key) const;`
    - `std::optional<int> getValueAsIntegerOptional<int>(...);`
  - Example:
    ```cpp
    auto maybeV = cfg.getValueOptional("S", "K");
    if (maybeV.has_value()) { // Note, while writing if (maybeV) is possible, it may cause ambiguity when the value is bool
        // present -> use *maybeV
    } else {
        // not set
    }
    ```
  - Benefit: clearly distinguishes missing keys from keys that exist but fail parsing or are empty.

## Boolean conventions

Booleans accept the following textual representations (case-insensitive):

- True: `1`, `true`, `yes`, `on`
- False: `0`, `false`, `no`, `off`

Example:
```cpp
bool enabled = cfg.getValueAsBool("Feature", "Enable", false);
auto maybeEnabled = cfg.getValueAsBoolOptional("Feature", "Enable");
```

## `stringlist` and `bytearray` support

- `getValueAsStringList(...)` / `getValueAsStringListOptional(...)`: parse stringlist formats produced by `stringlist::pack()`.
- `getValueAsByteArray(...)` / `getValueAsByteArrayOptional(...)`: parse hex-encoded byte arrays produced by `std::bytearray::tohex()`.

Example:
```cpp
std::stringlist items = cfg.getValueAsStringList("Section", "Items");
auto maybeBytes = cfg.getValueAsByteArrayOptional("Bin", "Data");
```

## API Reference (common)

- `bool loadFromFile(const std::filesystem::path& filename);`
- `bool saveToFile(const std::filesystem::path& filename) const;`
- `std::string getValue(const std::string& section, const std::string& key, const std::string& default_value = "") const;`
- `template<typename T> T getValueAsInteger(const std::string& section, const std::string& key, const T& default_value = T()) const;`
- `template<typename T> T getValueAsFloat(const std::string& section, const std::string& key, const T& default_value = T()) const;`
- `bool getValueAsBool(const std::string& section, const std::string& key, bool default_value = false) const;`
- `std::optional<std::string> getValueOptional(const std::string& section, const std::string& key) const;`
- `template<typename T> std::optional<T> getValueAsIntegerOptional(const std::string& section, const std::string& key) const;`
- `std::stringlist getValueAsStringList(const std::string& section, const std::string& key) const;`
- `std::bytearray getValueAsByteArray(const std::string& section, const std::string& key) const;`

(See `include/ini.hpp` for full API.)

## Examples

### Using explicit default
```cpp
int threads = cfg.getValueAsInteger<int>("App", "Threads", 4);
```

### Using type default (omit default arg)
```cpp
int timeout = cfg.getValueAsInteger<int>("Network", "Timeout"); // returns 0 if missing
```

### Using `std::optional` to check presence
```cpp
auto maybeUser = cfg.getValueOptional("User", "Name");
if (maybeUser) {
    std::cout << "User: " << *maybeUser << std::endl;
} else {
    std::cout << "User not set\n";
}
```

## Notes

- Section and key names in INI are case-sensitive.
- For possibly-missing configuration entries prefer the `std::optional` variants to clearly distinguish "not present" vs default/parse-failed.
- `getValueAsInteger` / `getValueAsFloat` return the supplied default on parse failure; the optional variants return `std::nullopt` on failure.

---

See tests under `SharedCppLib2Test` for usage examples.
