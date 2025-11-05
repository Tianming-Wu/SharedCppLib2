# bytearray - 二进制数据管理库

+ 名称: bytearray  
+ 命名空间: `std`  
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `basic` (包含 bytearray) |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::basic)
```

## 描述

Bytearray 是一个强大的二进制数据容器，它扩展了 `std::vector<std::byte>`，提供了全面的二进制数据操作、流 I/O、十六进制转换和类型安全数据处理功能。它作为 SharedCppLib2 中加密操作、文件处理和低级数据操作的基础。

## 快速开始

### 基本用法
```cpp
#include <SharedCppLib2/bytearray.hpp>

// 从字符串创建
std::bytearray data = "Hello World";
std::cout << "大小: " << data.size() << std::endl;

// 转换为十六进制
std::cout << "十六进制: " << data.tohex() << std::endl;

// 文件操作
std::ifstream file("data.bin", std::ios::binary);
std::bytearray file_content;
file_content.readAllFromStream(file);
```

### 高级数据处理
```cpp
// 类型转换
struct Point { int x, y; };
Point p{10, 20};
std::bytearray serialized = p;  // 自动序列化

Point restored = serialized.convert_to<Point>();  // 反序列化
```

## 核心功能

### 数据构造

#### 基本构造函数
```cpp
bytearray();  // 空数组
bytearray(const bytearray &ba);  // 复制
bytearray(const std::string &str);  // 从字符串
bytearray(const char *raw, size_t size);  // 从原始数据
```

#### 模板构造函数
```cpp
template<typename _Any>
bytearray(const _Any& in);
```
将任何可简单复制的类型序列化为 bytearray。

**示例:**
```cpp
int value = 42;
std::bytearray ba = value;  // 序列化整数
```

### 数据访问与操作

#### at & vat
```cpp
byte at(size_t i) const;
byte vat(size_t p, const byte &v = byte('\0')) const;
```
安全的元素访问，支持边界检查和默认值。

**示例:**
```cpp
std::bytearray data = "Hello";
std::byte b1 = data.at(0);     // 'H'
std::byte b2 = data.vat(10, std::byte{'X'});  // 'X' (安全访问)
```

#### subarr
```cpp
bytearray subarr(size_t begin, size_t size = -1) const;
```
从 bytearray 中提取子数组。

**示例:**
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
通过替换或插入数据修改内容。

### 数据转换

#### tostdstring
```cpp
std::string tostdstring() const;
```
将 bytearray 转换为 std::string。

#### tohex
```cpp
std::string tohex() const;
std::string tohex(size_t begin, size_t size = -1) const;
```
转换为十六进制字符串表示。

**示例:**
```cpp
std::bytearray data = "AB";
std::cout << data.tohex();  // "4142"
```

#### tostringlist & towstringlist
```cpp
std::stringlist tostringlist(const std::string& split = " ") const;
std::wstringlist towstringlist(const std::wstring& split = L" ") const;
```
使用分隔符将 bytearray 分割为字符串列表。

#### convert_to
```cpp
template<typename _T>
_T convert_to() const;
```
将 bytearray 反序列化回原始类型。

**要求:**
- 类型必须可简单复制
- Bytearray 大小必须匹配类型大小
- 正确的内存对齐

**示例:**
```cpp
std::bytearray serialized = 3.14f;
float value = serialized.convert_to<float>();
```

### 流操作

#### readFromStream
```cpp
bool readFromStream(std::istream& is, size_t size);
```
从流中读取指定数量的字节。

#### readAllFromStream
```cpp
bool readAllFromStream(std::istream& is);
```
读取整个流内容（适用于文件）。

#### readUntilDelimiter
```cpp
bool readUntilDelimiter(std::istream& is, char delimiter = '\0');
```
读取直到遇到指定的分隔符。

#### writeRaw
```cpp
void writeRaw(std::ostream& os) const;
```
将原始二进制数据写入输出流。

### 静态工厂方法

#### fromHex
```cpp
static bytearray fromHex(const std::string& hex);
```
从十六进制字符串创建 bytearray。

**示例:**
```cpp
std::bytearray data = std::bytearray::fromHex("48656c6c6f");
std::cout << data.tostdstring();  // "Hello"
```

#### fromRaw
```cpp
static bytearray fromRaw(const char* raw, size_t size);
```
从原始字符数据创建 bytearray。

### 实用操作

#### append
多种重载用于追加各种数据类型：
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
反转数组中的字节顺序。

#### swap
```cpp
void swap(bytearray &ba);
void swap(size_t a, size_t b, size_t size = 1);
```
与另一个 bytearray 交换内容或交换数组内的范围。

## 操作符重载

### 流操作符
```cpp
std::ostream& operator<<(std::ostream& os, const std::bytearray& ba);
std::istream& operator>>(std::istream& is, bytearray& ba);
```
智能流操作，自动处理十六进制/文本格式。

**示例:**
```cpp
std::bytearray data;
std::cin >> std::hex >> data;  // 读取十六进制输入
std::cout << std::hex << data; // 以十六进制输出
```

### 比较
```cpp
bool operator== (const bytearray &ba) const;
```
比较两个 bytearray 的精确二进制相等性。

## 高级用法

### 二进制文件处理
```cpp
std::bytearray process_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    std::bytearray content;
    
    if (content.readAllFromStream(file)) {
        // 处理二进制数据
        content.reverse();  // 示例：更改字节序
        return content;
    }
    throw std::runtime_error("读取文件失败");
}
```

### 网络数据处理
```cpp
void send_packet(std::ostream& network_stream, const std::bytearray& data) {
    // 添加头部
    std::bytearray packet;
    packet.append(static_cast<uint32_t>(data.size()));  // 大小前缀
    packet.append(data);
    
    packet.writeRaw(network_stream);
}
```

### 数据序列化
```cpp
struct NetworkPacket {
    uint32_t id;
    uint16_t type;
    float value;
};

std::bytearray serialize_packet(const NetworkPacket& packet) {
    return std::bytearray(packet);  // 自动序列化
}

NetworkPacket deserialize_packet(const std::bytearray& data) {
    return data.convert_to<NetworkPacket>();
}
```

## 性能提示

1. **使用 `reserve()`** 为已知数据大小预分配空间，避免重新分配
2. **优先使用带大小的 `append()`** 进行批量数据操作
3. **使用流操作** 处理大文件
4. **链式操作** 以减少临时拷贝

## 错误处理

- `at()` 对无效索引抛出 `std::out_of_range`
- `convert_to()` 对大小/对齐不匹配抛出 `std::runtime_error`
- `fromHex()` 对格式错误的十六进制字符串抛出 `std::invalid_argument`
- 流操作返回 `bool` 表示成功/失败

## 与其他库的集成

### SHA256 哈希
```cpp
std::bytearray compute_file_hash(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    std::bytearray content;
    content.readAllFromStream(file);
    return sha256::getMessageDigest(content);
}
```

### StringList 转换
```cpp
std::bytearray config_data = "key1=value1,key2=value2";
std::stringlist pairs = config_data.tostringlist(",");
// 结果: {"key1=value1", "key2=value2"}
```