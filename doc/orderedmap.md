# orderedmap - Key-Value Container That Keeps Its Order

+ Name: orderedmap
+ Namespace: `scl2`
+ Document Version: `1.1.0`

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

## Naming

The module is a map first, so the map-shaped operations keep the standard names and the
standard behaviour. After that, the name says which of the two dimensions an operation acts on:

| Group | Names | Acts on |
|---------|---------|---------|
| key | `insert` / `emplace` / `insert_or_assign` / `erase` / `find` / `at` / `operator[]` | the key |
| order | `insert_at` / `erase_at` / `nth` / `index_of` / `move_before` / `splice` / `sort` / `sort_by` / `remove_if` / `unique` / `push_back` / `push_front` / `pop_back` / `pop_front` / `emplace_back` / `emplace_front` / `reverse` / `front` / `back` | the insertion order, with `std::list`'s contracts |
| key order | `sorted_begin` / `sorted_end` | the key order, which `begin` / `end` deliberately do not give |

When a signature cannot be shared, the map behaviour keeps the name: the standard spellings
`insert(pos, ...)` and `erase(pos)` are only added while a key cannot be mistaken for a position
(`key_position_distinct`). `insert_at` and `erase_at` are always there.

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
| `insert(kv)` / `insert(key, value)` / `emplace` | append a new element and return `std::pair<iterator, bool>`: where it is, and whether it is new. An existing value is never overwritten |
| `insert_or_assign` | overwrites an existing value in place, position unchanged, and returns the same pair |
| `erase(key)` | returns `false` when there was nothing to erase |
| `erase(iterator)` | erases that element and returns the next one in insertion order |

`erase(iterator)` is meant for the erase-while-iterating loop:

```cpp
for (auto it = m.begin(); it != m.end(); ) {
    if (should_drop(*it)) it = m.erase(it);
    else                  ++it;
}
```

## The Order Dimension

These act on the insertion order only: the keys, the lookup structure and what
`sorted_begin()` / `sorted_end()` walk are untouched, no element is moved in memory, and
references stay valid.

| Call | Behaviour | Cost |
|---------|---------|---------|
| `nth(index)` | the element at that position, `end()` when out of range | O(min(index, size() - index)) |
| `index_of(key)` | where that element sits, empty when the key is not there | O(n) |
| `front()` / `back()` | first and last element in the insertion order | O(1) |
| `insert_at(pos, ...)` | insert in front of `pos`; `end()` appends; an existing key is neither moved nor overwritten | O(log n) |
| `insert_at(index, ...)` | the same, reached by index | O(index) + O(log n) |
| `erase_at(pos)` / `erase_at(index)` | erase and return the next element in the insertion order | O(log n), plus O(index) |
| `move_before(pos, it)` / `splice(pos, it)` | move an element that is already there in front of `pos` | O(1) |
| `sort(comp)` / `sort(first, last, comp)` | reorder that range with `comp` on the stored pairs; stable | O(n log n) |
| `sort_by(proj)` / `sort_by(first, last, proj)` | the same, with `proj(element)` compared by `<` | O(n log n) |
| `remove_if(pred)` | remove every element the predicate accepts | O(n) |
| `unique(same)` | drop consecutive elements `same` calls duplicates, keeping the first of each run | O(n) |

To reorder by something inside the mapped value, use the projection form:

```cpp
m.sort_by([](const auto& kv) { return kv.second.age; });   // Human::age
m.sort([](const auto& a, const auto& b) { return a.second < b.second; });   // or compare it yourself
```

`sort` reorders the order list and nothing else, so `sorted_begin()` still gives the key order
afterwards — the key order is a property of the lookup structure, not of the sequence. Sorting
that key order is not something the sequence side can express, which is why `sort` takes
positions from `begin()` / `end()`.

Positions are invalidated by anything that moves elements (`insert_at`, `erase_at`,
`move_before`, `sort`); references to elements are not.

The ends work like any ordered list (O(1) for the order, O(log n) for the key, and an existing
key is neither moved nor overwritten):

- `push_back(key, value)` / `push_back(kv)` / `emplace_back(key, value)` - append one element
- `push_front(...)` / `emplace_front(...)` - the same at the front
- `pop_back()` / `pop_front()` - drop the last / the first element; undefined when empty, like
  `std::list`
- `reverse()` - reverse the insertion order, O(n)

What an ordered list has and this container deliberately does not: `resize` / `assign` (a new
element needs a key), `operator[]` / `at` with an index (`at` keeps the key meaning, so the
position form is `nth(index)`), `max_size` (allocator plumbing), and the parameterless
`unique()`: the keys inside the pairs are unique already, so comparing whole elements can never
find two consecutive equals. `unique(same)`, on whatever is actually being deduplicated, is the
form that exists.

## Deviations from std::map

One line each, as promised in the header:

- Iteration (`begin`/`end`) is the insertion order; the key order is `sorted_begin`/`sorted_end`.
- `insert` / `emplace` never overwrite an existing value; use `insert_or_assign` for that.
- The stored key is not `const`, so writing `it->first` breaks the key order (the insertion
  order is unaffected). `std::map` prevents this by typing the key as `const`.
- `erase` does not move or reorder the other elements.
- The positional operations (`insert_at` / `erase_at` / `move_before` / `sort`) are extra —
  `std::map` has no order to work with.
- `erase(const_iterator)` is only added on top of `erase(key)` while the two can be told apart.
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
