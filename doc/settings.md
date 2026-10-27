# settings - one settings object bound to a file

+ Name: settings
+ Namespace: `scl2`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `settings` |
| Dependencies | `json2`, `toml`, `ini`, `xkeydb` |

To include:

```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::settings)
```

```cpp
#include <SharedCppLib2/settings.hpp>
```

## Description

`scl2::settings` holds an application's settings and is bound to one file. A key is the path of
the value inside that file — `window/width` is the member `width` of the object `window` — and the
file is written in one of four formats, chosen from its extension.

| Provider | The file | What a save keeps |
|---------|---------|---------|
| `json` | JSON text | the order of the members and the spelling of every untouched value |
| `toml` | TOML text | the same, plus comments, blank lines and line endings |
| `ini` | INI text | sections and keys; a value is text, so its type is read from how it was written |
| `xkeydb` | binary | typed values, optionally compressed, checked and encrypted |

A value is one of null, bool, integer, floating point, string or bytes. The type it was given is
the type it keeps, except in INI, where everything is text.

## Quick Start

```cpp
#include <SharedCppLib2/settings.hpp>

scl2::settings config("config.toml");

int width = config.get("window/width", 800);     // 800 when the key is missing
config.set("window/width", 1024);
config.set("window/title", "Editor");

config.save();                                    // also happens on destruction
```

The file is written when the object is destroyed unless `set_auto_save(false)` was called, so an
explicit `save()` is only needed when the file has to be there before then. The file then holds:

```toml
[window]
width = 1024
title = "Editor"
```

## Keys

A key is one or more names separated by `/`. A leading or trailing `/` is accepted, and an empty
segment is skipped, so `/window/width` and `window/width` are the same key.

- A **segment of digits** addresses an element of an array: `servers/1/host`.
- A level that does not exist yet becomes an **array** when the segment that names it is all
  digits, and an **object** otherwise. `set("pinned/0", 3)` makes `pinned` an array.
- A key may not contain `/` inside a name, since that is the separator.

> [!NOTE]
> INI is flat, so only the first two segments are used: the first is the section and the rest
> are joined into the key. A key with a single segment goes to the section `General`.

## Values

### Writing

```cpp
config.set("window/width", 1024);                  // integer
config.set("window/ratio", 1.5);                   // floating point
config.set("window/title", "Editor");              // string
config.set("window/maximised", true);              // bool
config.set("window/icon", scl2::bytearray(icon));  // bytes
config.set("window/width", nullptr);               // removes the key
config.set_default("window/height", 600);          // only when the key holds nothing
```

`set()` accepts bool, any integral or floating point type, any enumeration (stored as its
underlying value), `std::string`, `const char*`, `scl2::bytearray`, a class that is plain data
(its bytes are the value), and a class that supports `dump()` / `load()`. A null value removes
the key.

### Reading

```cpp
int width = config.get("window/width", 800);
std::string title = config.get("window/title", std::string());
bool maximised = config.get("window/maximised", false);
scl2::bytearray icon = config.get("window/icon", scl2::bytearray());

scl2::settings_key value = config.get("window/width");   // null when missing
if (value.is_int()) { ... }
if (config.contains("window/width")) { ... }
```

`get<T>()` answers the default value when the key is missing, when it names a container, or when
what is stored cannot be read as `T` — a settings file is edited by hand, so a wrong or missing
value is not an error.

Conversions the accessors make:

| Stored | `get<bool>` | `get<integer>` | `get<double>` | `get<std::string>` |
|---------|---------|---------|---------|---------|
| bool | as is | `0` / `1` | `0.0` / `1.0` | `true` / `false` |
| integer | `!= 0` | as is | converted | written out |
| floating point | `!= 0.0` | truncated | as is | written out |
| string | `true` / `false` / `1` / `0` | when it is a number | when it is a number | as is |
| bytes | — | — | — | `base64:…` |
| null | — | — | — | — |

`get<scl2::bytearray>()` on a string answers the decoded bytes when the text starts with
`base64:`, and the bytes of the text otherwise.

`remove()` answers whether the key was there; `clear()` empties the document.

### Enumerations

An enumeration registered with `scl2_strenum` or `scl2_bitstrenum` keeps its name, so a file
someone may open reads `"Automatic"` rather than `3`:

```cpp
config.set("mode", StartStopMode::Automatic);                       // the file holds "Automatic"
StartStopMode mode = config.get("mode", StartStopMode::Automatic);
StartStopMode same = config.get("mode").as_strenum<StartStopMode>();
```

| Type | Stored as | Read with |
|---------|---------|---------|
| registered with `scl2_strenum` | the name; the number when the table has no name for the value | `as_strenum<E>()`, `as<E>()`, `get<E>()` |
| registered with `scl2_bitstrenum` | `"Read \| Write"`; the number when the table has no name for the value | `as_bitstrenum<E>()`, `as<E>()`, `get<E>()` |
| anything else | the underlying value | `as_enum<E>()`, `as<E>()`, `get<E>()` |

- An `xkeydb` file keeps the number for every enumeration: that file is for the program, not
  for a reader.
- A name is read back by name; a number, or text that is one, is taken as that number, so a
  hand-edited file and one written by an older version both read.
- `as_enum<E>()` accepts only an enumeration that has no table; passing a registered one is a
  compile error, which is what makes `as_strenum()` / `as_bitstrenum()` the way in.

## The file

```cpp
config.path();                       // the file the object is bound to
config.provider();                   // the format that was chosen

config.save();                       // write it out; creates the parent directory
config.reload();                     // drop what is in memory and read the file again

config.set_auto_save(false);         // stop the destructor from writing
config.set_instant_save(true);       // write after every change
```

The format follows the extension: `.toml` is TOML, `.ini` is INI, `.xkeydb` (also `.xkdb` and
`.db`) is an `xkeydb` database, and anything else — including no extension — is JSON. The
constructor that takes `settings_provider` or `settings_options` overrides that.

A file that does not exist, is empty, or holds only whitespace starts as an empty document, so a
settings file created by a package manager or by `touch` works before anything is written. A JSON
file that had no content is written with the standard layout for its first save; from then on the
file's own text is what a save keeps.

### xkeydb

```cpp
scl2::settings_options options;
options.provider = scl2::settings_provider::xkeydb;
options.secret   = scl2::bytearray("pass phrase");
options.integrity = scl2::xkeydb::integrity_algo::crc32;
options.compress  = scl2::compress_algo::zlib;

scl2::settings secure("secure.xkeydb", options);
```

- `secret` unlocks an existing file and encrypts one that is created. A missing or wrong secret
  throws.
- `compress` and `integrity` are chosen when the file is **created**; the ones in an existing file
  are read from its header and are not changed by the options.
- Keys are stored with their path (`window/width`), since the database has no nesting.

## Reference

### `settings_provider`

| Value | Meaning |
|---------|---------|
| `automatic` | decided from the extension; the default |
| `json` | json2 |
| `toml` | TOML |
| `ini` | INI |
| `xkeydb` | an `xkeydb` database |

### `settings_options`

| Field | Description |
|---------|---------|
| `provider` | the format to use; `automatic` by default |
| `secret` | `xkeydb` only: unlocks an existing file and encrypts one that is created |
| `compress` / `integrity` | `xkeydb` only: how a file that has to be created is encoded and checked |

### `settings`

| Member | Description |
|---------|---------|
| `settings(path, provider)` | open or create that file |
| `settings(path, options)` | the same, with the options `settings_options` carries |
| `set(key, value)` | store a value; null removes the key |
| `set(key, enumeration)` / `set_default(key, enumeration)` | an enumeration: by name when its type has a table, the number in `xkeydb` |
| `set_default(key, value)` | store it only when the key holds nothing |
| `get(key)` | the value as `settings_key`; null when there is none |
| `get(key, default)` | the same, with a value for a missing key |
| `get<T>(key, default)` | the value read as `T`; the default when it cannot be |
| `contains(key)` | whether the key names something |
| `remove(key)` | remove the key; answers whether it was there |
| `clear()` | empty the document |
| `setValue()` / `getValue()` | the same as `set()` / `get()` |
| `path()` / `provider()` | what the object is bound to |
| `save()` / `reload()` | write / re-read the file |
| `auto_save()` / `set_auto_save()` | whether the destructor writes (on by default) |
| `instant_save()` / `set_instant_save()` | whether every change writes (off by default) |

A `settings` is move-only: a copy would be a second object bound to the same file.

### `settings_key`

| Member | Description |
|---------|---------|
| `is_null()` / `is_bool()` / `is_int()` / `is_float()` / `is_string()` / `is_bytes()` | what it holds |
| `type_name()` | that, as text |
| `as<T>()` | the value as `T`, with the conversions above |
| `as_enum<E>()` / `as_strenum<E>()` / `as_bitstrenum<E>()` | the value as an enumeration, by number or by name |
| `as_bool()` / `as_int64()` / `as_uint64()` / `as_double()` / `as_string()` / `as_bytearray()` | the same, by type |
| `as_trivial_class<T>()` / `as_nontrivial_class<T>()` | the bytes as a class |

### `globalsettings`

| Member | Description |
|---------|---------|
| `init(path, provider)` / `init(path, options)` | create the one instance; throws when it already exists |
| `reset()` | destroy it, saving it if auto-save is on |
| `initialized()` | whether it exists |
| `instance_ref()` | the instance; throws `std::logic_error` before `init()` |

The instance is destroyed when the program ends as well, so its file is written even when
`reset()` is never called.

## Behaviour

- **`get<T>()` never throws for a bad value.** The default comes back instead, so a settings
  file that was edited into something wrong does not stop the program.
- **A key that names an object, an array or a table is not a value.** `get(key)` answers null for
  it while `contains(key)` still answers true.
- **The destructor writes.** A failed write there is swallowed, since a destructor has nowhere to
  report it; call `save()` by hand when it matters.
- **Bytes are text in the text formats.** JSON, TOML and INI hold a `scl2::bytearray` as
  `base64:…`, which `get<bytearray>()` decodes back.
- **TOML has no null.** A null value removes the key, the same as in the other formats.
- **Nothing is locked.** One object is meant for one thread at a time.

## Not implemented yet

- Listing the keys of a level (a directory-like walk).
- Layered settings: a system file and a user file behind one object.
- A reformat of a file that already has text: a save keeps that text rather than re-indenting it.

## See Also

- [json2](json2.md) — the lossless JSON document behind the `json` provider
- [toml](toml.md) — the TOML document behind the `toml` provider
- [ini](ini.md) — the INI document behind the `ini` provider
- [xkeydb](xkeydb.md) — the key-value database behind the `xkeydb` provider
- [bytearray](bytearray.md) — the value type for bytes
