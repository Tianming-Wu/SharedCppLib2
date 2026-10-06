# orderedmap - 会保持自身顺序的键值容器

+ 名称: orderedmap
+ 命名空间: `scl2`
+ 文档版本: `1.1.0`

## CMake 信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库 | `orderedmap` |
| 依赖 | 无 |

引入方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::orderedmap)
```

## 描述

一个自带顺序的键值容器：同一批元素既能按插入顺序遍历，也能按键顺序遍历。

| 遍历方式 | 顺序 | 单步代价 |
|---------|---------|---------|
| `begin()` / `end()` | 元素被插入的顺序 | O(1) |
| `sorted_begin()` / `sorted_end()` | 键顺序，也就是 `std::map` 遍历的顺序 | O(1) |

查找、插入、删除都是 O(log n)。插入和删除都不搬动元素，所以指向其他元素的引用和指针始终有效，只有被删掉的那个元素自己的引用失效。新键追加在插入顺序的末尾。

`std::map` 做不到这件事：它的键顺序**就是**它的索引，它的节点也无法扩展。这个容器只覆盖库需要的 map 子集，不是完整的 STL 接口。

> [!NOTE]
> 该头文件只包含标准库，因此其他 SCL2 目标用不上的场合也能用它。

## 命名

模块首先是 map，所以 map 形状的操作保持标准名字与标准行为。名字本身则说明它作用于哪个维度：

| 分组 | 名字 | 作用于 |
|---------|---------|---------|
| 键 | `insert` / `emplace` / `insert_or_assign` / `erase` / `find` / `at` / `operator[]` | 键 |
| 顺序 | `insert_at` / `erase_at` / `nth` / `index_of` / `move_before` / `splice` / `sort` / `sort_by` / `remove_if` / `unique` / `push_back` / `push_front` / `pop_back` / `pop_front` / `emplace_back` / `emplace_front` / `reverse` / `front` / `back` | 插入顺序，契约照 `std::list` |
| 键顺序 | `sorted_begin` / `sorted_end` | 键顺序 —— `begin` / `end` 刻意不给这个 |

签名无法共用时以 map 行为优先：标准拼写 `insert(pos, ...)` 与 `erase(pos)` 只在键不可能被误当成位置时
（`key_position_distinct`）才额外提供；`insert_at` / `erase_at` 始终可用。

## 快速开始

```cpp
#include <SharedCppLib2/orderedmap.hpp>

scl2::ordered_map<std::string, int> m;
m.insert("banana", 2);
m.insert("apple",  1);
m.insert("cherry", 3);

for (const auto& kv : m)                 // banana、apple、cherry
    std::cout << kv.first << '\n';

for (auto it = m.sorted_begin(); it != m.sorted_end(); ++it)   // apple、banana、cherry
    std::cout << it->first << '\n';
```

## 遍历

- `begin()` / `end()` 走插入顺序。插入一个新键会把它追加到末尾；重复插入已存在的键不会挪动它。要把某个元素挪到末尾，
  就删掉它再插一次。
- `sorted_begin()` / `sorted_end()`（`sorted_iterator`）按键顺序遍历，也就是 `std::map` 的顺序。两者都是双向迭代器，
  所以 `--sorted_end()` 得到最大的键。
- `iterator` 可以转换成 `const_iterator`，两者之间可以互相比较。
- 遍历不分配内存，也不修改容器。

## 查找与更新

| 调用 | 行为 |
|---------|---------|
| `find` / `contains` / `count` | 不插入，不抛异常 |
| `at(key)` | 键不存在时抛 `std::out_of_range` |
| `operator[](key)` | 键不存在时插入 `ValueType{}`（要求可默认构造） |
| `operator[](key) const` | `std::map` 没有 const 版本；这里没有就抛异常 |
| `insert(kv)` / `insert(key, value)` / `emplace` | 追加新元素，返回 `std::pair<iterator, bool>`：位置，以及是否是新增。从不覆盖已有值 |
| `insert_or_assign` | 原地覆盖已有值，位置不变，返回同样的 pair |
| `erase(key)` | 没有可删的元素时返回 `false` |
| `erase(iterator)` | 删掉该元素并返回插入顺序里的下一个 |

`erase(iterator)` 是给「边遍历边删」用的：

```cpp
for (auto it = m.begin(); it != m.end(); ) {
    if (should_drop(*it)) it = m.erase(it);
    else                  ++it;
}
```

## 顺序维度

这些只作用于插入顺序：键、查找结构、以及 `sorted_begin()` / `sorted_end()` 走的东西都不变；元素在内存里
也不搬动，所以引用始终有效。

| 调用 | 行为 | 代价 |
|---------|---------|---------|
| `nth(index)` | 该位置上的元素，越界时返回 `end()` | O(min(index, size() - index)) |
| `index_of(key)` | 该元素在插入顺序里的位置；键不存在时为空 | O(n) |
| `front()` / `back()` | 插入顺序的第一个 / 最后一个元素 | O(1) |
| `insert_at(pos, ...)` | 插到 `pos` 之前；`end()` 就是追加；键已存在时既不搬动也不覆盖 | O(log n) |
| `insert_at(index, ...)` | 同上，用下标到达位置 | O(index) + O(log n) |
| `erase_at(pos)` / `erase_at(index)` | 删除并返回插入顺序里的下一个 | O(log n)，下标形式另加 O(index) |
| `move_before(pos, it)` / `splice(pos, it)` | 把已有元素挪到 `pos` 之前 | O(1) |
| `sort(comp)` / `sort(first, last, comp)` | 用 `comp` 作用于存储的 pair 来重排该范围，稳定 | O(n log n) |
| `sort_by(proj)` / `sort_by(first, last, proj)` | 同上，比较的是 `proj(element)`（用 `<`） | O(n log n) |
| `remove_if(pred)` | 删掉所有被谓词接受的元素 | O(n) |
| `unique(same)` | 丢掉相邻且被 `same` 判为重复的元素，每段保留第一个 | O(n) |

要按 mapped_type 里的某个东西排序，用投影形式：

```cpp
m.sort_by([](const auto& kv) { return kv.second.age; });   // Human::age
m.sort([](const auto& a, const auto& b) { return a.second < b.second; });   // 或者自己写比较器
```

`sort` 只重排顺序链表，所以之后 `sorted_begin()` 仍然是键顺序 —— 键序是查找结构的性质，不是序列的性质。
「对键序排序」在顺序面里不是一个能表达的操作，这也是 `sort` 取 `begin()` / `end()` 位置的原因。

位置会被任何搬动元素的操作作废（`insert_at`、`erase_at`、`move_before`、`sort`）；元素的引用不会。

端部操作与普通有序表一致（顺序 O(1)、键 O(log n)；键已存在时既不搬动也不覆盖）：

- `push_back(key, value)` / `push_back(kv)` / `emplace_back(key, value)` —— 追加一个元素
- `push_front(...)` / `emplace_front(...)` —— 同上，加到最前
- `pop_back()` / `pop_front()` —— 删掉最后 / 第一个元素；空容器时未定义，与 `std::list` 一致
- `reverse()` —— 反转插入顺序，O(n)

有序表有、而这里刻意不提供的：`resize` / `assign`（新元素需要一个键）、带下标的 `operator[]` / `at`
（`at` 保持键语义，位置形式是 `nth(index)`）、`max_size`，以及**无参的 `unique()`** —— pair 里的键本身
就唯一，比较整个元素永远不可能发现相邻重复；实际要去的重得用 `unique(same)` 指明。

## 与 std::map 的差异

每条一行，与头文件里写的一致：

- 遍历（`begin`/`end`）是插入顺序；键顺序是 `sorted_begin`/`sorted_end`。
- `insert` / `emplace` 不覆盖已有值，要覆盖请用 `insert_or_assign`。
- 存起来的键不是 `const`，所以写 `it->first` 会破坏键顺序（插入顺序不受影响）。`std::map` 用 `const` 键杜绝了这种情况。
- `erase` 不会搬动或重排其他元素。
- 位置操作（`insert_at` / `erase_at` / `move_before` / `sort`）是额外提供的：`std::map` 没有顺序可用。
- `erase(const_iterator)` 只在能与 `erase(key)` 区分开时额外提供。
- 没有 `emplace_hint`、`merge`、`extract`、节点句柄、分配器、反向迭代器 —— 只有库需要的那部分。

## 注意事项

- 键类型只需要满足比较器用到的部分（默认 `std::less<KeyType>`，即 `operator<`），不需要 `operator==` / `operator!=`。
  带状态的比较器会存在容器里，并随容器一起拷贝。
- `value_type` 是 `std::pair<KeyType, ValueType>`。不能拷贝的 `KeyType` / `ValueType` 只要 pair 和比较器允许，
  照样能用。
- `check_invariants()` 返回容器内部是否自洽；O(n)，供诊断使用，不要放进热路径。

> [!WARNING]
> 存起来的键不是 `const`，写 `it->first` 会破坏键顺序。要改键请删掉该元素再重新插入。

## 相关模块

[json](json.md) - 它里面的对象按键顺序保存，而不是按写进去的顺序。
