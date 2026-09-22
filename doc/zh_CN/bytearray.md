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
bytearray();                                        // 空数组
bytearray(const bytearray &ba);                     // 复制
bytearray(std::byte b);                             // 单个字节
explicit bytearray(const std::string &str);         // 从字符串，裸字节
explicit bytearray(const char *raw, size_t size);   // 从原始数据
explicit bytearray(const std::byte *raw, size_t size);
explicit bytearray(const void *raw, size_t size);
explicit bytearray(size_t count);                   // `count` 个零字节
explicit bytearray(size_t count, std::byte value);  // `count` 个 `value`
bytearray(std::initializer_list<std::byte> init);
template<typename InputIt> bytearray(InputIt first, InputIt last);
```

#### 写入一个值
```cpp
template<typename T> void append(const T& data);  // 可简单复制
static bytearray fromTrivialType(const auto& data);
```
没有接受任意类型的构造函数。值用 `append()` 写入，存的是对象的表示（即它在内存里的字节）；
想要一个表达式而不是两条语句时，`fromTrivialType()` 干的是同一件事。

**注意：** `bytearray(size_t count)` 收的是**个数**，所以拿一个整数单参数构造不是类型转换：
`scl2::bytearray(42)` 是 42 个零字节，不是数字 42。

**示例:**
```cpp
int value = 42;
scl2::bytearray ba = scl2::bytearray::fromTrivialType(value);   // 4 个字节
std::cout << ba.size();                                         // 4

scl2::bytearray same;
same.append(value);                                             // 一样，只是写成两条语句
```

#### B、PCB 与 bytes
```cpp
#define B(IN)   std::byte{IN}                            // 字节字面量，写起来短一点
#define PCB(IN) reinterpret_cast<const std::byte*>(&IN)  // 一个值的字节

template<size_t ContentSize> struct bytes;
```
`B(0x08)` 就是 `std::byte{0x08}`。`PCB(v)` 是把 `v` 的地址按字节看，正是那些
"指针 + 长度" 重载要的东西：

```cpp
uint32_t v = 0x12345678;
ba.append(PCB(v), sizeof(v));
```

如果这两个宏名字碍事，就在包含头文件前定义 `BYTEARRAY_NODEFINE`。`scl2::bytes<N>` 是一块
自带大小的定长字节块；它是以后那套定长构造 API 要收的参数类型，在那之前它和任何可简单
复制的值一样可以直接 append。

### 数据访问与操作

#### size、游标与缓冲区
```cpp
size_t size() const;
bool empty() const;
void clear();                          // 大小归零，两个游标也回到 0
const std::byte* data() const;         // 另有可变重载

tellr()、seekr(pos)                    // 读游标；seekr 是 const，游标本身 mutable
tellw()、seekw(pos)                    // 写游标，不传位置的 insert() 用它
bytesAvailable(length)、remaining()    // 还剩多少可读
available<T>()、fits<T>()              // 够不够读一个 T；是否正好 sizeof(T)
```
位置传 `seek_end` 表示"到末尾"。

#### 元素访问
```cpp
const std::byte& operator[](size_t i) const;   // 另有可变重载；不做边界检查
std::byte at(size_t i) const;                  // 越界抛 std::out_of_range
std::byte vat(size_t p, const std::byte& v = std::byte{0}) const;   // 越界返回 `v`
std::byte& front();  std::byte& back();
void push_back(std::byte b);
void resize(size_t n);  void resize(size_t n, std::byte v);
void reserve(size_t n);
```
迭代用通常的 `begin()` / `end()` / `cbegin()` / `cend()`。

**示例:**
```cpp
scl2::bytearray data("Hello");
std::byte b1 = data.at(0);     // 'H'
std::byte b2 = data.vat(10, std::byte{'X'});  // 'X' (安全访问)
```

#### copy_from 与 copy_to
```cpp
void copy_from(const void* raw, size_t size);   // 用 `size` 个字节替换内容
void copy_to(void* raw, size_t size) const;     // 把内容拷出去
```
两个函数对空指针、以及放不下的长度都抛 `std::invalid_argument`。

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

#### toWString、toStdWString、toString
```cpp
std::wstring toWString() const;      // 先 uint32_t 长度，再是字符
std::wstring toStdWString() const;   // 裸字节，按 wchar_t 读
std::string toString() const;        // toWString() 的 char 版本
```
带 `String` 的三个和带 `StdString` 的三个的区别，和下面 `fromString()` 与 `fromStdString()`
一样：一个要求前面有长度前缀，一个不要求。

#### toEscapedString、xtoEscapedString
```cpp
std::string toEscapedString() const;    // 可打印 ASCII 保留，其余转义
std::string xtoEscapedString() const;   // 每个字节都写成 \xNN
```
两者都是给日志、或者把字节嵌进文本用的。

#### toUtf8、toUtf16、toUtf32、toBase64
```cpp
u8string toUtf8() const;                 // 以及 toUtf16() / toUtf32()
std::string toBase64() const;
```
UTF 那三个只是把字节按那种编码重新解释，不转换也不校验。

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

#### as
```cpp
template<typename T> const T& as() const;
template<typename T> T& as();
```
把整个内容当成一个 `T` 读，不拷贝。大小必须正好是 `sizeof(T)`，否则抛 `std::out_of_range`。

**示例:**
```cpp
scl2::bytearray ba = scl2::bytearray::fromTrivialType(cfg);
myConfig back = ba.as<myConfig>();   // 拿到引用，再拷进 `back`
```

#### to
```cpp
template<typename _T>
_T to() const;
```
把 bytearray 反序列化回原始类型，按值返回。

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

#### toContainer
```cpp
template<typename _T> _T toContainer() const;
```
把内容装成一个容器（`_T` 的 `value_type` 可简单复制），元素个数由大小推出来。

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

#### fromRaw、fromPointer
```cpp
static bytearray fromRaw(const char* raw, size_t size);        // 另有 unsigned char 重载
static bytearray fromPointer(const void* ptr);                 // `ptr` 指向的那些字节
```
从原始字符数据创建 bytearray，或者从一个指针指向的内容创建。

#### fromString、fromWString
```cpp
static bytearray fromString(const std::string& str);    // 先 uint32_t 长度，再是字符
static bytearray fromWString(const std::wstring& str);
```
`readString()` / `toWString()` 的逆操作。

#### fromStdString、fromStdWString
```cpp
static bytearray fromStdString(const std::string& str);  // 只有字节
static bytearray fromStdWString(const std::wstring& str);
```
`toStdString()` / `toStdWString()` 的逆操作。注意 `fromString()` 和 `fromStdString()` **不能**
互替：前者会写长度前缀，后者不写。

#### fromBase64
```cpp
static bytearray fromBase64(const std::string& base64);
```

#### fromUtf8、fromUtf16、fromUtf32
```cpp
static bytearray fromUtf8(const std::u8string& utf8);   // 以及 UTF-16 / UTF-32 的同名函数
```
取那种字符串的字节，不转换也不校验。

#### fromTrivialType
```cpp
static bytearray fromTrivialType(const auto& data);
```
把一个可简单复制的值按自身字节写入。见[写入一个值](#写入一个值)。

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

#### insert 与 append
写侧共有三个位置，每个位置都是同一套重载：
```cpp
void insert(size_t pos, const bytearray &data);   // 写到指定位置
void insert(const bytearray &data);               // 写到写游标处
void append(const bytearray &data);               // 写到末尾
```
- `(const std::byte* data, size_t len)` —— 裸字节
- `(std::byte b)` —— 一个字节
- `(const T& data)` —— 任何可简单复制的值，按自身字节写入
- `(const std::string &str)` / `(const std::wstring &str)` —— 先 uint32_t 长度，再是字符
- `insertRawString(pos, str)` / `appendRawString(str)` —— 只有字符，没有长度前缀
- `insertRawWString(pos, str)` / `appendRawWString(str)` —— 宽字符版同理
- `insertByte(pos, uint8_t byte)` / `appendByte(uint8_t byte)` —— 从普通整数写一个字节
- `insertContainer(pos, const C &container)` / `appendContainer(const C &container)` —— 先个数、再元素大小、再元素

字符串形式与读函数是成对的：`append(str)` 写出的正是 `readString()` 读回的东西，
`appendRawString(str)` 写出的则是 `readRawString(n)` 读回的。

#### 移位、旋转与位运算
```cpp
bytearray shiftLeft(size_t offset) const;     // 以及 shiftRight()，offset 按字节
bytearray rotateLeft(size_t offset) const;    // 以及 rotateRight()
bytearray bitShiftLeft(size_t offset) const;  // 以及 bitShiftRight()，offset 按位
bytearray bitRotateLeft(size_t offset) const; // 以及 bitRotateRight()
bytearray bitTakeLeft(size_t bitCount) const; // 以及 bitTakeRight()，返回取下的位
```
每个都返回新的 bytearray，不动原来的那个。

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
bool operator==(const bytearray& other) const;
bool operator!=(const bytearray& other) const;
bool operator<(const bytearray& other) const;        // 字典序，仅供排序
bytearray operator+(const bytearray& other) const;   // 拼接
```
`==` 比的是字节是否完全相同。`operator<` 是字典序，给 `std::map` / `std::set` 的键用，
没有数值含义。

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
| `read<T>()` / `readString()` / `readWString()` / `readBytes(n)` / `readContainer<T>()` | 从读游标取出并推进它 |
| `readRawString(n)` / `readRawWString(n)` | 取 `n` 个字符，没有长度前缀 |
| `getref<T>()` | 拿到接下来 `sizeof(T)` 字节的可变引用，游标同时推进 |
| `available<T>()` / `bytesAvailable(n)` / `remaining()` | 如果按这个大小读，会不会成功 |
| `seekr(pos)` / `tellr()` | 读游标。`seekr(seek_end)` 到末尾 |
| `seekw(pos)` / `tellw()` | 写游标，不传位置的 `insert()` 用它 |
| `rp_guard()` | 离开作用域时恢复读游标 |

数据不够时每个读函数都抛 `std::out_of_range`，所以解包代码可以让异常把它带出去，不必逐个字段检查。

`rp_guard()` 返回一个 `read_guard`：只可移动，析构时恢复游标，异常退出作用域也照样恢复。
它是给"试探性读取"用的 —— 嗅探文件头、试一次解码再回退 —— 这种场合调用方的游标
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