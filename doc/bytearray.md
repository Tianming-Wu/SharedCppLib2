# bytearray - Binary Data Management Library

+ Name: bytearray
+ Namespace: `scl2`
+ Document Version: `1.2.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `basic` (contains bytearray) |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::basic)
```

## Description

Bytearray is a powerful binary data container that extends `std::vector<std::byte>` with comprehensive utilities for binary data manipulation, stream I/O, hex conversion, and type-safe data handling. It serves as the foundation for cryptographic operations, file processing, and low-level data manipulation in SharedCppLib2.

## Quick Start

### Basic Usage
```cpp
#include <SharedCppLib2/bytearray.hpp>

// Create from a string
scl2::bytearray data("Hello World");   // the constructor from std::string is explicit
std::cout << "Size: " << data.size() << std::endl;

// Convert to hex
std::cout << "Hex: " << data.toHex() << std::endl;

// File operations
std::ifstream file("data.bin", std::ios::binary);
scl2::bytearray file_content;
file_content.readAllFromStream(file);
```

### Advanced Data Handling
```cpp
// Type conversion: a trivially copyable object is written in as its own bytes
struct Point { int x, y; };
Point p{10, 20};
scl2::bytearray serialized;
serialized.append(p);                       // sizeof(Point) bytes

Point restored = serialized.to<Point>();    // Deserialization
```

## Core Features

### Data Construction

#### Basic Constructors
```cpp
bytearray();                                  // Empty array
bytearray(const bytearray &ba);               // Copy
explicit bytearray(const std::string &str);   // From string, raw bytes
explicit bytearray(const char *raw, size_t size);  // From raw data
explicit bytearray(size_t count);             // `count` zeroed bytes
```

#### Writing a Value
```cpp
template<typename T> void append(const T& data);  // trivially copyable
static bytearray fromTrivialType(const auto& data);
```
There is no constructor that takes an arbitrary type, so a value is written in with
`append()`, which stores its object representation - its bytes as they are in memory.

**Careful:** `bytearray(size_t count)` takes a *count*, so a one-argument construction from
an integer is not a conversion. `scl2::bytearray(42)` is 42 zeroed bytes, not the number 42;
use `append()`.

**Example:**
```cpp
int value = 42;
scl2::bytearray ba;
ba.append(value);                                    // 4 bytes
scl2::bytearray same = scl2::bytearray::fromTrivialType(value);
```

### Data Access & Manipulation

#### at & vat
```cpp
byte at(size_t i) const;
byte vat(size_t p, const byte &v = byte('\0')) const;
```
Safe element access with bounds checking and default value support.

**Example:**
```cpp
scl2::bytearray data("Hello");
std::byte b1 = data.at(0);     // 'H'
std::byte b2 = data.vat(10, std::byte{'X'});  // 'X' (safe access)
```

#### subarr
```cpp
bytearray subarr(size_t begin, size_t n = seek_end) const;
```
Extracts a subarray from the bytearray. The default `n` is `seek_end`, which means "to the
end".

**Example:**
```cpp
scl2::bytearray data("Hello World");
scl2::bytearray hello = data.subarr(0, 5);  // "Hello"
scl2::bytearray world = data.subarr(6);     // "World"
```

#### replace, insert & erase
```cpp
scl2::bytearray& replace(size_t pos, size_t len, const bytearray &data);
void insert(size_t pos, const bytearray &data);
void erase(size_t pos, size_t len);
```
Modifies content by replacing, inserting or removing data.

### Data Conversion

#### toStdString
```cpp
std::string toStdString() const;
```
Converts bytearray to std::string, the raw bytes and nothing else.

#### toHex
```cpp
std::string toHex() const;
std::string toHex(size_t begin, size_t size = seek_end) const;
```
Converts to hexadecimal string representation.

**Example:**
```cpp
scl2::bytearray data("AB");
std::cout << data.toHex();  // "4142"
```

#### toStringlist & toWStringlist
```cpp
scl2::stringlist toStringlist(const std::string& split = " ") const;
scl2::wstringlist toWStringlist(const std::wstring& split = L" ") const;
```
Splits bytearray into string list using delimiter.

#### to
```cpp
template<typename _T>
_T to() const;
```
Deserializes bytearray back to original type.

**Requirements:**
- Type must be trivially copyable
- Bytearray size must match type size
- Proper memory alignment

**Example:**
```cpp
scl2::bytearray serialized;
serialized.append(3.14f);
float value = serialized.to<float>();
```

### Stream Operations

#### readFromStream
```cpp
bool readFromStream(std::istream& is, size_t size);
```
Reads specific number of bytes from stream.

#### readAllFromStream
```cpp
bool readAllFromStream(std::istream& is);
```
Reads entire stream content (useful for files).

#### readUntilDelimiter
```cpp
bool readUntilDelimiter(std::istream& is, char delimiter = '\0');
```
Reads until specified delimiter is encountered.

#### writeRaw
```cpp
void writeRaw(std::ostream& os) const;
```
Writes raw binary data to output stream.

### Static Factory Methods

#### fromHex
```cpp
static bytearray fromHex(const std::string& hex);
```
Creates bytearray from hexadecimal string.

**Example:**
```cpp
scl2::bytearray data = scl2::bytearray::fromHex("48656c6c6f");
std::cout << data.toStdString();  // "Hello"
```

`fromHex()` skips characters that are not hex digits; it does not throw.

#### fromRaw
```cpp
static bytearray fromRaw(const char* raw, size_t size);
```
Creates bytearray from raw character data.

#### randomarray
```cpp
static bytearray randomarray(size_t size);
```
Creates a bytearray of `size` random bytes taken from the platform's entropy source.

**Example:**
```cpp
scl2::bytearray key = scl2::bytearray::randomarray(32);   // a 256-bit key
std::cout << key.toHex();                                  // e.g. "9f3c..."
```

The result cannot be seeded or reproduced, which is what makes it suitable for keys,
IVs and nonces. Bulk generation is slower than a PRNG, and this throws
`std::runtime_error` if the platform provides no entropy source.

### Utility Operations

#### append
Multiple overloads for appending various data types:
- `append(const bytearray &ba)`
- `append(const std::byte* data, size_t len)`
- `append(std::byte b)`
- `template<typename T> append(const T& data)` - any trivially copyable value, written as its own bytes
- `append(const std::string &str)` / `append(const std::wstring &str)` - uint32_t length, then the characters
- `appendRawString(const std::string &str)` - the characters alone, no length prefix
- `appendByte(uint8_t byte)`

#### reverse
```cpp
void reverse();
```
Reverses the byte order in the array.

#### swap
```cpp
void swap(bytearray &ba);
void swap(size_t a, size_t b, size_t len = 1);
```
Swaps content with another bytearray or swaps ranges within array.

## Operator Overloads

### Stream Operators
```cpp
std::ostream& operator<<(std::ostream& os, const scl2::bytearray& ba);
std::istream& operator>>(std::istream& is, bytearray& ba);
```
Binary both ways: `<<` writes the bytes as they are (no hex, no escaping), and `>>` reads
the whole stream into the bytearray. For text or hex, go through `toHex()` / `fromHex()`
and the iostream manipulators yourself.

**Example:**
```cpp
scl2::bytearray data;
data.readAllFromStream(std::cin);          // read everything
std::cout << data.toHex();                 // print it as hex
```

### Comparison
```cpp
bool operator== (const bytearray &ba) const;
```
Compares two bytearrays for exact binary equality.

## Advanced Usage

### Binary File Processing
```cpp
scl2::bytearray process_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    scl2::bytearray content;
    
    if (content.readAllFromStream(file)) {
        // Process binary data
        content.reverse();  // Example: change endianness
        return content;
    }
    throw std::runtime_error("Failed to read file");
}
```

### Network Data Handling
```cpp
void send_packet(std::ostream& network_stream, const scl2::bytearray& data) {
    // Add header
    scl2::bytearray packet;
    packet.append(static_cast<uint32_t>(data.size()));  // Size prefix
    packet.append(data);
    
    packet.writeRaw(network_stream);
}
```

### Data Serialization
```cpp
struct NetworkPacket {
    uint32_t id;
    uint16_t type;
    float value;
};

scl2::bytearray serialize_packet(const NetworkPacket& packet) {
    return scl2::bytearray(packet);  // Automatic serialization
}

NetworkPacket deserialize_packet(const scl2::bytearray& data) {
    return data.to<NetworkPacket>();
}
```

## Performance Tips

1. **Use `reserve()`** for known data sizes to avoid reallocations
2. **Prefer `append()` with size** for bulk data operations
3. **Use stream operations** for large file processing
4. **Chain operations** to minimize temporary copies

## Serialization with the read cursor

`bytearray` keeps two cursors. Sequential reads consume from the read cursor, so a decoder
can read field after field without keeping a position of its own:

```cpp
#include <SharedCppLib2/bytearray.hpp>

struct User {
    uint32_t id;
    std::string name;
    uint64_t created_at;
};

// Serialization
scl2::bytearray serialize(const User& user) {
    scl2::bytearray data;
    data.append(user.id);         // 4 bytes
    data.append(user.name);       // uint32_t length, then the characters
    data.append(user.created_at);
    return data;
}

// Deserialization
User deserialize(const scl2::bytearray& data) {
    User user;
    user.id = data.read<uint32_t>();
    user.name = data.readString();      // reads the length prefix back
    user.created_at = data.read<uint64_t>();
    return user;
}
```

### Cursors

| Member | Description |
|---------|---------|
| `read<T>()` / `readString()` / `readBytes(n)` / `readContainer<T>()` | Take from the read cursor and advance it |
| `available<T>()` / `bytesAvailable(n)` / `remaining()` | Whether a read of that size would succeed |
| `seekr(pos)` / `tellr()` | The read cursor. `seekr(seek_end)` goes to the end |
| `seekw(pos)` / `tellw()` | The write cursor, which `insert()` without a position uses |
| `rp_guard()` | Restores the read cursor when the guard goes out of scope |

Every read throws `std::out_of_range` when the data runs out, so a decoder can let the
exception carry it out instead of checking each field.

`rp_guard()` is for speculative reads - sniffing a header, trying a decode and falling back -
where the caller's cursor must not move:

```cpp
{
    auto guard = data.rp_guard();
    if (data.read<uint32_t>() == magic) { ... }   // cursor moves
}   // cursor restored
```

### bytearray_view

`bytearray_view` is a non-owning span over bytes that belong to someone else: `data()`,
`size()`, `empty()`, `operator[]`, `at()`, `subarr()` and comparison. It has no cursor of
its own - it passes a range around without copying, and parsing is the `bytearray`'s job.

```cpp
class bytearray_view {
public:
    bytearray_view(const bytearray& ba);
    bytearray_view(const std::byte* data, size_t size);

    const std::byte* data() const;
    size_t size() const;
    bool empty() const;

    std::byte operator[](size_t i) const;
    std::byte at(size_t i) const;                  // throws when out of range
    bytearray subarr(size_t begin, size_t n = bytearray::seek_end) const;

    bool operator==(const bytearray_view& other) const;
    bool operator!=(const bytearray_view& other) const;
};
```

A view cannot be built from a temporary `bytearray` (`bytearray_view(const bytearray&&)` is
deleted), so it cannot outlive the data it points at in that case.

## Memory Hygiene

`clear()` only resets the size and the two cursors; the bytes stay in the allocation. For
key material there are two additions:

#### wipe
```cpp
void wipe() noexcept;
```
Overwrites every byte with zero and keeps the size.

```cpp
scl2::bytearray key = scl2::bytearray::randomarray(32);
// ... use it ...
key.wipe();     // the buffer no longer holds the key
```

The writes go through a `volatile` pointer, so they are not removed as dead stores - a plain
`std::fill` on a buffer that is about to be freed may be. Do not use this as a general way to
zero an array; it is for data that must not be left behind.

#### secure_bytearray
```cpp
class secure_bytearray : public bytearray;
```
A `bytearray` that wipes itself when it is destroyed. Copying is deleted, moving is allowed.

```cpp
scl2::secure_bytearray key = scl2::bytearray::randomarray(32);
```

It protects the container itself and nothing else. Any `bytearray` produced by value
(`subarr()`, `readBytes()`, arithmetic, `operator+`) is a plain copy that is not wiped, and
bytes the underlying `std::vector` has already reallocated away are out of reach. Use it for
members that hold a secret for their whole lifetime.

## Error Handling

- `at()` throws `std::out_of_range` for invalid indices
- `to<T>()` throws `std::runtime_error` for size/alignment mismatches
- `bytearray::read()` and friends throw `std::out_of_range` for insufficient data
- `fromHex()` does not throw: characters that are not hex digits are skipped
- Stream operations return `bool` indicating success/failure

## Integration with Other Libraries

### SHA256 Hashing
```cpp
scl2::bytearray compute_file_hash(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    scl2::bytearray content;
    content.readAllFromStream(file);
    return scl2::sha256::getMessageDigest(content);
}
```

### StringList Conversion
```cpp
scl2::bytearray config_data("key1=value1,key2=value2");
scl2::stringlist pairs = config_data.toStringlist(",");
// Results in {"key1=value1", "key2=value2"}
```