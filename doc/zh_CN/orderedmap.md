# orderedmap - 会保持自身顺序的键值容器

+ 名称: orderedmap
+ 命名空间: `scl2`
+ 文档版本: `1.0.0`

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
| `insert` / `emplace` | 追加了新元素时返回 `true`，从不覆盖已有值 |
| `insert_or_assign` | 原地覆盖已有值，位置不变 |
| `erase(key)` | 没有可删的元素时返回 `false` |
| `erase(iterator)` | 删掉该元素并返回插入顺序里的下一个 |

`erase(iterator)` 是给「边遍历边删」用的：

```cpp
for (auto it = m.begin(); it != m.end(); ) {
    if (should_drop(*it)) it = m.erase(it);
    else                  ++it;
}
```

## 与 std::map 的差异

每条一行，与头文件里写的一致：

- 遍历（`begin`/`end`）是插入顺序；键顺序是 `sorted_begin`/`sorted_end`。
- `insert` / `emplace` 返回 `bool` 且不覆盖已有值，要覆盖请用 `insert_or_assign`。
- 存起来的键不是 `const`，所以写 `it->first` 会破坏键顺序（插入顺序不受影响）。`std::map` 用 `const` 键杜绝了这种情况。
- `erase` 不会搬动或重排其他元素。
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
