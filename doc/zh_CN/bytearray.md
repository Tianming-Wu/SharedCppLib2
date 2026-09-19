# bytearray - 二进制数据管理库

+ 名称: bytearray
+ 命名空间: `scl2`
+ 文档版本: `1.2.0`

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
scl2::bytearray data("Hello World");   // 从 std::string 的构造函数是 explicit
std::cout << "大小: " << data.size() << std::endl;

// 转换为十六进制
std::cout << "十六进制: " << data.toHex() << std::endl;

// 文件操作
std::ifstream file("data.bin", std::ios::binary);
scl2::bytearray file_content;
file_content.readAllFromStream(file);
```

### 高级数据处理
```cpp
// 类型转换：可简单复制的对象按自身字节写入
struct Point { int x, y; };
Point p{10, 20};
scl2::bytearray serialized;
serialized.append(p);                       // sizeof(Point) 个字节

Point restored = serialized.to<Point>();    // 反序列化
```

## 核心功能

### 数据构造

#### 基本构造函数
```cpp
bytearray();                                  // 空数组
bytearray(const bytearray &ba);               // 复制
explicit bytearray(const std::string &str);   // 从字符串，裸字节
explicit bytearray(const char *raw, size_t size);  // 从原始数据
explicit bytearray(size_t count);             // `count` 个零字节
```

#### 写入一个值
```cpp
template<typename T> void append(const T& data);  // 可简单复制
static bytearray fromTrivialType(const auto& data);
```
没有接受任意类型的构造函数，值要用 `append()` 写入，它存的是对象的表示（即它在内存里的字节）。

**注意：** `bytearray(size_t count)` 收的是**个数**，所以拿一个整数单参数构造不是类型转换：
`scl2::bytearray(42)` 是 42 个零字节，不是数字 42。要写值就用 `append()`。

**示例:**
```cpp
int value = 42;
scl2::bytearray ba;
ba.append(value);                                    // 4 个字节
scl2::bytearray same = scl2::bytearray::fromTrivialType(value);
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
scl2::bytearray data("Hello");
std::byte b1 = data.at(0);     // 'H'
std::byte b2 = data.vat(10, std::byte{'X'});  // 'X' (安全访问)
```

#### subarr
```cpp
bytearray subarr(size_t begin, size_t n = seek_end) const;
```
从 bytearray 中提取子数组。`n` 的默认值 `seek_end` 表示"到末尾"。

**示例:**
```cpp
scl2::bytearray data("Hello World");
scl2::bytearray hello = data.subarr(0, 5);  // "Hello"
scl2::bytearray world = data.subarr(6);     // "World"
```

#### replace、insert 与 erase
```cpp
scl2::bytearray& replace(size_t pos, size_t len, const bytearray &data);
void insert(size_t pos, const bytearray &data);
void erase(size_t pos, size_t len);
```
通过替换、插入或删除数据修改内容。

### 数据转换

#### toStdString
```cpp
std::string toStdString() const;
```
将 bytearray 转换为 std::string，就是那些裸字节。

#### toHex
```cpp
std::string toHex() const;
std::string toHex(size_t begin, size_t size = seek_end) const;
```
转换为十六进制字符串表示。

**示例:**
```cpp
scl2::bytearray data("AB");
std::cout << data.toHex();  // "4142"
```

#### toStringlist & toWStringlist
```cpp
scl2::stringlist toStringlist(const std::string& split = " ") const;
scl2::wstringlist toWStringlist(const std::wstring& split = L" ") const;
```
使用分隔符将 bytearray 分割为字符串列表。

#### to
```cpp
template<typename _T>
_T to() const;
```
将 bytearray 反序列化回原始类型。

**要求:**
- 类型必须可简单复制
- Bytearray 大小必须匹配类型大小
- 正确的内存对齐

**示例:**
```cpp
scl2::bytearray serialized;
serialized.append(3.14f);
float value = serialized.to<float>();
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
scl2::bytearray data = scl2::bytearray::fromHex("48656c6c6f");
std::cout << data.toStdString();  // "Hello"
```

`fromHex()` 会跳过不是十六进制数字的字符，不抛异常。

#### fromRaw
```cpp
static bytearray fromRaw(const char* raw, size_t size);
```
从原始字符数据创建 bytearray。

#### randomarray
```cpp
static bytearray randomarray(size_t size);
```
从平台熵源取 `size` 个随机字节，构造一个 bytearray。

**示例：**
```cpp
scl2::bytearray key = scl2::bytearray::randomarray(32);   // 一个 256 位密钥
std::cout << key.toHex();                                  // 例如 "9f3c..."
```

结果无法设种子、也无法复现，这正是它能用于密钥、IV、nonce 的原因。大块生成比 PRNG 慢；
平台若没有熵源会抛 `std::runtime_error`。

### 实用操作

#### append
多种重载用于追加各种数据类型：
- `append(const bytearray &ba)`
- `append(const std::byte* data, size_t len)`
- `append(std::byte b)`
- `template<typename T> append(const T& data)` —— 任何可简单复制的值，按自身字节写入
- `append(const std::string &str)` / `append(const std::wstring &str)` —— 先 uint32_t 长度，再是字符
- `appendRawString(const std::string &str)` —— 只有字符，没有长度前缀
- `appendByte(uint8_t byte)`

#### reverse
```cpp
void reverse();
```
反转数组中的字节顺序。

#### swap
```cpp
void swap(bytearray &ba);
void swap(size_t a, size_t b, size_t len = 1);
```
与另一个 bytearray 交换内容或交换数组内的范围。

## 操作符重载

### 流操作符
```cpp
std::ostream& operator<<(std::ostream& os, const scl2::bytearray& ba);
std::istream& operator>>(std::istream& is, bytearray& ba);
```
两个方向都是二进制的：`<<` 原样写出字节（不做十六进制、不做转义），`>>` 把整个流读进
bytearray。需要文本或十六进制时，自己过 `toHex()` / `fromHex()` 和 iostream 的操纵器。

**示例:**
```cpp
scl2::bytearray data;
data.readAllFromStream(std::cin);          // 全部读入
std::cout << data.toHex();                 // 以十六进制打印
```

### 比较
```cpp
bool operator== (const bytearray &ba) const;
```
比较两个 bytearray 的精确二进制相等性。

## 高级用法

### 二进制文件处理
```cpp
scl2::bytearray process_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    scl2::bytearray content;
    
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
void send_packet(std::ostream& network_stream, const scl2::bytearray& data) {
    // 添加头部
    scl2::bytearray packet;
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

scl2::bytearray serialize_packet(const NetworkPacket& packet) {
    return scl2::bytearray(packet);  // 自动序列化
}

NetworkPacket deserialize_packet(const scl2::bytearray& data) {
    return data.to<NetworkPacket>();
}
```

## 性能提示

1. **使用 `reserve()`** 为已知数据大小预分配空间，避免重新分配
2. **优先使用带大小的 `append()`** 进行批量数据操作
3. **使用流操作** 处理大文件
4. **链式操作** 以减少临时拷贝

## 使用读游标进行序列化

`bytearray` 自带两个游标。顺序读从读游标取，所以解包代码可以一个字段接着一个字段地读下去，
不必自己记位置：

```cpp
#include <SharedCppLib2/bytearray.hpp>

struct User {
    uint32_t id;
    std::string name;
    uint64_t created_at;
};

// 序列化
scl2::bytearray serialize(const User& user) {
    scl2::bytearray data;
    data.append(user.id);         // 4 个字节
    data.append(user.name);       // 先 uint32_t 长度，再是字符
    data.append(user.created_at);
    return data;
}

// 反序列化
User deserialize(const scl2::bytearray& data) {
    User user;
    user.id = data.read<uint32_t>();
    user.name = data.readString();      // 把长度前缀读回来
    user.created_at = data.read<uint64_t>();
    return user;
}
```

### 游标

| 成员 | 说明 |
|---------|---------|
| `read<T>()` / `readString()` / `readBytes(n)` / `readContainer<T>()` | 从读游标取出并推进它 |
| `available<T>()` / `bytesAvailable(n)` / `remaining()` | 如果按这个大小读，会不会成功 |
| `seekr(pos)` / `tellr()` | 读游标。`seekr(seek_end)` 到末尾 |
| `seekw(pos)` / `tellw()` | 写游标，不传位置的 `insert()` 用它 |
| `rp_guard()` | 离开作用域时恢复读游标 |

数据不够时每个读函数都抛 `std::out_of_range`，所以解包代码可以让异常把它带出去，不必逐个字段检查。

`rp_guard()` 是给"试探性读取"用的 —— 嗅探文件头、试一次解码再回退 —— 这种场合调用方的游标
不应该被移动：

```cpp
{
    auto guard = data.rp_guard();
    if (data.read<uint32_t>() == magic) { ... }   // 游标移动
}   // 游标恢复
```

### bytearray_view

`bytearray_view` 是一个非拥有的视图，指向别人的字节：`data()`、`size()`、`empty()`、
`operator[]`、`at()`、`subarr()` 以及比较。它自己不带游标 —— 它的作用是零拷贝地传递一段
范围，解析是 `bytearray` 的事。

```cpp
class bytearray_view {
public:
    bytearray_view(const bytearray& ba);
    bytearray_view(const std::byte* data, size_t size);

    const std::byte* data() const;
    size_t size() const;
    bool empty() const;

    std::byte operator[](size_t i) const;
    std::byte at(size_t i) const;                  // 越界时抛异常
    bytearray subarr(size_t begin, size_t n = bytearray::seek_end) const;

    bool operator==(const bytearray_view& other) const;
    bool operator!=(const bytearray_view& other) const;
};
```

不能从临时 `bytearray` 构造视图（`bytearray_view(const bytearray&&)` 已被删除），所以
在那种情况下它不会比数据活得久。

## 内存清理

`clear()` 只重置大小和两个游标，字节仍留在缓冲区里。针对密钥材料有两个补充：

#### wipe
```cpp
void wipe() noexcept;
```
把所有字节写为 0，大小不变。

```cpp
scl2::bytearray key = scl2::bytearray::randomarray(32);
// ... 使用 ...
key.wipe();     // 缓冲区里不再有密钥
```

写入经过 `volatile` 指针，因此不会被当作死存储消除 - 对一个即将释放的缓冲区做普通
`std::fill` 是有可能被消除的。不要拿它当通用的清零手段，它是给"不允许残留的数据"用的。

#### secure_bytearray
```cpp
class secure_bytearray : public bytearray;
```
析构时自动清理自己的 `bytearray`。禁止拷贝，允许移动。

```cpp
scl2::secure_bytearray key = scl2::bytearray::randomarray(32);
```

它保护的只有这个容器本身。任何按值返回的 `bytearray`（`subarr()`、`readBytes()`、
算术运算、`operator+`）都是不会被清理的普通副本，而底层 `std::vector` 已经重新分配的
内存也不在它触及范围内。适合用来做"整个生命周期内持有密钥"的成员。

## 错误处理

- `at()` 对无效索引抛出 `std::out_of_range`
- `to<T>()` 对大小/对齐不匹配抛出 `std::runtime_error`
- `bytearray::read()` 系列对数据不足抛出 `std::out_of_range`
- `fromHex()` 不抛异常：不是十六进制数字的字符会被跳过
- 流操作返回 `bool` 表示成功/失败

## 与其他库的集成

### SHA256 哈希
```cpp
scl2::bytearray compute_file_hash(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    scl2::bytearray content;
    content.readAllFromStream(file);
    return scl2::sha256::getMessageDigest(content);
}
```

### StringList 转换
```cpp
scl2::bytearray config_data("key1=value1,key2=value2");
scl2::stringlist pairs = config_data.toStringlist(",");
// 结果: {"key1=value1", "key2=value2"}
```