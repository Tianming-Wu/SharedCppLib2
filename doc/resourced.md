# resourced - Where a Program's Resources Live

+ Name: resourced
+ Namespace: `scl2`
+ Header: `<SharedCppLib2/resourced.hpp>`
+ Document Version: `1.1.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `resourced` |
| Dependencies | `basic`, `orderedmap` |

Include usage:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::resourced)
```

```cpp
#include <SharedCppLib2/resourced.hpp>
```

`find_package(SharedCppLib2)` also provides `scl2_add_resources()`, which is how a directory of
files becomes a pack inside the binary. See [Building a pack](#building-a-pack).

## Description

A **resource** is a named blob of bytes that a program needs but did not compute: an icon, a
language pack, a shader, a template, a payload for an installer. This module answers one question —
given a name, where do the bytes come from — and leaves what they mean to the caller.

Sources are **layered**. A manager holds a list of mounted sources, each with a name of its own, and
a lookup asks them until one has the resource. Later mounts are searched first, so a pack placed
beside the program overrides what the program shipped with, and a directory mounted last overrides
both. `info()` reports which source answered, so a lookup can be explained rather than guessed at.

A pack is **not compressed and not encrypted**. The Encryption API and the compression providers
take a `bytearray`, and a pack is a `bytearray`, so a caller who wants either can have it without
this module knowing about it.

## Quick Start

```cpp
#include <SharedCppLib2/resourced.hpp>
#include "myapp_resources.hpp"    // written by scl2_add_resources(), see below

scl2::ResourceManager res;

// 1. What is inside the binary, parsed where it lies — nothing is copied out of it.
res.mount("builtin", *scl2::ResourceView::parse(myapp_resources::resources()));

// 2. A pack file beside the program, when there is one. Answers before "builtin".
res.mount_pack_file("sidecar", "app.pack");

// 3. A directory of loose files, for development. Answers before both.
res.mount_directory("dev", "res/");

// Ask by name. get() works for every kind of source.
if (const auto bytes = res.get(myapp_resources::k_i18n_zh_CN_toml))
    load_language(*bytes);

// view() is the same bytes without a copy, when the source holds them in memory.
if (const auto bytes = res.view(myapp_resources::k_app_ico))
    scl2::set_window_icon(hwnd, scl2::to_hicon(*bytes, 32, 32));
```

## Models

### Sources

`ResourceType` says how a source keeps its bytes. It is a property of the **mount**, not of the
pack: the same pack is `Embedded` when its bytes are inside the binary and `ExternalPacked` when
they are in a file.

| `ResourceType` | Mounted with | How the bytes arrive |
|---------|---------|--------------|
| `Embedded` | `mount(name, ResourcePack)` or `mount(name, ResourceView)` | from memory; `view()` hands out a pointer into it |
| `ExternalFile` | `mount_directory(name, dir)` | read from disk on every lookup |
| `ExternalPacked` | `mount_pack_file(name, file)` | the directory is read while mounting, each entry from the file on demand |
| `PlatformEmbedded` | — | reserved; nothing produces it yet |
| `SystemResource` | — | reserved; nothing produces it yet |
| `Null` | — | not known, or not looked up yet |

Mounting takes no preprocessing. A source mounted at run time is indistinguishable from one built
into the program: nothing is registered ahead of time, and a file that appears after the program
was built is found by `mount_directory()` like any other.

A source name can be mounted once. `mount*()` returns `false` when the name is taken, when the
directory does not exist, or when the file cannot be read as a pack.

### The pack format

All integers are little-endian, so a pack written on one machine reads on another.

| Offset | Size | Field |
|---------|---------|-------|
| 0 | 8 | magic: `SCL2RES\0` |
| 8 | 4 | format version: `1` |
| 12 | 4 | resource count |
| 16 | — | directory: `count` entries, each `name length (4)`, `name bytes`, `size (8)` |
| — | — | payload: the data of every entry, in directory order |

The directory **is** the index. Entry offsets are not stored: an entry starts where the previous one
ended, so writing takes one pass and the directory stays small. Everything that needs an offset
computes it from the sizes in front of it, and a lookup jumps straight to the bytes rather than
walking the payload.

A name is the path of the file inside the resource directory, with forward slashes — `i18n/zh_CN.toml`
for a file at `res/i18n/zh_CN.toml`. Nothing translates it.

## Reference

### scl2_add_resources

Provided by `find_package(SharedCppLib2)`. Packs a directory and attaches the result to a target.

```cmake
scl2_add_resources(<target> DIR <resource-dir>
                   [NAMESPACE <ns>] [STEM <name>] [OUT_DIR <dir>] [LINK <lib> ...])

scl2_add_resources(<target> GENERATED <generated-dir> [STEM <name>] [LINK <lib> ...])
```

`DIR` packs the directory and attaches the generated source. `STEM` and `NAMESPACE` both default to
the target name, so `scl2_add_resources(myapp DIR res/)` writes `myapp.hpp` / `myapp.cpp` with the
constants in `namespace myapp`.

The generated header carries one constant per resource plus one accessor:

```cpp
namespace myapp {
inline constexpr std::string_view k_i18n_zh_CN_toml = "i18n/zh_CN.toml";
std::span<const std::byte> resources();
}
```

Getting the names from the header is what turns a mistyped resource name into a compile error. The
generated source depends on nothing but `<cstddef>`, `<span>` and `<string_view>`.

`GENERATED` attaches `<generated-dir>/<stem>.cpp` and `<generated-dir>/<stem>.hpp` without running
anything, for a build that cannot run the tool itself. See [Limits and roadmap](#limits-and-roadmap).

### ResourcePack

An owned container. Build one, write it out, or hold one that was read.

| Member | Description |
|---------|--------------|
| `add(name, data)` | append a resource; `false` when the name is taken, `std::invalid_argument` when it is empty or too long |
| `replace(name, data)` | overwrite, or add when new; an existing entry does not move |
| `erase(name)` | remove one; `false` when it was not there |
| `contains(name)` / `size()` / `empty()` | query |
| `names()` | every name, in pack order |
| `view(name)` / `get(name)` | the bytes as a view into the pack, or as a copy |
| `info(name)` | name, size, and the offset the bytes will have in the pack |
| `dump()` / `dump_to(out)` | serialize to a `bytearray`, or write to any `std::ostream` |
| `load(data)` / `load(stream)` | parse; throws `std::runtime_error` on anything malformed |

`load()` on a `bytearray` starts at its read cursor and leaves the cursor just past the pack.
`load()` on a stream reads to exactly the end of the pack. A failed `load()` leaves the pack as it
was.

### ResourceView

The reader for a pack that is already in memory. It parses the directory and copies nothing: `view()`
returns a `bytearray_view` pointing into the blob.

```cpp
std::optional<scl2::ResourceView> ResourceView::parse(std::span<const std::byte> blob) noexcept;
```

| Member | Description |
|---------|--------------|
| `parse(blob)` | nothing on a blob that is not a pack, or whose directory runs past the end |
| `contains(name)` / `size()` / `empty()` | query |
| `names()` | every name, in pack order |
| `view(name)` | the bytes, pointing into the blob |
| `get(name)` | the bytes, copied out |
| `info(name)` | name, size, offset |
| `blob()` | the memory this was parsed from |

### ResourceManager

The layered lookup. A manager registers itself while it lives, so the calls below are static and
reach that one manager; `res.mount(...)` written on an object reaches the same place.

| Call | Description |
|---------|--------------|
| `mount(source, pack)` | mount an owned `ResourcePack` |
| `mount(source, view)` | mount a `ResourceView`; the blob has to outlive it |
| `mount_directory(source, dir, recursive = true)` | mount a directory of ordinary files |
| `mount_pack_file(source, file)` | mount a pack stored in a file |
| `unmount(source)` | remove a source; `false` when it was not mounted |
| `clear()` | remove every source |
| `contains(name)` | whether anything answers |
| `view(name)` | the bytes without a copy; `std::nullopt` when the source cannot hand one out |
| `get(name)` | the bytes as a copy; works for every source |
| `info(name)` | name, source, type, size, offset |
| `names()` | every name, once each, in search order |
| `sources()` | the mounted source names, in mount order |

Copying a manager is not allowed: a copy would be a second registry that nothing forwards to.

### The default registry

```cpp
int main() {
    scl2::ResourceManager res;                       // registers itself, lives as long as the program
    scl2::res::mount_builtin(myapp::resources());    // parse and mount, in one call

    // ... anywhere else in the program
    if (const auto bytes = scl2::res::get(myapp::k_config_toml))
        use(*bytes);
}
```

`scl2::res` forwards to the manager the program constructed: `manager()`, `has_manager()`,
`mount_builtin(pack, source)`, `mount`, `mount_directory`, `mount_pack_file`, `unmount`,
`contains`, `get`, `view`, `info`, `names`, `sources`, and `text(name)` for bytes that are text.

The calls on `ResourceManager` itself reach the same registry, so the two spellings are
interchangeable. Asking before a manager exists throws `std::logic_error` rather than answering
"not found": a call made before `main()` constructed one is a mistake in the program, not a missing
resource. One manager per program — nothing here reaches a second one.

### Windows: bytes into platform objects

```cpp
void* to_hicon(const scl2::bytearray_view& image, int cx = 0, int cy = 0);
bool set_window_icon(void* hwnd, void* icon);
```

The handles are plain pointers so that this header does not have to include `<windows.h>`, which
defines `small`, `near` and `far` as macros and would put those into every translation unit that
uses the module. An `HWND` or an `HICON` converts to and from a plain pointer without a cast, so a
caller writes the real types as usual.

`to_hicon()` builds an `HICON` from memory — no `.rc` file, no resource section and no file on disk
take part, so an icon can come from any source this module can mount. It accepts either a complete
`.ico` or a single icon image block, and given a multi-size `.ico` it picks the frame for `cx`/`cy`;
`0` for either means "leave it as it is". The caller owns the result and passes it to
`DestroyIcon()`.

`set_window_icon()` sets both the `ICON_BIG` and `ICON_SMALL` slots, so the title bar and the
taskbar button are covered.

The icon shown for the executable itself lives in the PE resource section, which only a resource
compiler can write; this module does not touch it.

## Behaviour

- **Errors.** Anything the caller asks for that may not be there comes back as `std::optional`.
  Malformed data throws `std::runtime_error`; an empty or over-long name passed to `add()` or
  `replace()` throws `std::invalid_argument`. `ResourceView::parse()` never throws — a blob it
  cannot use is `std::nullopt`.
- **`view()` against `get()`.** `get()` always works and returns a copy. `view()` returns bytes that
  belong to the source and is offered only when the source already holds them in memory: an owned
  pack and a memory pack give views, a directory and a pack file do not. Ask for `view()` where a
  copy is a real cost, and fall back to `get()`.
- **Lifetimes.** A `ResourceView` borrows the blob it was parsed from: the blob must outlive it and
  must not move. A source mounted at a name is owned by the manager, except a view, whose blob is
  the caller's.
- **Reading a pack file.** Only the directory is held in memory; each entry is read out of the file
  when it is asked for. A pack larger than memory therefore works, as long as the individual
  entries fit.
- **Threads.** `ResourceManager` is not synchronized: mounting while another thread looks up is a
  data race. After mounting is done, lookups are safe to make from several threads, with one
  exception: a lookup that reads an entry from a directory or from a pack file opens the file for
  that call and closes it again, so no cursor is shared.
- **Resource names from a directory.** A name is a path relative to the mounted directory. A name
  that would leave it — absolute, or containing `..` — is refused rather than followed.

## Notes

- A resource name is a plain string. Nothing here reserves a prefix or a syntax: the lookup is a
  call, not a virtual file system, so a name never has to be marked as a resource path.
- The bytes a resource resolves to are ordinary bytes — a `scl2::bytearray` or a
  `scl2::bytearray_view`. They can be handed to any reader that takes one, put behind a
  `std::ispanstream` to be read as a `std::istream`, or viewed as text.
- A pack holds entries of any size, and the same name may exist in several mounted sources; the
  search order decides which one answers.
- `info()` reports the offset of an entry inside its container — the pack, or the pack file. A
  directory has no container to be offset into, so there it reports `0`.

## Limits and roadmap

- **No compression, encryption or integrity check.** A pack is a plain container; apply any of the
  three to the bytes, or to the whole pack, outside this module.
- **No platform resource source.** `ResourceType::PlatformEmbedded` and `SystemResource` are
  reserved and nothing produces them.
- **Icons only, not cursors or fonts.** `CreateIconFromResourceEx()` builds icons only.
- **Resource packing does not work when cross-compiling.** `scl2_add_resources()` builds its tool
  with the consumer's compiler, so under a cross toolchain that tool cannot run on the build
  machine. `GENERATED` and the resgen executable installed in the package's `bin/` are the way out:
  run the tool where it can run, and attach the result. This library is not expected to be used in a
  cross-compiling project — Android has its own resource system, and Windows and Linux build
  natively — so this is a stated limit rather than a planned feature.
- **A pack file is read per entry.** Nothing caches an entry, so a resource read in a hot loop is
  read from disk every time; a caller that needs that should hold the bytes itself.

## See Also

- [encoding](encoding.md) — text that goes into a resource, and the boundary it crosses to reach a console
- [toml](toml.md) — a configuration or language file to keep inside a pack
- [standalone_module](standalone_module.md) — how a module states the standard it needs
- [autofetch](cmake/autofetch.md) — getting SharedCppLib2 into a project
