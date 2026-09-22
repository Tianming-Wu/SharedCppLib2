# xkeydb - 绑定文件的键值数据库

+ 名称: xkeydb
+ 命名空间: `scl2::xkeydb`
+ 文档版本: `1.0.0`

## CMake 信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库 | `xkeydb` |
| 依赖 | `basic`、`aes`、`sha256`、`hmac`、`crc32`、`zlib`、`fileio`、`compression` |

引入方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::xkeydb)
```

## 描述

一个存放在单个文件里的键值数据库，打开期间驻留在内存中。

值就是 `scl2::variant`，所以 variant 能装的东西都能存，包括数组和嵌套对象 —— 一整棵树放在一个键下面也没问题。

数据库是整体读写的：没有增量写入的路径，一次保存就是把内存里的数据整个重写回文件。

文件可以压缩、可以做防意外损坏的校验、也可以加密。这些都记录在文件里，打开时不需要额外被告知。

`scl2::xkeydb::database` 绑定一个路径。构造它只读文件头，所以在加载任何数据之前对象就知道路径上有什么；加载本身是一个单独的步骤。

## 快速开始

```cpp
#include <SharedCppLib2/xkeydb.hpp>

scl2::xkeydb::database db(platform::executable_dir() / "main.db");

if (!db.exists()) {
    db.initialize();                     // 首次运行：路径上还什么都没有
}

if (db.needsSecret()) {
    db.unlock(password);                 // 一个 scl2::bytearray
}

db.open();                               // 加载数据

int launches = db.value("launches").as_int();
db.setValue("launches", launches + 1);

db.close();                              // 写回文件
```

## 生命周期

### 路径上到底是什么

`exists()`、`valid()`、`status()` 回答三个不同的问题，其中前两个的区别是关键：

| `status()` | `exists()` | `valid()` | 含义 |
|---------|---------|---------|---------|
| `missing` | false | false | 路径上什么都没有 |
| `ok` | true | true | 可用的数据库 |
| `not_database` | true | false | 有东西，但不是我们的文件 |
| `newer_version` | true | false | 由更新的格式版本写出 |
| `older_version` | true | false | 由更旧的格式版本写出 |
| `unsupported` | true | false | 用了本构建读不了的编码方式 |
| `damaged` | true | false | 被截断，或记录的长度对不上 |
| `inaccessible` | true | false | 路径读不了 |

`exists()` 说的是文件系统，`valid()` 说的是内容。文件存在但 `valid()` 为假，比文件不存在严重得多：那可能是一个装着真实数据的旧库，所以库自己绝不会去动它 —— `initialize()` 在路径上有东西时会拒绝，`open()` 要求 `valid()`。

### 三件事

```cpp
if (!db.exists()) db.initialize();   // 创建
if (db.needsSecret()) db.unlock(pw); // 需要秘密时提供它
db.open();                           // 加载
```

打开之后：

```cpp
db.save();                           // 写回
db.discard();                        // 丢掉改动并重新加载
db.close();                          // 写回并释放文件
```

| 调用 | 作用 |
|---------|---------|
| `initialize(opt = {})` | 创建文件并写入一个空数据库。路径上已经有东西时失败 |
| `open(inst = None)` | 加载数据。同参数重复调用不做任何事；参数不同则抛，请改用 `reopen()` |
| `close()` | 写回后释放文件。已经关闭时调用是空操作，而且它绝不会创建文件 |
| `save()` | 写回但不关闭。没有改动时不做任何事 |
| `forceSave()` | 即使没有改动也重写 |
| `discard()` | 丢掉未保存的改动，重新读取文件 |
| `reopen(inst = None)` | 先 `close()` 再 `open()`。只切换只读位不需要任何 I/O |
| `destroy()` | 删除文件，并把对象恢复成刚构造完的样子 |

`close()` 不清空数据，所以关闭之后对象读起来就像最后那份状态的快照，`hasData()` 表示有没有这么一份。注意这个快照是内存里的那份，不是文件里的现状 —— 文件可能已经被别的进程改过。

`isDirty()` 表示有没有还没落到文件里的改动。

### 错误

对当前状态没有意义的操作 —— 打开一个不是数据库的路径、给不需要秘密的库解锁、在只读状态下写 —— 抛 `scl2::xkeydb::xkeydb_exception`，它带一个 `xkeydb_error` 码。直接作用于数据的调用则返回 `xkeydb_error`，这样一次普通的 `addKey()` 失败不必被 try 包起来。`describe()` 能把 `file_status` 或 `xkeydb_error` 变成文本。

## 键与值

键是 `scl2::bytearray`，因为它本来就是任意字节串。但键绝大多数时候是文本，所以每个收键的调用也接受 `std::string_view`，字符串字面量可以直接用：

```cpp
db.setValue("name", "value");           // 存在五个字节的 "name" 下面
db.setValue(scl2::bytearray{0x00, 0x01}, 1);   // 二进制键
```

要把一个 `std::string` 本身当作键，键就是它的字节：`scl2::bytearray(str)` 和 `scl2::bytearray::fromStdString(str)` 都是这个意思。`scl2::bytearray::fromString()` 是另一回事 —— 它是序列化用的长度前缀形式，产生的键任何普通字符串都匹配不上。

键按字节序排列，`keys()` 与 `keys(prefix)` 返回的顺序也是它。

## 接口

### 类型

| 名称 | 含义 |
|---------|---------|
| `xkeydb::database` | 数据库本身 |
| `xkeydb::file_status` | 路径上是什么，见上面的表 |
| `xkeydb::xkeydb_error` | 数据操作与文件操作的结果 |
| `xkeydb::xkeydb_exception` | 生命周期调用抛出，`code()` 里是 `xkeydb_error` |
| `xkeydb::inst_config` | 本句柄的策略：`None` / `ReadOnly` / `NoImplicitSave` |
| `xkeydb::cipher_algo` | `none` / `aes_cbc_128` / `aes_cbc_256` |
| `scl2::compression_id` | 用哪个压缩算法：`none`、内置的，或应用自己注册的。见 [compression](compression.md) |
| `xkeydb::integrity_algo` | `none` / `crc32` / `sha256` / `hmac_sha256` |
| `xkeydb::file_config` | 文件用什么编码：`cipher`、`compress`、`integrity` |
| `xkeydb::xkeydb_config` | `file_config` 加 `inst_config`，见 `config()` |
| `xkeydb::init_options` | 只能在创建时决定的：`compress`、`integrity`、`wal`（预留） |
### 状态与配置

| 函数 | 说明 |
|---------|---------|
| `path()` | 这个句柄指向的文件 |
| `config()` | 文件用什么编码，以及本句柄的策略 |
| `lastError()` | 上一次报失败的调用为什么失败 |
| `isOpen()` / `hasData()` / `isDirty()` | 句柄处在生命周期的哪一步，见上 |
| `isEncrypted()` / `needsSecret()` / `isUnlocked()` | 加密状态，见下 |
| `lock(secret, algo, rounds)` | 打开加密，或换一个秘密 |
| `unlock(secret)` / `tryUnlock(secret)` | 校验秘密，并留下后续读写需要的东西 |
| `setCompression(id)` / `setIntegrity(algo)` | 改文件用什么编码 |
| `addCompressionAlgo(id, provider)` / `hasCompressionAlgo(id)` | 注册自己的算法。要在 `open()` 之前 |
### 数据

| 函数 | 说明 |
|---------|---------|
| `keyCount()` | 键的数量 |
| `hasKey(key)` | 键在不在 |
| `get(key, out)` | 把值填进 `out`，并返回键在不在 |
| `value(key)` | 取到的值；键不在时返回默认 variant |
| `addKey(key, value)` | 只在键不存在时添加，已存在返回 `AlreadyExists` |
| `setValue(key, value)` | 插入，或覆盖已有的 |
| `eraseKey(key)` | 删除键；删一个不存在的键不算错 |
| `clear()` | 删除所有键 |
| `keys()` | 全部键，按序返回。这是一份快照，边遍历边改数据库是安全的 |
| `keys(prefix)` | 所有以 `prefix` 开头的键，按序返回 |
| `entries()` | 逐对产出，按键序 |

`value()` 分不清"键不存在"和"键存在但值是 null"，两者都给默认 variant。在意这个区别时用 `get()` 或 `hasKey()`。

`entries()` 只在标准库提供 `std::generator` 时才声明。它是边走边看的，所以和 `keys()` 不同，遍历期间改动数据库是不允许的，而且它不能活得比数据库对象长。

### 文件

| 函数 | 说明 |
|---------|---------|
| `takeBackup(path)` | 在别处写一份副本。已打开的数据库完全不受影响 —— 路径、脏标志都不动 |
| `saveAs(path, inst = None)` | 写一份副本，然后把句柄移过去，后续工作都在副本上做。原文件留在原地 |
| `moveTo(path)` | 迁移文件本身：写出新的一份、删掉旧的、句柄跟过去 |
| `destroy()` | 删除文件并重置对象 |

`saveAs()` 和 `moveTo()` 在目标已存在时返回 `AlreadyExists`，不会覆盖。这四个都不看只读策略，因为它们作用于文件而不是文件的内容。

`saveAs()` 会采用新句柄的实例策略，默认是读写。只读的数据库就是靠它被带到可以编辑的地方。

## 压缩、校验与加密

压缩和校验由 `init_options` 在创建时选定，之后可以用 `setCompression()` 与 `setIntegrity()` 改；加密通过 `lock()` 设置。

### 压缩

`init_options::compress` 和 `setCompression()` 收的是 `scl2::compression_id`，所以一个库可以指名 `none`、某个内置算法，或者应用自己注册的那一个：

```cpp
constexpr scl2::compression_id myCodec{uint8_t{200}};   // user_compression_base 往上

db.addCompressionAlgo(myCodec, scl2::compression_provider::from(compress, decompress));
db.initialize({ .compress = myCodec });
```

`addCompressionAlgo()` 会拒绝低于 `user_compression_base` 的号（那个区间是库的）、已经被占用的号，以及缺了一半的 provider。它必须在 `open()` 之前调用：打开文件时要用文件里记着的号去查算法，所以打开一个指名了你没注册的算法的库，会以 `UnsupportedFormat` 失败，而不是把负载交给错误的算法去解。

一个库一旦用了自己的算法，就只有同样注册了它的程序能读 —— 库并不知道那个号是什么意思，也无从判断另一个程序对它的理解是否一致。详见 [compression](compression.md)。

### 校验

| 选择 | 需要秘密 | 能防什么 |
|---------|---------|---------|
| `none` | 否 | 什么都不防 |
| `crc32` | 否 | 意外损坏：坏扇区、写坏、拷贝被截断 |
| `sha256` | 否 | 同上，但检查强度高得多 |
| `hmac_sha256` | 是 | 以上全部，另外还能发现篡改 |

前三档都很便宜，如果你的目的只是"发现损坏"而不是"挡住别人"，它们就是正确答案 —— 相对于一次保存的其他开销可以忽略。它们防不了篡改，因为改数据的人可以顺手把校验值一起重算。

`hmac_sha256` 是任何加密算法都会带上的那一档；也可以单独用，即 `lock(secret, cipher_algo::none)`：数据保持明文可读，但改动会被发现。

### 加密

```cpp
db.lock(secret);                                   // 用 AES-CBC-256 加密
db.lock(secret, cipher_algo::aes_cbc_128);
db.lock(secret, cipher_algo::none);                // 只做认证，不加密
db.lock(secret, cipher_algo::aes_cbc_256, rounds); // 比默认更多的轮数
```

`lock()` 会重新取一份盐并立即写盘，所以对一个已经加密的库调用它等于换了一次密钥。`unlock()` 校验秘密并保留后续读写所需的东西；秘密不正确时抛 `EncryptFailure`，`tryUnlock()` 则用 `bool` 表达同一件事，给偏好返回值的调用方。

解锁是刻意做慢的，目的就是让猜测短口令变得昂贵。默认大约是零点几秒；`lock()` 的最后一个参数就是一个数据库要求不同工作量的方式，而这个选择随文件一起保存，所以以后调大它不会把自己锁在门外。

`isEncrypted()` 说的是内容有没有被隐藏。`needsSecret()` 说的是到底需不需要秘密，**打开之前要判断的是后者**。

秘密本身不会被保留，只留下从它派生出来的东西，并且会在数据库对象销毁时擦除。

## 注意

> [!NOTE]
> **两个实例策略会改变写盘的时机。** `inst_config::ReadOnly` 让所有写入失败，且 `close()` 不写盘。`inst_config::NoImplicitSave` 则只留 `save()` 一条写盘路径：`close()` 把改动留在内存里，`lock()`、`setCompression()`、`setIntegrity()` 都要等到下一次 `save()` 才生效。`initialize()` 仍然会写，因为它本来就没有东西可存。在 `NoImplicitSave` 下有未保存改动时 `reopen()` 会拒绝执行，而不是悄悄把它们丢掉 —— 先调 `save()` 或 `discard()`。

> [!NOTE]
> **保存是原子的。** 新文件写在旧文件旁边，先刷盘再替换旧的，所以读者不会看到写了一半的数据库，掉电也不会让路径指向一份从未落盘的数据。

> [!NOTE]
> **压缩通常没有收益。** `zlib` 模块写出的是固定哈夫曼块，基本压不小，甚至可能多几个字节。数据以文本为主、值得一试的时候再打开，不要默认开。

> [!NOTE]
> **格式版本是拒绝而不是翻译。** 由更新格式版本写出的文件会报告为 `newer_version` 而不是被猜着读，旧版本的则干脆不读。真需要跨版本转换时，那应该是一个独立工具的工作，而不是让每次编译都背着一套旧读取逻辑。

> [!WARNING]
> **同一时刻只用一个句柄。** 这个模块不加文件锁，也没有增量写入的路径：两个句柄各自保存，就是互相覆盖，后写的赢。用 `saveAs()` 或 `moveTo()` 把数据库存到别处之后，当前句柄会跟着去新路径，但其他句柄仍然指着旧文件。

## 相关模块

- [compression](compression.md) —— 压缩算法的命名、包装与注册
- [bytearray](bytearray.md) —— 键的类型，以及用来擦除密钥的 `wipe()`
- [aes](aes.md) —— `cipher_algo` 背后的加密算法
- [sha256](sha256.md) —— `integrity_algo::sha256` 背后的散列
- [hmac](hmac.md) —— `integrity_algo::hmac_sha256` 背后的带密钥校验
- [crc32](crc32.md) —— `integrity_algo::crc32` 背后的校验和
