# toml - TOML Library

+ Name: toml
+ Namespace: `scl2`
+ Document Version: `0.3.0`

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

`toml` is a [standalone module](standalone_module.md) but for
[`orderedmap`](orderedmap.md) — a table is a `scl2::ordered_map` — so copying the module into a
project means copying `orderedmap.hpp` next to `toml.hpp` and `toml.cpp`.

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
> This module is in early development (v0.3.0). It covers the core TOML 1.0 syntax needed by real-world configuration files (including Minecraft `META-INF/neoforge.mods.toml`). Strict edge cases of the spec are not fully enforced.

## Fidelity

A TOML file is written by a person, and the file a program writes back should still be that
person's file. `fromString()` and `fromFile()` therefore keep the source text by default:

```cpp
scl2::toml doc = scl2::toml::fromString(text);   // fidelity::raw, the default
doc["database"]["port"] = 6432;                  // one value changed
doc.toString();                                  // one line changed — comments and all
```

| Kept as written | Regenerated |
| --- | --- |
+| every comment, at the end of a line and on lines of its own | a value that was assigned to |
+| the order of the members | a member that was added |
+| blank lines and indentation | a value that was built by hand |
+| the line endings (CRLF stays CRLF), a byte order mark, a last line without one | everything, after a `fidelity::semantic` parse |
+| how a value is spelled: `0x1F`, `1_000_000`, `'literal'`, `1.0`, `1979-05-27T07:32:00Z` | |
+| `[a.b]` / `[[a.b]]` headers, inline tables, dotted keys, arrays across lines | |

What follows from it while editing:

- **A changed value keeps everything around it** — the key, the spacing before the value and the
  comment at the end of its line.
- **A new member lands at the end of the values of its own section**, before the next `[header]`,
  which is where TOML wants it. A new table becomes a `[header]` block of its own.
- **A pushed array element takes the separator style of the array** — `"b",` on its own line stays
  on its own line — and erasing the first element takes its comma with it.

`fidelity::semantic` is the other level: only the values are kept, the members still follow the
order of the file, and `toString()` regenerates a canonical document with no comments.

```cpp
scl2::toml doc = scl2::toml::fromString(text, scl2::fidelity::semantic);
```

### What the source text is kept in

Every member carries the slices of the file around it: the blank lines and comments in front of it
(`lead()`), the key as it stands (`key_text()`), the text between the key and the value
(`separator()`), and the rest of its line, line ending included (`trail()`). A scalar carries its
token (`raw()`); a container carries nothing of its own, because it is written out of the members
inside it — which is why editing one element cannot leave a stale copy of the whole array behind.

`has_source()` says whether a value came from a file, and `dirty()` whether its value was replaced
afterwards: a dirty value is written again, its comments staying. `set_raw("0x1F")`,
`set_comment("why")` and `set_lead("# section\n")` state the text for a value that was built by
hand.

There is no `dirty()` on the document: keep the text you read and compare it with `toString()` to
know whether anything changed.

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
| Erase | `erase(key)` for a member, `erase(index)` for an array element |
| Source text | `has_source()`, `dirty()`, `raw()`, `lead()`, `trail()`, `key_text()`, `separator()`, `tail()`, `header_text()`, `table_style()`, `is_inline_table()` |
| Source text (write) | `set_raw(text)`, `set_lead(text)`, `set_trail(text)`, `set_comment(text)`, `set_inline_table()` |
| Universal | `type()` → `toml_value_type`, `size()`, `empty()`, `operator==` |
| Assign | `assign_to(T& dest)` — write this value into a user variable (concrete type or `std::variant`) |

### `toml` — the document (always a table)

```cpp
static toml fromString(const std::string& str, fidelity f = fidelity::raw);   // parse
static toml fromFile(const std::filesystem::path& path, fidelity f = fidelity::raw);

std::string toString() const;                                        // serialize
std::string toFile(const std::filesystem::path& path) const;         // serialize to file

fidelity mode() const;                                               // which level it was parsed with
const std::string& epilog() const;                                   // the text after the last member
void set_epilog(std::string text);

bool has_key(const std::string& key) const;                          // member exists?
bool contains(const std::string& key) const;
toml_value&       operator[](const std::string& key);                // create-or-get
const toml_value& operator[](const std::string& key) const;          // throws if missing
size_t size() const;                                                 // number of members
bool empty() const;
bool erase(const std::string& key);                                  // remove a member, comment included

toml_table&       table();                                           // direct map access (iterate)
const toml_table& table() const;                                     // scl2::ordered_map: document order
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
- **A table is `scl2::ordered_map`**: `toml_table` iterates in the order of the file, and `find` / `at` / `count` are there as well; `operator[]` appends when the key is new. A `toml_value` can still be built from a `std::map`, and `assign_to()` still fills one.
- **`toString()` writes the members in the order of the file** (of insertion, for a document built by hand), not in key order — that is what a round trip needs. `table().sorted_begin()` walks them in key order instead.
- **A value after a `[header]` belongs to that table.** That is TOML, not the parser: a top-level key has to be written before the first header. Adding a member through the API puts it where it belongs.
- **An array of inline tables is a value**: `points = [{ x = 1 }]` stays on its line and is no longer rewritten as `[[points]]` blocks. An array of tables built by hand is still written as blocks.
- **Mutating through `as_xxx()`** — the mutable overloads, as in `doc["a"].as_int() = 5` — changes the value but keeps the spelling it was read with. Assign to the value instead (`doc["a"] = 5`) to have the token written again.
- **A key is quoted when it has to be** (`"a.b"`, `"a b"`), so a key that looks like a dotted path does not become one on the way out.

## Not implemented yet

- Reordering members: `table()` is an `ordered_map`, so `move_before()`, `insert_at()` and
  `sort_by()` are there, but `toml` has no shorthand of its own for them.
- A reformatter: the writer keeps the layout of the file rather than re-indenting or aligning it.
- Strictness: a readable but unusual file is accepted (see the note above), so not every document
  that TOML 1.0 rejects is rejected here.
