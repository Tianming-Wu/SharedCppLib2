# yaml - YAML Library

+ Name: yaml
+ Namespace: `scl2::yaml`
+ Document Version: `0.1.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `yaml` |

Include usage:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::yaml)
```

```cpp
#include <SharedCppLib2/yaml.hpp>
```

## Description

`yaml` provides YAML 1.2 parsing for SharedCppLib2. It supports a streaming, line-oriented parser with feature flags for incremental adoption of the full YAML specification.

> [!WARNING]
> This module is in early development (v0.1.0). Currently implements block-style mappings and sequences, scalars, comments, and double-quoted escapes. Flow style (`{}`, `[]`), anchors (`&`, `*`), tags (`!!`), and multi-document streams are not yet supported but are designed into the feature flag system.
>
> **This module is read-only.** There is no exporter or serialization support yet. It can parse YAML into a `document` tree for reading, but cannot write YAML output.

## Quick Start

### Parse and access

```cpp
#include <SharedCppLib2/yaml.hpp>

auto doc = scl2::yaml::document::fromString(
    "name: Bob\n"
    "scores:\n"
    "  - 95\n"
    "  - 87\n"
);

std::string name   = doc["name"].as_string();     // "Bob"
int64_t    first   = doc["scores"][0].as_int();   // 95
bool       is_obj  = doc["scores"].is_array();    // true
```

### Parse from file

```cpp
#include <fstream>

std::ifstream file("config.yaml");
auto doc = scl2::yaml::document::fromStream(file);
```

### Feature flags

Control which YAML features the parser accepts:

```cpp
scl2::yaml::features feat;
feat.comments   = false;  // disable # comments
feat.anchors    = true;   // enable &/* (when implemented)

auto doc = scl2::yaml::document::fromString(input, feat);
```

## Core API

### value

The underlying data type. Stores one of: `nullptr`, `bool`, `int64_t`, `double`, `std::string`, `std::vector<value>`, `std::map<std::string, value>`, `alias_ref`.

#### Type checks

```cpp
bool is_null() const;
bool is_bool() const;
bool is_int() const;
bool is_double() const;
bool is_string() const;
bool is_array() const;
bool is_object() const;
bool is_alias() const;
```

#### Type access (const & mutable)

```cpp
nullptr_t as_null() const;
bool as_bool() const;
int64_t as_int() const;
double as_double() const;
const std::string& as_string() const;
const std::vector<value>& as_array() const;
const std::map<std::string, value>& as_object() const;
const alias_ref& as_alias() const;
```

Each `as_*` method has a mutable overload returning a reference.

#### Array operations

```cpp
value& operator[](size_t index);
const value& operator[](size_t index) const;
const value& at(size_t index) const;
size_t array_size() const;
std::generator<const value&> array_elements() const;
```

#### Object operations

```cpp
value& operator[](const std::string& key);
const value& operator[](const std::string& key) const;
const value& at(const std::string& key) const;
bool has_key(const std::string& key) const;
size_t object_size() const;
bool remove_member(const std::string& key);
std::generator<std::pair<const std::string&, const value&>> object_members() const;
```

#### Universal

```cpp
size_t size() const;       // array/object size, string length, or 0
void clear();              // reset to null
type type() const;         // index-based type query
```

### document (extends value)

Inherits all constructors and methods from `value`. Adds:

```cpp
// Factory methods
static document fromStream(std::istream& input, features feat = {});
static document fromString(const std::string& input, features feat = {});
```

### features

Controls which YAML features the parser accepts. Set directly on a `parser` instance or pass to `fromStream`/`fromString`.

```cpp
struct features {
    bool comments   = true;   // # line comments
    bool multi_line = true;   // | and > scalar blocks (not yet implemented)
    bool anchors    = false;  // &anchor and *alias (not yet implemented)
    bool tags       = false;  // !!type tags (not yet implemented)
    bool multi_doc  = false;  // --- / ... document separators (not yet implemented)
};
```

### parser

Internal streaming parser. Users should prefer `document::fromString()` / `document::fromStream()`. For streaming multi-document input, use `parseNext()`.

```cpp
class parser {
public:
    static document fromStream(std::istream& input, features feat = {});
    static document fromString(const std::string& input, features feat = {});
    bool parseNext(std::istream& input, document& out);

    features feat;
};
```

## Supported YAML Syntax

### Scalars

| Input | Type |
|-------|------|
| `null`, `~` | null |
| `true`, `True`, `TRUE`, `yes`, `Yes`, `YES`, `on`, `On`, `ON` | bool (true) |
| `false`, `False`, `FALSE`, `no`, `No`, `NO`, `off`, `Off`, `OFF` | bool (false) |
| `42`, `-17`, `+3` | int |
| `3.14`, `-2.5`, `1e10` | double |
| `hello`, `"hello"`, `'hello'` | string |

### Block mappings

```yaml
name: Alice
age: 30
scores:
  - 95
  - 87
```

### Block sequences

```yaml
- Alice
- Bob
- Carol
```

### Compact syntax

```yaml
people:
  - name: Alice
    age: 25
  - name: Bob
    age: 30
```

### Comments

```yaml
# comment line
key: value  # inline comment
```

### Double-quoted escapes

| Escape | Result |
|--------|--------|
| `\n` | newline |
| `\t` | tab |
| `\\` | backslash |
| `\"` | double quote |
| `\a` `\b` `\f` `\r` `\v` `\e` | control chars |
| `\xNN` | hex byte |
| `\uXXXX` | Unicode BMP |
| `\UXXXXXXXX` | Unicode full range |

## YAML Value Types

| Type | Enum Value | C++ Storage |
|------|-----------|-------------|
| Null | `type::null` | `nullptr_t` |
| Boolean | `type::boolean` | `bool` |
| Integer | `type::integer` | `int64_t` |
| Floating | `type::floating` | `double` |
| String | `type::string` | `std::string` |
| Array | `type::array` | `std::vector<value>` |
| Object | `type::object` | `std::map<std::string, value>` |
| Alias | `type::alias` | `alias_ref` |
