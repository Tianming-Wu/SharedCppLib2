# bytearray 重构迁移计划 (REByteArray → scl2::bytearray)

## 设计变更概要

REByteArray 的核心改进:
- **集成读写游标**：`read_pointer` / `write_pointer` 内置于类中，不再需要 `bytearray_view`
- **私有继承** `std::vector<uint8_t>`：不暴露 vector 的 API，接口更清晰
- **统一 append/insert**：单个模板 `append(const T&)` 替代几十个整型重载
- **内部使用 `uint8_t`** 而非 `std::byte`（避免显式构造的繁琐）

## 已确认的设计决策

| 决策 | 结论 |
|------|------|
| `peek<T>()` | 移除。用户可手动保存/恢复游标 |
| `bytearray_view` | 移除旧的游标视图；改为类似 `string_view` 的非持有切片 |
| 存储长度类型 | `uint32_t`（RE 方式）。溢出前内存先成瓶颈，流式处理才是正确方案 |
| `canbe<T>` | 重命名为 `fits<T>()`——"数据大小是否适合解释为 T" |
| `as<T>()` | 保留 const ref 版本；新增非 const ref 版本；新增 `to<T>()` 值拷贝 |
| `toStdString()` raw 版 | RE 已有 `insertRawString`/`readRawString`；增加 `toStdString()` 裸转换 |
| Base64 模块 | 合并入 `basic`；保留 `base64` INTERFACE target 重定向 |
| `peekContainer` | 移除（同 peek） |
| gdump 容器支持 | 必须保留 `appendContainer` + `readContainer` 的 gdump 版本 |
| 旧版其他功能 | 适当迁移（toHex、reverse、swap、工厂方法、流 I/O 等） |
| 兼容性 | 大版本更新，不要求完全向后兼容 |

---

## 一、可直接继承的功能（REByteArray 已有）

| REByteArray | 等价旧 API | 备注 |
|---|---|---|
| `insert(pos, T)` | `insert(pos, ba)` / 构造+append | 模板化，统一处理 |
| `insert(T)` | 无直接对应 | 在 write_pointer 处写入（新增能力） |
| `append(T)` | `append(T)` | 统一的模板替代所有重载 |
| `append(string)` | `addString(string)` | 存储格式: uint32_t 长度 + 数据 |
| `append(wstring)` | `addWString(wstring)` | 同上 |
| `read<T>()` | `bytearray_view::read<T>()` | 集成到本体 |
| `readString()` | `bytearray_view::readString()` | 集成到本体 |
| `readWString()` | `bytearray_view::readWString()` | 集成到本体 |
| `readBytes(n)` | `bytearray_view::readBytes(n)` | 集成到本体 |
| `readContainer<T>()` | `bytearray_view::readContainer<T>()` | 集成到本体 |
| `size()`, `clear()` | 同 | 继承自 vector |
| `bytesAvailable(n)` | `bytearray_view::available(n)` | 重命名 |
| `remaining()` | `bytearray_view::remaining()` | 同名 |
| `seekr(pos)` | `bytearray_view::seek(pos)` | 重命名 |
| `seekw(pos)` | 无 | 新增 |
| `tellr()`, `tellw()` | `bytearray_view::tell()` | 分读/写两个游标 |
| `as<T>()` | `convert_to<T>()` / `as<T>()` | 返回 const 引用（旧版返回值拷贝） |
| `canbe<T>()` | 无直接等价 | 检查大小是否匹配 |
| `toString()` | `toStdString()` | 从 uint32_t 前缀格式读取 |
| `fromString(s)` | `fromStdString(s)` | 工厂方法 |

---

## 二、需要新增的功能（REByteArray 缺失，旧版有）

### 2.1 读写相关
| 功能 | 旧 API | 重要性 | 实现难度 |
|---|---|---|---|
| `peek<T>()` | `bytearray_view::peek<T>()` | 中 | 低 — 保存/恢复 read_pointer |
| `peekString()` | `bytearray_view::peekString()` | 低 | 低 |
| `getref<T>()` (const 版) | 无 | 低 | 低 |

### 2.2 转换/序列化
| 功能 | 旧 API | 重要性 | 实现难度 |
|---|---|---|---|
| `toHex()` | `toHex()` | **高** — 调试、JSON 扩展 | 低 |
| `toBase64()` | `toBase64()` | **高** — datauri、settings | 低（已有 Base64 模块） |
| `fromBase64(s)` | `fromBase64(s)` | **高** — JSON、settings | 低 |
| `toEscapedString()` | `toEscapedString()` | 中 | 中 |
| `toStdString()` (raw) | `toStdString()` | 中 — 返回裸数据 | 低 |
| `toStdWString()` (raw) | `toStdWString()` | 低 | 低 |
| `toStringlist()` | `toStringlist()` | 低 | 低 |
| `toUtf8/16/32()` | `toUtf8/16/32()` | 低 — 很少使用 | 中 |
| `fromUtf8/16/32()` | `fromUtf8/16/32()` | 低 | 中 |

### 2.3 操作
| 功能 | 旧 API | 重要性 | 实现难度 |
|---|---|---|---|
| `copy_from()` / `copy_to()` | 同名 | 中 | 低 |
| `replace(pos, len, ba)` | 同名 | 中 | 低 |
| `subarr(begin, size)` | 同名 | **高** — 广泛使用 | 低 — 用 vector 构造 |
| `operator==` | 同名 | 中 | 低 |
| `operator+` | 同名 | 低 | 低 |
| `operator<<` / `operator>>` (位移) | 同名 | 低 | 中 |
| `shiftLeft/Right` | 同名 | 低 | 中 |
| `rotateLeft/Right` | 同名 | 低 | 中 |
| `reverse()` | 同名 | 低 | 低 |
| `swap(bytearray)` | 同名 | 低 | 低 |
| `vat()` | 同名 | 低 | 低 |

### 2.4 流 I/O
| 功能 | 旧 API | 重要性 | 实现难度 |
|---|---|---|---|
| `readFromStream()` | 同名 | **高** — fileio、网络 | 低 |
| `readAllFromStream()` | 同名 | **高** — fileio、rule engine | 低 |
| `readUntilDelimiter()` | 同名 | 低 | 低 |
| `writeRaw(ostream)` | 同名 | **高** | 低 |
| `operator<<(ostream)` | 外部函数 | **高** — 调试 | 低 |

### 2.5 工厂方法
| 功能 | 旧 API | 重要性 | 实现难度 |
|---|---|---|---|
| `fromHex(s)` | 同名 | 中 | 低 |
| `fromRaw(ptr, size)` | 同名 | 中 | 低 |
| `fromPointer(ptr)` | 同名 | 低 | 低 |
| `fromStdString(s)` (raw) | 同名 | 中 | 低 |
| `fromStdWString(s)` (raw) | 同名 | 低 | 低 |

### 2.6 容器写入
| 功能 | 旧 API | 重要性 | 实现难度 |
|---|---|---|---|
| `appendContainer(T)` (trivially copyable) | `appendContainer(T)` | 中 | 低 — RE 已有 `insertContainer` |
| `appendContainer(T)` (gdump) | `appendContainer(T)` | 低 | 中 |

---

## 三、设计冲突需要解决

### 3.1 `uint8_t` vs `std::byte`
- RE 内部用 `uint8_t`，但接受 `std::byte` 参数（做 cast）
- 旧版继承 `std::vector<std::byte>`，外部直接用 `std::byte`
- **建议**：保持 RE 的 `uint8_t` 内部存储 + `std::byte` 兼容接口

### 3.2 `as<T>()` 语义差异
- 旧版：返回**值拷贝**（`convert_to` → `bit_cast` 后返回）
- RE：返回 **const 引用**（`reinterpret_cast`）
- **建议**：提供两个 —— `as<T>()` 返回 const 引用，`to<T>()` 返回值拷贝（含对齐检查）

### 3.3 字符串存储格式
- 旧版 `addString`：格式为 `size_t` 长度 + 数据
- RE `append(string)`：格式为 `uint32_t` 长度 + 数据
- **破坏性变更**：不同长度类型导致二进制不兼容
- **建议**：统一用 `uint32_t`（RE 的方式），文档中说明

### 3.4 `appendContainer` 存储格式
- 旧版：`size_t count + size_t elemSize + data`
- RE (`insertContainer`)：`uint32_t count + uint32_t elemSize + data`
- **破坏性变更**：同上
- **建议**：统一用 `uint32_t`

### 3.5 `readContainer` 的 gdump 支持
- 旧版有 non-trivial element 的 gdump 容器读写
- RE 不支持
- **需要新增**：对于 `has_gdump_container<T>` 且非 trivially_copyable_container 的容器

### 3.6 `peek` 系列
- RE 没有 peek（不推进游标的读取）
- 旧版 `bytearray_view` 有 `peek<T>()`, `peekString()`, `peekBytes()`
- **可新增**：保存/恢复 read_pointer 的 peek 方法

---

## 四、bytearray_view 的处理

REByteArray 设计将 bytearray_view 的功能集成到 bytearray 本体。但 `bytearray_view` 作为**不可变视图**有其独立价值：

- 作为函数参数（明确只读语义）
- 绑定到已有 bytearray 而不复制

**建议**：保留轻量的 `bytearray_view`，但简化为仅包装 `const bytearray&` + 转发 read 方法（本身无游标，直接委托给 bytearray 的 read）。

---

## 五、影响范围（需要修改的文件）

### 直接依赖 bytearray API 的文件（需要适配）：
- `include/bytearray.hpp` — 完全重写
- `src/bytearray.cpp` — 完全重写
- `src/bytearray_base64.cpp` — 适配
- `include/bytearray_api.hpp` — `gload` 用 `bytearray_view` → 适配
- `include/bytearray_api_forward.hpp` — concept 用 `bytearray_view` → 适配
- `include/Base64.hpp` — `encode(bytearray_view)` → `encode(bytearray)`
- `src/Base64.cpp` — 适配
- `include/aes.hpp` + `src/aes.cpp` — 适配
- `include/json.hpp` + `src/json.cpp` — `bytearray_view` → `bytearray`
- `include/datauri.hpp` + `src/datauri.cpp` — 适配
- `include/settings.hpp` + `src/settings.cpp` — 适配
- `include/fileio.hpp` + `src/fileio.cpp` — 适配
- `include/hmac.hpp`, `sha256.hpp`, `crc32.hpp` + 对应 cpp — 适配
- `include/tcpclient.hpp`, `tcpserver.hpp`, `udp.hpp` — 适配
- `include/api.hpp` — 适配
- `include/condition.hpp` + `src/condition.cpp` — 适配
- `include/keydb.hpp` + `src/keydb.cpp` — 适配
- `include/ini.hpp` + `src/ini.cpp` — 前向声明 + 适配

### 文档：
- `doc/bytearray.md` + 中文版
- `doc/api_reference.md` + 中文版
- 其他引用 bytearray API 的文档

---

## 六、建议执行顺序

1. **新 bytearray.hpp** — 基于 REByteArray 重写，添加缺失功能
2. **新 bytearray.cpp** — 实现非模板方法
3. **删除 bytearray_view 独立类** — 功能集成到 bytearray
4. **适配 Base64 模块**
5. **适配 datauri 模块**
6. **适配 JSON 模块**
7. **适配 AES / hash 模块**
8. **适配网络模块** (tcpclient, tcpserver, udp)
9. **适配 settings 模块**
10. **适配 fileio / condition / keydb / ini / api 等其余模块**
11. **更新文档**
12. **更新测试**

---

## 七、风险评估

| 风险 | 等级 | 缓解 |
|---|---|---|
| 二进制格式变更（size_t→uint32_t） | **高** | 文档明确说明；提供迁移指南 |
| `bytearray_view` 移除导致大量 API 变更 | **高** | 保留轻量 view；逐个模块适配 |
| `as<T>()` 语义变化（值→引用） | 中 | 提供 `to<T>()` 作为值拷贝替代 |
| `std::byte` → `uint8_t` 内部变更 | 低 | 公共接口保持 `std::byte` 兼容 |
| `peek` 缺失 | 中 | 新增 peek 方法 |
