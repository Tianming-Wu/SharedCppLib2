# orderedmap - Key-Value Container That Keeps Its Order

+ Name: orderedmap
+ Namespace: `scl2`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `orderedmap` |
| Dependencies | none |

To include:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::orderedmap)
```

## Description

A key-value container that keeps an order of its own, so the same elements can be walked in the
order they were inserted or in key order.

| Traversal | Order | Cost per step |
|---------|---------|---------|
| `begin()` / `end()` | the order the elements were inserted in | O(1) |
| `sorted_begin()` / `sorted_end()` | key order, what `std::map` iterates in | O(1) |

Lookup, insertion and erase are O(log n). Neither insertion nor erase moves an element, so
references and pointers to the other elements stay valid; only a reference to the erased element
itself dies. A new key goes to the end of the insertion order.

`std::map` cannot do this: its key order *is* its index, and its nodes cannot be extended. This
container covers the map-like subset the library needs, not the whole STL interface.

> [!NOTE]
> The header only includes the standard library, so it can be used where the other SCL2 targets
> are not available.

## Quick Start

```cpp
#include <SharedCppLib2/orderedmap.hpp>

scl2::ordered_map<std::string, int> m;
m.insert("banana", 2);
m.insert("apple",  1);
m.insert("cherry", 3);

for (const auto& kv : m)                 // banana, apple, cherry
    std::cout << kv.first << '\n';

for (auto it = m.sorted_begin(); it != m.sorted_end(); ++it)   // apple, banana, cherry
    std::cout << it->first << '\n';
```

## Iteration

- `begin()` / `end()` walk the order of insertion. Inserting a new key appends it at the end;
  re-inserting an existing key does not move it. Erasing and inserting again is how an element
  is moved to the end.
- `sorted_begin()` / `sorted_end()` (`sorted_iterator`) walk the elements by key, which is what
  `std::map` does. Both are bidirectional iterators, so `--sorted_end()` gives the largest key.
- `iterator` converts to `const_iterator`; both compare with each other.
- Iterating does not allocate and does not modify the container.

## Looking Up and Updating

| Call | Behaviour |
|---------|---------|
| `find` / `contains` / `count` | no insertion, no exception |
| `at(key)` | throws `std::out_of_range` when the key is missing |
| `operator[](key)` | inserts `ValueType{}` when the key is missing (needs a default constructor) |
| `operator[](key) const` | there is no const version in `std::map`; this one throws instead |
| `insert` / `emplace` | return `true` when a new element was appended, never overwrite |
| `insert_or_assign` | overwrites an existing value in place, position unchanged |
| `erase(key)` | returns `false` when there was nothing to erase |
| `erase(iterator)` | erases that element and returns the next one in insertion order |

`erase(iterator)` is meant for the erase-while-iterating loop:

```cpp
for (auto it = m.begin(); it != m.end(); ) {
    if (should_drop(*it)) it = m.erase(it);
    else                  ++it;
}
```

## Deviations from std::map

One line each, as promised in the header:

- Iteration (`begin`/`end`) is the insertion order; the key order is `sorted_begin`/`sorted_end`.
- `insert` / `emplace` return `bool` and do not overwrite; use `insert_or_assign` for that.
- The stored key is not `const`, so writing `it->first` breaks the key order (the insertion
  order is unaffected). `std::map` prevents this by typing the key as `const`.
- `erase` does not move or reorder the other elements.
- No `emplace_hint`, `merge`, `extract`, `map[key]`-style node handles, allocators, or reverse
  iterators - only what the library needs.

## Notes

- The key type only needs what the comparator uses (`std::less<KeyType>`, so `operator<` by
  default) - no `operator==` / `operator!=`. A comparator that carries state is stored in the
  container and copied along with it.
- `value_type` is `std::pair<KeyType, ValueType>`. A `KeyType` or `ValueType` that cannot be
  copied still works as long as the pair and the comparator allow it.
- `check_invariants()` reports whether the container is internally consistent; it is O(n) and
  meant for diagnostics, not for a hot loop.

> [!WARNING]
> The stored key is not `const`, so writing `it->first` breaks the key order. Erase the element
> and insert it again instead.

## See Also

[json](json.md) - the objects there are held in key order, which is not the order they were
written in.
