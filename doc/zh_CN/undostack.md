# undostack - 撤销与重做：状态式与动作式

+ 名称: undostack
+ 命名空间: `scl2`
+ 头文件: `<SharedCppLib2/undostack.hpp>`
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库 | `undostack` |
| 依赖 | 无 |

引入方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::undostack)
```

```cpp
#include <SharedCppLib2/undostack.hpp>
```

> [!NOTE]
> 该头文件只包含标准库，因此其他 SCL2 目标用不上的场合也能用它。

## 描述

撤销/重做历史，两种形式，区别在于一步里装的是什么。

| 栈 | 一步装什么 | `undo()` 做什么 |
|---------|---------|---------|
| `undostack<T>` | 一份完整状态的副本 | 把游标移到下面那个状态 |
| `commandstack` | 一个动作，连同它作用的对象 | 反向执行那个动作 |

两者都是「一次撤销 = 一步」。`commandstack` 的一步可以用 merge id 折进下面那一步，也可以用宏把若干步并成一步。

`undostack<T>` 按值保存状态，所以 `T` 只需要可拷贝构造，别无其他要求。`commandstack` 保存回调，回调里持有它要改的对象的引用，所以被改的对象既不会被拷贝、也不会被存下来，一个栈可以同时驱动好几个这样的对象。

## 快速开始

状态历史：

```cpp
#include <SharedCppLib2/undostack.hpp>

scl2::undostack<std::string> text;
text.push("hello");
text.push("hello world");

text.size();                    // 2
text.index();                   // 2，已应用的状态数
text.undo();                    // true
text.current();                 // "hello"
text.redo();                    // true
text.current();                 // "hello world"
```

动作历史：

```cpp
scl2::commandstack ur;

ur.push("rotate +90",
        [&scene, id]{ scene.rotate(id, +90); },   // redo
        [&scene, id]{ scene.rotate(id, -90); });  // undo

ur.undo();                      // scene.rotate(id, -90)
ur.redo();                      // scene.rotate(id, +90)
ur.undo_label();                // "rotate +90"
```

`push` 自己会执行那个动作，调用方不必先改完再推入。

## undostack

### 推入

| 调用 | 行为 |
|---------|---------|
| `push(state)` | 记录一份状态副本并让它成为当前状态 |
| `push(std::move(state))` | 同上，把状态移动进去 |

推入会丢掉所有被撤销掉的状态，所以重做链只活到下一次推入之前。

### 游标

`index()` 数的是已应用的状态数，取值从 0 到 `size()`。全部撤销掉时是 0，一次都没撤过时等于 `size()`。

| 调用 | 行为 |
|---------|---------|
| `undo()` | 回退一个状态；最老的状态成为当前状态后返回 `false` |
| `redo()` | 前进一个状态；最新的状态成为当前状态后返回 `false` |
| `can_undo()` / `can_redo()` | 上面两个是否会返回 `true` |
| `current()` | 游标所在的状态；`index()` 为 0 时抛 `std::logic_error` |
| `at(position)` | 该位置上的状态，0 是最老的；越界时抛 `std::out_of_range` |

### 数量与寿命

| 调用 | 行为 |
|---------|---------|
| `size()` / `empty()` | 保存着多少个状态 |
| `clear()` | 丢掉所有状态，`index()` 归 0 |
| `set_limit(n)` | 最多保存 n 个状态，多出来的从最老的开始丢；0 表示不限，也是默认值 |
| `limit()` | 当前上限 |

## commandstack

### 推入

| 调用 | 行为 |
|---------|---------|
| `push(label, apply)` | 一个动作，现在以 `undo_dir::Redo` 调用，撤销时以 `undo_dir::Undo` 调用 |
| `push(label, redo, undo)` | 两个回调，各管一个方向，都不带参数 |
| `push(label, merge_id, apply)` | 同上，当下面那一步带着同一个 merge id 时折进它 |
| `push(label, merge_id, redo, undo)` | 同上 |

`label` 就是 `undo_label()` 与 `redo_label()` 报出来的东西。空动作，或者两个回调里缺一个，抛 `std::invalid_argument`。小于 0 的 `merge_id` 永不折叠，`no_merge` 就是这个值。

折叠是替换下面那一步，而不是再加一步，所以折进来的动作必须能重做和撤销那一步到目前为止做过的全部事情：

```cpp
const auto start = text.size();                  // 这一段的起点
ur.push("typing", id_typing,
        [&text, start, newest]{ text.resize(start); text += newest; },
        [&text, start]      { text.resize(start); });
```

### 步进

| 调用 | 行为 |
|---------|---------|
| `undo()` | 执行游标下面那一步的动作；没有时返回 `false` |
| `redo()` | 执行游标上那一步的动作；没有时返回 `false` |
| `can_undo()` / `can_redo()` | 上面两个是否会返回 `true` |
| `undo_label()` | `undo()` 会执行的那一步的标签；没有时为空 |
| `redo_label()` | `redo()` 会执行的那一步的标签；没有时为空 |

### 历史

| 调用 | 行为 |
|---------|---------|
| `size()` | 保存着多少步；一个宏算一步 |
| `index()` | 已应用多少步 |
| `empty()` | 是否一步都没有 |
| `clear()` | 丢掉所有步 |
| `set_limit(n)` / `limit()` | 与 undostack 相同的上限，按步数计 |

### 宏

```cpp
ur.macro_begin("fill 10 nodes");
for (auto* node : nodes)
    ur.push("fill", [node]{ node->fill(); }, [node]{ node->unfill(); });
ur.macro_end();
```

`macro_begin` 与 `macro_end` 把它们之间的每一次推入并成一步，标签取 `macro_begin` 的那个。撤销这一步时，它的动作按相反顺序执行。宏可以嵌套，最外层那个才是那一步。一个什么都没记下的宏会被丢掉，不会变成一步。

宏开着的时候，`undo()` 与 `redo()` 抛 `std::logic_error`，`can_undo()` 与 `can_redo()` 返回 `false`。没有配对的 `macro_begin()` 就调 `macro_end()` 抛 `std::logic_error`。

### 干净状态

| 调用 | 行为 |
|---------|---------|
| `set_clean()` | 把当前位置标为干净 |
| `is_clean()` | 游标是否就在那个位置上 |

它指向的那一步被丢掉时（被一次推入丢掉重做链，或者被上限挤出），这个标记也就没了。

## 注意事项

- `commandstack` 存的是回调；回调要改的对象只是被访问，从不被拷贝。
- 一个 `commandstack` 里的所有动作共用一份历史，不管每个动作改的是哪个对象。
- 没有任何东西会被序列化；栈的生命周期就是对象的生命周期。
- 宏开着的时候，`size()` 已经把正在构建的那一步算进去了。

> [!WARNING]
> merge id 是把一次推入折进下面那一步，而不是新增一步；所以只撤销自己那一小步的动作，会把折叠段里更早的部分留在原处。
