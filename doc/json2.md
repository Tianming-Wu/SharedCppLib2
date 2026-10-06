# json2 - lossless JSON

+ Name: json2
+ Namespace: `scl2::json2`
+ Header: `<SharedCppLib2/json2.hpp>`
+ Document Version: `0.1.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `json2` |

Include usage:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::json2)
```

```cpp
#include <SharedCppLib2/json2.hpp>
```

`json2` depends on nothing but the standard library, so it is a [standalone module](standalone_module.md):
copy `json2.hpp` and `json2.cpp` into a project and it builds on its own.

## Description

`json` throws the source text away: it parses into a value tree and writes a new document out of
it, so indentation, key order and the way a number was written are decided by the writer, not by
whoever wrote the file. `json2` is the module for the other job — **read a document, change a
value, write it back, and have everything you did not touch come out byte for byte the way it
was.**

```cpp
#include <SharedCppLib2/json2.hpp>
using namespace scl2::json2;

document doc = document::parse(R"({
  "name": "demo",      // the layout survives
  "count": 1.0,        // and so does the spelling: 1.0, not 1
  "tags": [ "a", "b" ]
})");

doc.root()["count"] = value(2);
std::cout << doc.serialize();
```

```json
{
  "name": "demo",      // the layout survives
  "count": 2,          // the spelling of the values that were not touched survives too
  "tags": [ "a", "b" ]
}
```

Note that `//` comments above are only there to show the point: JSON has no comments, and `json2`
does not add any.

What survives a parse → edit → serialize cycle:

| Kept as written | Regenerated |
| --- | --- |
+| indentation, line breaks and spaces between tokens | a value that was assigned to through the API |
+| the order of object members | a member that was added |
+| how a number was spelled: `1.0`, `1e5`, `-0`, `0.5000` | a key that was renamed |
+| how a string was escaped: `\u00e9` stays `\u00e9`, `\/` stays `\/` | the separators of a document built from scratch |
+| duplicate keys (both survive, in order) | everything, after a `fidelity::semantic` parse |
+| a byte order mark, and the text around the root value | |

## Fidelity

`document::parse(text, fidelity)` takes either of two levels:

| | `fidelity::raw` (default) | `fidelity::semantic` |
| --- | --- | --- |
+| source text | kept | dropped |
+| member order | document order | document order |
+| duplicate keys | kept as they are | the first one wins, the rest are dropped (what `json` does) |
+| number spelling | kept | regenerated from the value |
+| `serialize()` | source byte for byte, except what was edited | regenerated compact: `{"a":[1,2]}` |
+| memory | one extra string per node | values only |

`fidelity::semantic` is the mode to use when the document is a working value rather than a file:
it is smaller, and `serialize()` gives a normalized form.

## How the source text is kept

Every array element and object member carries the text in front of it (`lead`: the comma and the
whitespace), and a container carries only `tail`, what sits between its last element (or its
opening bracket) and the closing bracket. Serialization is then

```
"[" + lead[0] + element[0] + lead[1] + element[1] + ... + tail + "]"
```

which is why nothing has to be reformatted: the layout is data, not a rendering choice. A scalar
keeps its source token in `raw()`; an **empty `raw()` means "there is no source text"** and the
value is written from its decoded form. That happens when the document was parsed in
`fidelity::semantic`, when a value was assigned to, or when it was built by hand.

A comma belongs to the element that *follows* it, which is what makes editing predictable: remove
an element and its separator leaves with it, except for the first element, where the separator
sits in the second one's `lead` and is stripped instead. Adding an element or a member copies the
separator style of its neighbour, so a formatted document keeps its formatting.

## Objects and lookup

Members are stored in a vector, in document order, so walking an object is a linear pass over
contiguous memory and parsing is an append. Lookup by key uses an index sorted by key that is
built on demand — on the first lookup of an object that is big enough to warrant one — so it is
O(log n) at scale, and **a lookup never reorders the members**.

Above how many members the index is built is a compile-time knob:

```cpp
#define SCL2_JSON2_INDEX_THRESHOLD 16    // the default
#define SCL2_JSON2_INDEX_THRESHOLD 0     // index every object from its first member
#define SCL2_JSON2_INDEX_THRESHOLD SIZE_MAX  // no index at all, always a linear scan
```

| Call | Meaning |
|---------|---------|
+| `root()["key"]` | the value, appended as `null` when the key is not there |
+| `root().find("key")` | `value*`, `nullptr` when there is none |
+| `root().has_key("key")` | whether it is there |
+| `root().erase_key("key")` | remove the first member with that key |
+| `root().as_object_body().members()` | the members, in document order, for walking |
+| `root()[0]`, `root().size()` | arrays, by index |
+| `root().push_back(value("x"))` | append to an array |
+| `root().erase(0)` | remove an array element |
| `root().at(0)` / `root().at("key")` | the element / the member, bounds checked (throws `std::out_of_range`) |
| `root().front()` / `root().back()` / `root().pop_back()` | the ends of an array |
| `root().length()` | the length of a string |
| `root().clear_as_array()` / `root().clear_as_object()` | make it an empty array / object |
| `root().empty_as_array()` / `root().empty_as_object()` | whether it is one, and empty |
| `root().items()` / `root().members()` | the elements / members, with the source text in front of each |

## Path queries (JSON Pointer)

`at_path` / `find_path` / `contains_path` take a [JSON Pointer](https://datatracker.ietf.org/doc/html/rfc6901),
the syntax `json` also uses: `/a/0/b`, with `~1` standing for a `/` inside a key and `~0` for a `~`.
The empty pointer is the whole document.

```cpp
document doc = document::parse(text);

if (doc.contains_path("/servers/0/port"))
    std::cout << doc.at_path("/servers/0/port").as_int();

value* port = doc.find_path("/servers/0/port");   // nullptr when it names nothing
```

| Call | Meaning |
| --- | --- |
| `at_path(pointer)` | the value; throws `std::out_of_range` when the path names nothing |
| `find_path(pointer)` | `value*`, `nullptr` when the path names nothing |
| `contains_path(text)` | whether `find_path` would find something |

A pointer can be parsed once and applied many times, which is what makes it worth having as an
object of its own:

```cpp
json2::pointer p("/servers/0/port");   // parsed once
doc.at_path(p).set_int(8080);
```

The cases that name nothing: a path that runs through a scalar (`/a/0/x` where `/a/0` is a
number), an index with a leading zero (`/a/01`), `-` — the token that means "one past the end"
when writing — and an index past the end. None of them throw; only `at_path` does, and only when
nothing was found. A pointer that does not start with `/` is rejected when it is built
(`std::invalid_argument`), not when it is applied.

## Regenerated output, and comparing

`serialize()` is the faithful one. `to_string()` regenerates: it ignores the source text and the
layout, and writes what the values hold, laid out by an `exporter`. The default is **the same
layout `scl2::json` writes by default** — one member or element per line, indented with four
spaces, `": "` after a key, and an empty container on one line. `_apicheck/jsoncrossprobe.cpp`
feeds the same document to both modules and compares the two outputs byte for byte, so they do not
drift apart.

```cpp
document ugly = document::parse("{ \"x\" : 1.50, \"y\":[1e5, 2] }");

ugly.serialize();          // { "x" : 1.50, "y":[1e5, 2] }   — as it was written
ugly.to_string();          // laid out again, four spaces
ugly.to_compact_string();  // {"x":1.5,"y":[1e+05,2]}       — the compact form
```

| `exporter` | |
| --- | --- |
| `indentStyle` | `none` / `space2` / `space4` (the default) / `tab` |
| `isCompact` | no line breaks at all, and no space after the `:` |
| `isInline` | line breaks become single spaces, so everything ends up on one line |
| `escapeNonAscii` | every byte above 0x7F is written as a `\uXXXX` escape, a surrogate pair above U+FFFF; invalid UTF-8 becomes `U+FFFD` |
| `exporter::compact_exporter()` / `inline_exporter()` | the two settings most callers want, ready made |

Doubles are written in the shortest form that reads back as the same number, so `1.50` becomes
`1.5` and `1e5` becomes `1e+05`: both are valid JSON, both are the same value. An integral double
keeps a fraction (`1.0`), so it also reads back as a double rather than turning into an integer.
`json` writes doubles the same way since its 1.12.1 (`_apicheck/jsoncrossprobe.cpp` compares the
two modules' float output as well, byte for byte).

Values compare with `==`. Arrays are compared element by element; objects **by key**, so member
order does not matter — and neither does spelling, because only what a value holds is compared.
`1` and `1.0` are different types, so they are not equal. `assign_to(T&)` writes a scalar into a
`bool`, an integral, a floating point or a `std::string`, without parsing anything, so a string
value does not quietly become a number.

## Errors

`parse` throws `json2::parsing_error`, whose text carries the offset:

```
json2 parsing error: expected ',' or '}' (at offset 42)
```

A byte that cannot be printed is shown in hex (`unexpected byte '\xEF'`), because that is almost
always an encoding problem — a byte order mark in the middle of a document, or UTF-16 text.

Nesting is limited to 256 levels, since the parser is recursive. Writing a number that is not
finite throws: JSON has no infinity and no NaN.

## Not implemented yet

- creating a path that does not exist while walking it: `at_path` reads, and a path that names
  nothing throws; `operator[]` is the way to build one down (`doc.root()["a"]["b"] = value(1)`)
- sorting, merging and the other algorithms a document library tends to grow later

## Deliberately not part of this module

- **the SharedCppLib2 integration** (`gdump` / `gload`, `bytearray`, the `datauri` extensions).
  `json2` is a standalone module and stays one: a middle layer is where the library's own
  integration goes, so the dependency runs from that layer to `json2` and never the other way.
- **`dump()` / `load()`** for the document. For what a document needs internally,
  `to_compact_string()` already gives the text and `document::parse` takes it back, so a binary
  form would add a second encoder without adding a capability. For a compact binary tree there is
  `jbt`, which exists for exactly that.

## Deviations from `json`

- `json` keeps objects in a `std::map`, so its output is in key order; `json2` keeps document
  order. Neither normalizes the other's.
- `json` has the `bytearray` / `data_uri` extensions behind `SCL2_JSON_ENABLE_EXTENSIONS`;
  `json2` has no extensions at all so far.
- Arrays and objects are `value::push_back` / `operator[]` instead of `json_value`'s methods, and
  values are accessed as `value` (there is no separate `json` / `json_value` split).

## See Also

- [json](json.md) — the value-tree module, which stays the right one for plain data handling
- [xml2](xml2.md) — the same fidelity idea for XML, where `fidelity::raw` and `fidelity::semantic`
  are also the two levels
- [standalone_module.md](standalone_module.md) — what "standalone module" means here
