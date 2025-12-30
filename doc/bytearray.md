# bytearray - Binary Data Management Library

+ Name: bytearray  
+ Namespace: `std`  
+ Document Version: `1.1.0`

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

// Create from string
std::bytearray data = "Hello World";
std::cout << "Size: " << data.size() << std::endl;

// Convert to hex
std::cout << "Hex: " << data.tohex() << std::endl;

// File operations
std::ifstream file("data.bin", std::ios::binary);
std::bytearray file_content;
file_content.readAllFromStream(file);
```

### Advanced Data Handling
```cpp
// Type conversion
struct Point { int x, y; };
Point p{10, 20};
std::bytearray serialized = p;  // Automatic serialization

Point restored = serialized.convert_to<Point>();  // Deserialization
```

## Core Features

### Data Construction

#### Basic Constructors
```cpp
bytearray();  // Empty array
bytearray(const bytearray &ba);  // Copy
bytearray(const std::string &str);  // From string
bytearray(const char *raw, size_t size);  // From raw data
```

#### Template Constructor
```cpp
template<typename _Any>
bytearray(const _Any& in);
```
Serializes any trivially copyable type to bytearray.

**Example:**
```cpp
int value = 42;
std::bytearray ba = value;  // Serializes the integer
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
std::bytearray data = "Hello";
std::byte b1 = data.at(0);     // 'H'
std::byte b2 = data.vat(10, std::byte{'X'});  // 'X' (safe access)
```

#### subarr
```cpp
bytearray subarr(size_t begin, size_t size = -1) const;
```
Extracts a subarray from the bytearray.

**Example:**
```cpp
std::bytearray data = "Hello World";
std::bytearray hello = data.subarr(0, 5);  // "Hello"
std::bytearray world = data.subarr(6);     // "World"
```

#### replace & insert
```cpp
std::bytearray& replace(size_t pos, size_t len, const bytearray &ba);
std::bytearray& insert(size_t pos, const bytearray &ba);
```
Modifies content by replacing or inserting data.

### Data Conversion

#### tostdstring
```cpp
std::string tostdstring() const;
```
Converts bytearray to std::string.

#### tohex
```cpp
std::string tohex() const;
std::string tohex(size_t begin, size_t size = -1) const;
```
Converts to hexadecimal string representation.

**Example:**
```cpp
std::bytearray data = "AB";
std::cout << data.tohex();  // "4142"
```

#### tostringlist & towstringlist
```cpp
std::stringlist tostringlist(const std::string& split = " ") const;
std::wstringlist towstringlist(const std::wstring& split = L" ") const;
```
Splits bytearray into string list using delimiter.

#### convert_to
```cpp
template<typename _T>
_T convert_to() const;
```
Deserializes bytearray back to original type.

**Requirements:**
- Type must be trivially copyable
- Bytearray size must match type size
- Proper memory alignment

**Example:**
```cpp
std::bytearray serialized = 3.14f;
float value = serialized.convert_to<float>();
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
std::bytearray data = std::bytearray::fromHex("48656c6c6f");
std::cout << data.tostdstring();  // "Hello"
```

#### fromRaw
```cpp
static bytearray fromRaw(const char* raw, size_t size);
```
Creates bytearray from raw character data.

### Utility Operations

#### append
Multiple overloads for appending various data types:
- `append(const bytearray &ba)`
- `append(const byte &b)`
- `append(const byte* pb, size_t size)`
- `append(const char* str, size_t size)`
- `append(const char* str)`
- `append(uint8_t val)`

#### reverse
```cpp
void reverse();
```
Reverses the byte order in the array.

#### swap
```cpp
void swap(bytearray &ba);
void swap(size_t a, size_t b, size_t size = 1);
```
Swaps content with another bytearray or swaps ranges within array.

## Operator Overloads

### Stream Operators
```cpp
std::ostream& operator<<(std::ostream& os, const std::bytearray& ba);
std::istream& operator>>(std::istream& is, bytearray& ba);
```
Smart stream operations that automatically handle hex/text formatting.

**Example:**
```cpp
std::bytearray data;
std::cin >> std::hex >> data;  // Read hex input
std::cout << std::hex << data; // Output as hex
```

### Comparison
```cpp
bool operator== (const bytearray &ba) const;
```
Compares two bytearrays for exact binary equality.

## Advanced Usage

### Binary File Processing
```cpp
std::bytearray process_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    std::bytearray content;
    
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
void send_packet(std::ostream& network_stream, const std::bytearray& data) {
    // Add header
    std::bytearray packet;
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

std::bytearray serialize_packet(const NetworkPacket& packet) {
    return std::bytearray(packet);  // Automatic serialization
}

NetworkPacket deserialize_packet(const std::bytearray& data) {
    return data.convert_to<NetworkPacket>();
}
```

## Performance Tips

1. **Use `reserve()`** for known data sizes to avoid reallocations
2. **Prefer `append()` with size** for bulk data operations
3. **Use stream operations** for large file processing
4. **Chain operations** to minimize temporary copies

## Serialization with bytearray_view

For efficient deserialization with automatic cursor management, use `bytearray_view`:

```cpp
#include <SharedCppLib2/bytearray.hpp>

struct User {
    uint32_t id;
    std::string name;
    uint64_t created_at;
};

// Serialization
std::bytearray serialize(const User& user) {
    std::bytearray data;
    data.append(std::bytearray(user.id));
    data.append(bytearray::toSafeString(user.name));
    data.append(std::bytearray(user.created_at));
    return data;
}

// Deserialization with bytearray_view
User deserialize(const std::bytearray& data) {
    std::bytearray_view view(data);
    User user;
    user.id = view.read<uint32_t>();
    user.name = view.readString();
    user.created_at = view.read<uint64_t>();
    return user;
}
```

### bytearray_view Features

- **Cursor Management**: Automatic position tracking for sequential reads
- **Safe Access**: Bounds checking with informative error messages
- **String Support**: Direct `readString()` for length-prefixed strings
- **Stream-like Interface**: Familiar `read()`, `peek()`, `seek()` operations

### bytearray_view API

```cpp
class bytearray_view {
public:
    bytearray_view(const bytearray& data);
    
    // Read operations
    template<typename _T> _T read();      // Read & advance cursor
    template<typename _T> _T peek();      // Read without advancing
    std::string readString();              // Read length-prefixed string
    std::string peekString();              // Peek string without advancing
    
    // Cursor control
    void seek(size_t pos);                // Move to absolute position
    void reset();                          // Reset cursor to 0
    size_t tell() const;                  // Get current position
    
    // Data inspection
    bool available(size_t bytes) const;   // Check if bytes available
    size_t remaining() const;              // Bytes from cursor to end
    size_t size() const;                   // Total size
    bool empty() const;                    // Check if empty
    
    // Direct access (passthrough)
    const byte* data() const;
    byte at(size_t i) const;
    bytearray subarr(size_t begin, size_t size = -1) const;
};
```

## Error Handling

- `at()` throws `std::out_of_range` for invalid indices
- `convert_to()` throws `std::runtime_error` for size/alignment mismatches
- `fromHex()` throws `std::invalid_argument` for malformed hex strings
- `bytearray_view::read()` throws `std::out_of_range` for insufficient data
- Stream operations return `bool` indicating success/failure

## Integration with Other Libraries

### SHA256 Hashing
```cpp
std::bytearray compute_file_hash(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    std::bytearray content;
    content.readAllFromStream(file);
    return sha256::getMessageDigest(content);
}
```

### StringList Conversion
```cpp
std::bytearray config_data = "key1=value1,key2=value2";
std::stringlist pairs = config_data.tostringlist(",");
// Results in {"key1=value1", "key2=value2"}
```