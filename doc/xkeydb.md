# xkeydb - File-backed Key-Value Database

+ Name: xkeydb
+ Namespace: `scl2::xkeydb`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `xkeydb` |
| Dependencies | `basic`, `aes`, `sha256`, `hmac`, `crc32`, `zlib`, `fileio`, `compression` |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::xkeydb)
```

## Description

A key-value database that lives in one file, held in memory while it is open.

The values are `scl2::variant`, so anything a variant can hold can be stored, including
arrays and nested objects. A whole tree can therefore sit under a single key.

The database is read and written as a whole. There is no incremental write path, and a
save rewrites the file from the data in memory.

The file can be compressed, checked for accidental corruption, and encrypted. All of that
is recorded in the file itself, so opening one does not require being told how it was built.

`scl2::xkeydb::database` is bound to a path. Constructing it reads the file header and
nothing else, so the object knows what is at the path before any data has been loaded, and
loading is a step of its own.

## Quick Start

```cpp
#include <SharedCppLib2/xkeydb.hpp>

scl2::xkeydb::database db(platform::executable_dir() / "main.db");

if (!db.exists()) {
    db.initialize();                     // first run: there is nothing there yet
}

if (db.needsSecret()) {
    db.unlock(password);                 // an scl2::bytearray
}

db.open();                               // load the data

int launches = db.value("launches").as_int();
db.setValue("launches", launches + 1);

db.close();                              // write it back
```

## Lifecycle

### What is at the path

`exists()`, `valid()` and `status()` answer three different questions, and the difference
between the first two is the one that matters:

| `status()` | `exists()` | `valid()` | Meaning |
|---------|---------|---------|---------|
| `missing` | false | false | there is nothing at the path |
| `ok` | true | true | a usable database |
| `not_database` | true | false | something is there, but it is not one of ours |
| `newer_version` | true | false | written by a newer format version |
| `older_version` | true | false | written by an older format version |
| `unsupported` | true | false | written with an encoding this build cannot read |
| `damaged` | true | false | truncated, or the recorded sizes do not add up |
| `inaccessible` | false | false | the path could not be read |

`exists()` is about the file system, `valid()` is about the contents. A file that is there
but is not `valid()` is a much bigger problem than one that is simply absent: it may be an
older database holding real data, so the library never touches it on its own. `initialize()`
refuses when anything is at the path, and `open()` requires `valid()`.

### The three things you can do with it

```cpp
if (!db.exists()) db.initialize();   // create
if (db.needsSecret()) db.unlock(pw); // supply the secret, if one is needed
db.open();                           // load
```

Or, once it is open:

```cpp
db.save();                           // write back
db.discard();                        // throw the changes away and reload
db.close();                          // write back and release the file
```

| Call | What it does |
|---------|---------|
| `initialize(opt = {})` | Creates the file and writes an empty database. Fails when something is already at the path |
| `open(inst = None)` | Loads the data. The same call twice does nothing; a different `inst` throws, use `reopen()` |
| `close()` | Writes back, then releases the file. Calling it when it is already closed does nothing, and it never creates a file |
| `save()` | Writes back without closing. Does nothing when nothing has changed |
| `forceSave()` | Writes back even when nothing has changed |
| `discard()` | Throws the unsaved changes away and loads the file again |
| `reopen(inst = None)` | `close()` followed by `open()`. Switching only the read-only bit needs no I/O |
| `destroy()` | Deletes the file and returns the object to how it was right after construction |

`close()` does not clear the data, so after it the object reads like a snapshot of the last
state, and `hasData()` says whether there is one. Keep in mind that the snapshot is what was
in memory, not what is in the file now: another process may have changed the file since.

`isDirty()` reports whether there are changes that have not reached the file.

### Errors

Things that make no sense for the current state - opening a path that is not a database,
unlocking something that needs no secret, writing while read-only - throw
`scl2::xkeydb::xkeydb_exception`, which carries an `xkeydb_error` code. The calls that work
on data return an `xkeydb_error` directly instead, so a normal `addKey()` failure does not
have to be caught. `describe()` turns either a `file_status` or an `xkeydb_error` into text.

## Keys and values

A key is a `scl2::bytearray`, since it is an arbitrary blob. Keys are almost always text
though, so every call that takes a key also takes a `std::string_view`, and a string literal
works directly:

```cpp
db.setValue("name", "value");           // stored under the five bytes "name"
db.setValue(scl2::bytearray{0x00, 0x01}, 1);   // a binary key
```

When a `std::string` has to become a key, its bytes are the key: `scl2::bytearray(str)` and
`scl2::bytearray::fromStdString(str)` both give that. `scl2::bytearray::fromString()` is the
length-prefixed form meant for serialization, so it produces a key that no plain string ever
matches.

Keys are kept in byte order, which is what `keys()` and `keys(prefix)` are sorted by.

## API

### Types

| Name | Meaning |
|---------|---------|
| `xkeydb::database` | The database itself |
| `xkeydb::file_status` | What is at the path, see the table above |
| `xkeydb::xkeydb_error` | Result of a data or file operation |
| `xkeydb::xkeydb_exception` | Thrown by the lifecycle calls; `code()` holds a `xkeydb_error` |
| `xkeydb::inst_config` | Policy of this handle: `None` / `ReadOnly` / `NoImplicitSave` |
| `xkeydb::cipher_algo` | `none` / `aes_cbc_128` / `aes_cbc_256` |
| `scl2::compression_id` | Which compression algorithm: `none`, a built-in, or one the application registered. See [compression](compression.md) |
| `xkeydb::integrity_algo` | `none` / `crc32` / `sha256` / `hmac_sha256` |
| `xkeydb::file_config` | What the file is encoded with: `cipher`, `compress`, `integrity` |
| `xkeydb::xkeydb_config` | `file_config` plus `inst_config`, see `config()` |
| `xkeydb::init_options` | What can only be chosen at creation: `compress`, `integrity`, `wal` (reserved) |

### State and configuration

| Function | Description |
|---------|---------|
| `path()` | The file this handle refers to |
| `config()` | What the file is encoded with, and the policy of this handle |
| `lastError()` | Why the last call that reports failure failed |
| `isOpen()` / `hasData()` / `isDirty()` | Where the handle is in its lifecycle, see above |
| `isEncrypted()` / `needsSecret()` / `isUnlocked()` | Encryption state, see below |
| `lock(secret, algo, rounds)` | Turns encryption on, or changes the secret |
| `unlock(secret)` / `tryUnlock(secret)` | Checks a secret and keeps what is needed to read and write |
| `setCompression(id)` / `setIntegrity(algo)` | Changes what the file is encoded with |
| `addCompressionAlgo(id, provider)` / `hasCompressionAlgo(id)` | Registers an algorithm of your own. Before `open()` |

### Data

| Function | Description |
|---------|---------|
| `keyCount()` | Number of keys |
| `hasKey(key)` | Whether the key is there |
| `get(key, out)` | Fills `out` and returns whether the key was there |
| `value(key)` | The value, or a default variant when the key is not there |
| `addKey(key, value)` | Adds a key that is not there yet. `AlreadyExists` when it is |
| `setValue(key, value)` | Inserts, or overwrites what is there |
| `eraseKey(key)` | Removes the key. Removing one that is not there is not an error |
| `clear()` | Removes every key |
| `keys()` | Every key, in order. A snapshot, so changing the database while walking it is safe |
| `keys(prefix)` | Every key that starts with `prefix`, in order |
| `entries()` | Every pair, one at a time, in key order |

`value()` cannot tell a missing key from one that holds a null, since both give a default
variant. Use `get()`, or `hasKey()`, when that matters.

`entries()` is only declared when the standard library has `std::generator`. It walks the
database as it goes, so unlike `keys()` it does not tolerate changes mid-walk, and it must
not outlive the database.

### The file

| Function | Description |
|---------|---------|
| `takeBackup(path)` | Writes a copy elsewhere. The open database is not disturbed at all - not its path, not its dirty flag |
| `saveAs(path, inst = None)` | Writes a copy, then moves the handle to it, so the work that follows happens on the copy. The original file is left behind |
| `moveTo(path)` | Moves the file itself: the new copy is written, the old file removed, the handle follows |
| `destroy()` | Deletes the file and resets the object |

`saveAs()` and `moveTo()` fail with `AlreadyExists` rather than writing over something. All
four ignore the read-only policy, since they act on the file rather than on its contents.

`saveAs()` takes the instance policy of the new handle, read-write by default. That is how a
read-only database can be taken somewhere it may be edited.

## Compression, integrity and encryption

`init_options` chooses compression and integrity at creation. Both can be changed later with
`setCompression()` and `setIntegrity()`, and encryption is set through `lock()`.

### Compression

`init_options::compress` and `setCompression()` take a `scl2::compression_id`, so a file can
name `none`, one of the built-in algorithms, or one the application registered itself:

```cpp
constexpr scl2::compression_id myCodec{uint8_t{200}};   // user_compression_base and up

db.addCompressionAlgo(myCodec, scl2::compression_provider::from(compress, decompress));
db.initialize({ .compress = myCodec });
```

`addCompressionAlgo()` refuses an identifier below `user_compression_base` (that range belongs
to the library), one that is already registered, and a provider that is missing a half. It has
to be called before `open()`: that is where the identifier recorded in a file is looked up, so
opening a database that names an algorithm you have not registered fails with
`UnsupportedFormat` rather than handing the payload to the wrong codec.

Once a database uses an algorithm of its own, only a program that registers the same one can
read the file - the library has no idea what that identifier means, and cannot tell whether
some other program means the same thing by it. See [compression](compression.md).

### Integrity

| Choice | Needs a secret | Catches |
|---------|---------|---------|
| `none` | no | nothing |
| `crc32` | no | accidental corruption: a bad sector, a torn write, a truncated copy |
| `sha256` | no | the same, with a much stronger check |
| `hmac_sha256` | yes | the above, and tampering as well |

The first three are cheap and are the right answer when the goal is noticing damage rather
than keeping anyone out; they cost nothing noticeable next to the rest of a save. They cannot
catch tampering, because whoever changes the data can recompute them.

`hmac_sha256` is what any cipher uses, and it can also be used on its own by calling
`lock(secret, cipher_algo::none)`, which leaves the data readable but makes changes visible.

### Encryption

```cpp
db.lock(secret);                                   // encrypt with AES-CBC-256
db.lock(secret, cipher_algo::aes_cbc_128);
db.lock(secret, cipher_algo::none);                // authenticate, do not encrypt
db.lock(secret, cipher_algo::aes_cbc_256, rounds); // more rounds than the default
```

`lock()` draws a fresh salt and writes the file straight away, so it also re-keys a database
that is already encrypted. `unlock()` checks the secret and keeps what it needs to read and
write the file; it throws `EncryptFailure` on a wrong secret, and `tryUnlock()` reports the
same thing as a `bool` for callers that prefer to ask.

Unlocking is deliberately slow, so that guessing a short secret is expensive. The default
costs on the order of a tenth of a second; the last argument of `lock()` is how a database
asks for a different amount of work, and the choice travels with the file, so raising it
later does not lock anyone out.

`isEncrypted()` says whether the contents are hidden. `needsSecret()` says whether a secret
is needed at all, and that is the one to call before `open()`.

The secret itself is not kept. Only what is derived from it, and that is erased when the
database object goes away.

## Notes

> [!NOTE]
> **Two instance policies change how the file is written.** `inst_config::ReadOnly` makes
> every write fail, and `close()` writes nothing. `inst_config::NoImplicitSave` leaves
> nothing but `save()` able to write: `close()` keeps the changes in memory, and `lock()`,
> `setCompression()` and `setIntegrity()` only take effect at the next `save()`.
> `initialize()` still writes, since there is nothing to save yet. `reopen()` refuses to run
> with unsaved changes under `NoImplicitSave` rather than quietly dropping them - call
> `save()` or `discard()` first.

> [!NOTE]
> **Saves are atomic.** The new file is written next to the old one and flushed before the
> old one is replaced, so a reader never sees a half-written database, and a power loss
> cannot leave the path pointing at data that never landed.

> [!NOTE]
> **Compression rarely pays off.** The `zlib` module writes fixed-Huffman blocks, so it
> usually saves nothing and can even add a few bytes. Turn it on when the data is mostly
> text and worth trying, not by default.

> [!NOTE]
> **The format version is checked, not translated.** A file written by a newer format
> version is reported as `newer_version` rather than guessed at, and one from an older
> version is not read at all. Converting between versions, if it is ever needed, is a job
> for a separate tool rather than something every build carries around.

> [!WARNING]
> **One handle at a time.** Nothing in this module locks the file, and there is no
> incremental write path: two handles that both save will simply overwrite each other's
> work, last writer winning. A database saved elsewhere with `saveAs()` or `moveTo()` takes
> the new path with it, but any other handle still points at the old file.

## See Also

- [compression](compression.md) — where a compression algorithm is named, wrapped and registered
- [bytearray](bytearray.md) — the key type, and the `wipe()` used to erase secrets
- [aes](aes.md) — the ciphers behind `cipher_algo`
- [sha256](sha256.md) — the hash behind `integrity_algo::sha256`
- [hmac](hmac.md) — the keyed check behind `integrity_algo::hmac_sha256`
- [crc32](crc32.md) — the checksum behind `integrity_algo::crc32`
