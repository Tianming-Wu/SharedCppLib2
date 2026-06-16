# json - JSON Library

+ Name: json
+ Namespace: `scl2`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `json` |

Include usage:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::json)
```

```cpp
#include <SharedCppLib2/json.hpp>
```

## Description

`json` provides JSON parsing, manipulation, and serialization for SharedCppLib2. It supports standard JSON types (null, boolean, integer, floating, string, array, object) and provides a familiar map/array access pattern.

With `SCL2_JSON_ENABLE_EXTENSIONS` defined, it additionally supports `bytearray` and `inline_data_uri` types, with automatic base64 and data URI string decoding during parsing. This extension is **enabled by default** when building with SharedCppLib2 (the CMake option `SCL2_JSON_ENABLE_EXTENSIONS` defaults to `ON` and propagates to consumers via `PUBLIC` compile definitions). When using the module standalone, you need to define it manually before including the header.

> [!WARNING]
> This module is in early development. While functional, it may lack some advanced features found in mature libraries like jsoncpp. It is, however, lightweight and can be used independently of SharedCppLib2 — just copy `json.hpp` and `json.cpp`.

## Quick Start

### Parse and access

```cpp
#include <SharedCppLib2/json.hpp>

scl2::json j = scl2::json::fromString(R"({
    "name": "test",
    "count": 42,
    "ratio": 3.14,
    "active": true,
    "tags": ["dev", "test"],
    "meta": {
        "author": "me"
    }
})");

std::string name  = j["name"].as_string();       // "test"
int64_t    count  = j["count"].as_int();           // 42
double     ratio  = j["ratio"].as_double();        // 3.14
bool       active = j["active"].as_bool();         // true

// Array
for (const auto& tag : j["tags"].as_array()) {
    std::cout << tag.as_string() << std::endl;
}

// Nested object
std::string author = j["meta"]["author"].as_string(); // "me"
```

### UTF-8 and Chinese characters

JSON is a UTF-8 format (RFC 8259). The library stores strings as raw bytes — what you put in is what you get back. For Chinese to work correctly across platforms, ensure inputs are UTF-8:

- **JSON files**: Save them as UTF-8 (recommended for all JSON files).
- **MSVC source code**: Add `/utf-8` to compiler flags so `std::string("你好")` produces UTF-8 bytes.
- **Fallback**: Use the `json_u8()` helper with `u8` literals:

```cpp
// MSVC without /utf-8: "你好" is GBK bytes → wrong
// With /utf-8 flag:    "你好" is UTF-8 bytes → correct
// Without /utf-8 flag: use this helper:
j["name"] = scl2::json_value(scl2::json_u8(u8"你好"));
```

When reading a UTF-8 JSON file, Chinese is stored as UTF-8 and exported back as UTF-8 — no extra work needed.

### Build and export

The `json` class provides convenient built-in export methods:

```cpp
scl2::json j;
j["title"]   = scl2::json_value(std::string("Config"));
j["version"] = scl2::json_value(static_cast<int64_t>(2));
j["debug"]   = scl2::json_value(true);

std::string formatted = j.toString();        // Pretty-printed with indentation
std::string compact   = j.toCompatString();  // Compact, no extra whitespace

// File I/O
scl2::json cfg = scl2::json::fromFile("config.json");
cfg.toFile("config_updated.json");
```

If you need custom formatting (e.g. tab indentation, compact export, inline mode), use [`json_exporter`](#json_exporter) directly:

```cpp
scl2::json_exporter exporter;
// ... configure exporter ...
std::string output = exporter.exportToString(j);
```

### Export format examples

The same JSON structure exported with different exporter settings:

**Input structure:**
```json
{"debug":false,"person":{"name":"Bob","scores":[95,87]}}
```

**Default (`space4`):**
```json
{
    "debug": false,
    "person": {
        "name": "Bob",
        "scores": [
            95,
            87
        ]
    }
}
```

**Compact (`isCompat = true`):**
```json
{"debug":false,"person":{"name":"Bob","scores":[95,87]}}
```

**Tab indentation (`indent_style::tab`):**
```json
{
	"debug": false,
	"person": {
		"name": "Bob",
		"scores": [
			95,
			87
		]
	}
}
```

**Inline mode (`isInline = true`):**
```json
{ "debug": false, "person": { "name": "Bob", "scores": [ 95, 87 ] } }
```

**Space2 (`indent_style::space2`):**
```json
{
  "debug": false,
  "person": {
    "name": "Bob",
    "scores": [
      95,
      87
    ]
  }
}
```

> [!TIP]
> Use `json::toCompatString()` for the compact format. For other styles, configure a `json_exporter` instance and call `exportToString()`.

## Core API

### json_value

The underlying value type. Stores one of: `nullptr`, `bool`, `int64_t`, `double`, `std::string`, `std::vector<json_value>`, `std::map<std::string, json_value>`.

#### Type checks

```cpp
bool is_null() const;
bool is_bool() const;
bool is_int() const;
bool is_double() const;
bool is_string() const;
bool is_array() const;
bool is_object() const;
```

#### Type access (const & mutable)

```cpp
nullptr_t as_null() const;
bool as_bool() const;
int64_t as_int() const;
double as_double() const;
const std::string& as_string() const;
const std::vector<json_value>& as_array() const;
const std::map<std::string, json_value>& as_object() const;
```

Each `as_*` method has a mutable overload returning a reference.

#### Array operations

```cpp
json_value& operator[](size_t index);
const json_value& operator[](size_t index) const;
const json_value& at(size_t index) const;
size_t array_size() const;
std::generator<const json_value&> array_elements() const;  // range-for friendly
```

#### Object operations

```cpp
json_value& operator[](const std::string& key);
const json_value& operator[](const std::string& key) const;
const json_value& at(const std::string& key) const;
bool has_key(const std::string& key) const;
size_t object_size() const;
std::generator<std::pair<const std::string&, const json_value&>> object_members() const;
```

#### Universal

```cpp
size_t size() const;       // array/object size, string length, or 0
void clear();              // reset to null
json_value_type type() const;
```

### json (extends json_value)

Inherits all constructors and methods from `json_value`. Adds:

```cpp
// Factory methods
static json fromString(const std::string& str);
static json fromFile(const std::string& filename);

// Export
std::string toString() const;          // Pretty-printed
std::string toCompatString() const;    // Compact (no extra whitespace)
std::string toFile(const std::string& filename) const;
```

### json_exporter

Controls the output format of `json::toString()`. All public members can be set directly by the user.

```cpp
enum class indent_style { none, space2, space4, tab };

// Factory helpers
static json_exporter compact_exporter();   // isCompat + escapeNonAscii + none indent
static json_exporter inline_exporter();    // isInline + none indent

// Public fields — set these directly before calling exportToString()
bool isCompat;             // compact output (no extra whitespace)
bool isInline;             // inline format (newlines replaced with spaces)
bool escapeNonAscii;       // escape non-ASCII UTF-8 as \uXXXX for max portability
indent_style indentStyle;  // indent style (default: space4)

std::string exportToString(const json& j);
std::string exportToCompatString(const json& j);
```

> [!NOTE]
> `escapeNonAscii` decodes UTF-8 codepoints and emits them as `\uXXXX` escape sequences, making the output safe for environments that cannot handle raw UTF-8. The `compact_exporter()` preset enables this by default. For local-only use, leave it off to keep Chinese and other non-ASCII characters human-readable.

### json_parser

Internal parser class. Users should use `json::fromString()` / `json::fromFile()` instead.

## Extensions (`SCL2_JSON_ENABLE_EXTENSIONS`)

This extension is **enabled by default** when building with SharedCppLib2 — the CMake option `SCL2_JSON_ENABLE_EXTENSIONS` defaults to `ON`, and is set as a `PUBLIC` compile definition on the `json` target, so any target linking to `SharedCppLib2::json` automatically gets it.

If you are using the module standalone (just copying `json.hpp` and `json.cpp`), define it manually before including the header:

```cpp
#define SCL2_JSON_ENABLE_EXTENSIONS
#include <SharedCppLib2/json.hpp>
```

When enabled, the following additional features are available:

- **`bytearray` type** — stores binary data directly; serialized as `"base64:..."` strings; strings starting with `base64:` are auto-decoded on parse.
- **`inline_data_uri` type** — stores data URI values; serialized as the URI string; strings starting with `data:` are auto-parsed on parse.
- **`\xNN` escape sequences** — hex escape recognition in JSON strings.

> [!NOTE]
> These extensions produce standard JSON output (base64 strings and data URI strings), so they remain interoperable with any standard JSON parser.

## JSON Value Types

| Type | Enum Value | C++ Storage |
|------|-----------|-------------|
| Null | `json_value_type::null` | `nullptr_t` |
| Boolean | `json_value_type::boolean` | `bool` |
| Integer | `json_value_type::integer` | `int64_t` |
| Floating | `json_value_type::floating` | `double` |
| String | `json_value_type::string` | `std::string` |
| Array | `json_value_type::array` | `std::vector<json_value>` |
| Object | `json_value_type::object` | `std::map<std::string, json_value>` |
| Bytearray* | `json_value_type::bytearray` | `std::bytearray` |
| Data URI* | `json_value_type::data_uri` | `inline_data_uri` |

\* Available only with `SCL2_JSON_ENABLE_EXTENSIONS`.
