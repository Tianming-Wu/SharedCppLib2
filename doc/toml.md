# toml - TOML Library

+ Name: toml
+ Namespace: `scl2`
+ Document Version: `0.1.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `toml` |

Include usage:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::toml)
```

```cpp
#include <SharedCppLib2/toml.hpp>
```

## Description

`toml` is a TOML (Tom's Obvious, Minimal Language) parser and serializer for SharedCppLib2. A TOML document is always a **table of key/value pairs**, which maps naturally to a `scl2::toml` object whose values are `scl2::toml_value`.

Supported value types: `string`, `integer`, `floating`, `boolean`, `date-time`, `array`, `table` — TOML has **no `null` type**.

Supported syntax:
- `key = value` pairs and **dotted keys** (`a.b.c = 1`)
- `[table]` headers and **`[[array-of-tables]]`** headers
- arrays `[1, 2, 3]` and **inline tables** `{ a = 1, b = "x" }` (multi-line inline tables are accepted)
- basic (`"..."`, escapes, `"""` multi-line) and literal (`'...'`, `'''` multi-line) strings
- integers (decimal / hex / octal / binary, underscores allowed), floats (incl. `inf` / `nan`), booleans
- comments (`#` to end of line)
- date-times are kept verbatim (e.g. `2024-01-01T12:00:00Z`)

> [!WARNING]
> This module is in early development (v0.1.0). It covers the core TOML 1.0 syntax needed by real-world configuration files (including Minecraft `META-INF/neoforge.mods.toml`). Strict edge cases of the spec are not fully enforced.

## Quick Start

### Parse and access

```cpp
#include <SharedCppLib2/toml.hpp>

auto doc = scl2::toml::fromString(R"(
title = "My App"
version = 1.5
enabled = true
released = 2024-01-01T12:00:00Z

[database]
host = "localhost"
port = 5432

[[servers]]
name = "alpha"
[[servers]]
name = "beta"
)");

std::string title    = doc["title"].as_string();              // "My App"
double      version  = doc["version"].as_double();            // 1.5
bool        enabled  = doc["enabled"].as_bool();              // true
std::string released = doc["released"].as_datetime();         // "2024-01-01T12:00:00Z"
std::string host     = doc["database"]["host"].as_string();   // "localhost"
size_t      n        = doc["servers"].array_size();           // 2
std::string first    = doc["servers"][0]["name"].as_string(); // "alpha"
```

### Parse from file

```cpp
auto doc = scl2::toml::fromFile("config.toml");   // UTF-8
```

### Serialize back

```cpp
std::string text = doc.toString();   // serialize to TOML text
doc.toFile("copy.toml");             // write to a file
```

### Reading Minecraft `META-INF/neoforge.mods.toml`

A typical mod file has top-level keys, a `[[mods]]` array-of-tables, and `[[dependencies.<modid>]]`:

```cpp
auto doc = scl2::toml::fromFile("META-INF/neoforge.mods.toml");

std::string modLoader = doc["modLoader"].as_string();   // "javafml"

// [[mods]] is always an array, even with a single element:
for (const auto& m : doc["mods"].as_array()) {
    std::string id   = m["modId"].as_string();          // "hostilenetworks"
    std::string ver  = m["version"].as_string();        // "6.5.1"  <- a string, not a number
    std::string desc = m["description"].as_string();    // '''...''' multi-line string
}

// multiple [[dependencies.<modid>]] blocks become array elements:
for (const auto& dep : doc["dependencies"]["hostilenetworks"].as_array()) {
    std::string modId = dep["modId"].as_string();       // "minecraft" / "neoforge" / ...
    std::string type  = dep["type"].as_string();        // "required"
    std::string range = dep["versionRange"].as_string(); // "[1.21.1,)"
}
```

## Core API

### `toml_value` — a value inside a table or array

| Category | Members |
|---|---|
| Type checks | `is_bool()`, `is_int()`, `is_double()`, `is_string()`, `is_datetime()`, `is_array()`, `is_table()` |
| Read access | `as_bool()`, `as_int()`, `as_double()`, `as_string()`, `as_datetime()`, `as_array()`, `as_table()` |
| Write access | mutable `as_xxx()` overloads |
| Array | `operator[](size_t)`, `array_size()`, `push_back()`, `empty_as_array()` |
| Table | `operator[](const std::string&)`, `at(key)`, `has_key()`, `contains()`, `table_size()` |
| Universal | `type()` → `toml_value_type`, `size()`, `empty()`, `operator==` |

### `toml` — the document (always a table)

```cpp
static toml fromString(const std::string& str);                      // parse
static toml fromFile(const std::filesystem::path& path);             // parse file

std::string toString() const;                                        // serialize
std::string toFile(const std::filesystem::path& path) const;         // serialize to file

bool has_key(const std::string& key) const;                          // member exists?
bool contains(const std::string& key) const;
toml_value&       operator[](const std::string& key);                // create-or-get
const toml_value& operator[](const std::string& key) const;          // throws if missing
size_t size() const;                                                 // number of members
bool empty() const;

toml_table&       table();                                           // direct map access (iterate)
const toml_table& table() const;
```

### Value types

| `toml_value_type` | TOML example | C++ access |
|---|---|---|
| `string` | `"hello"`, `'x'`, `"""multi-line"""` | `as_string()` |
| `integer` | `42`, `0x2A`, `0o52`, `0b101010` | `as_int()` |
| `floating` | `3.14`, `1e10`, `inf`, `nan` | `as_double()` |
| `boolean` | `true` / `false` | `as_bool()` |
| `datetime` | `2024-01-01T12:00:00Z` | `as_datetime()` (raw text) |
| `array` | `[1, 2, 3]` | `as_array()` |
| `table` | `[a]` header, `{ a = 1 }` inline | `as_table()` |

## Notes

- **No `null` type**: TOML has no null; a default-constructed `toml_value` is an empty string.
- **`[[...]]` is always an array**: even when it appears once, access elements with `[0]`.
- **Multi-line inline tables are accepted** (e.g. `mods = [ { ... }, { ... } ]` spanning lines) — the TOML spec forbids this, but real-world files (like `neoforge.mods.toml`) use it.
- **Dotted access** via `operator[]` on nested tables/arrays handles arrays-of-tables automatically.
- **Quoted numbers are strings**: in mod files `version = "6.5.1"` is a *string* — use `as_string()`, not `as_int()`.
