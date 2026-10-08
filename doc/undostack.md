# undostack - Undo and Redo, as States or as Actions

+ Name: undostack
+ Namespace: `scl2`
+ Header: `<SharedCppLib2/undostack.hpp>`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | `undostack` |
| Dependencies | none |

Include usage:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::undostack)
```

```cpp
#include <SharedCppLib2/undostack.hpp>
```

> [!NOTE]
> The header only includes the standard library, so it can be used where the other SCL2 targets
> are not available.

## Description

An undo/redo history, in two forms that differ in what one step holds.

| Stack | A step holds | `undo()` runs |
|---------|---------|---------|
| `undostack<T>` | a copy of a whole state | moves the cursor to the state below it |
| `commandstack` | an action, with the object it edits | that action backwards |

One undo is one step in both. A `commandstack` step can be folded into the step below it with a
merge id, and several steps can be grouped into one with a macro.

`undostack<T>` keeps states by value, so `T` has to be copy constructible and nothing more.
`commandstack` keeps callbacks, and a callback holds a reference to whatever it edits, so the
objects being edited are not copied or stored, and one stack can drive several of them.

## Quick Start

History of states:

```cpp
#include <SharedCppLib2/undostack.hpp>

scl2::undostack<std::string> text;
text.push("hello");
text.push("hello world");

text.size();                    // 2
text.index();                   // 2, states applied
text.undo();                    // true
text.current();                 // "hello"
text.redo();                    // true
text.current();                 // "hello world"
```

History of actions:

```cpp
scl2::commandstack ur;

ur.push("rotate +90",
        [&scene, id]{ scene.rotate(id, +90); },   // redo
        [&scene, id]{ scene.rotate(id, -90); });  // undo

ur.undo();                      // scene.rotate(id, -90)
ur.redo();                      // scene.rotate(id, +90)
ur.undo_label();                // "rotate +90"
```

`push` runs the action itself, as part of the call, so a caller does not apply a change first and
then push it.

## undostack

### Pushing

| Call | Behaviour |
|---------|---------|
| `push(state)` | records a copy of the state and makes it current |
| `push(std::move(state))` | the same, moving the state in |

A push drops everything that was undone, so a redo chain only lives until the next push.

### The cursor

`index()` counts the states that are applied, from 0 to `size()`. It is 0 when everything has been
undone, and `size()` when nothing has.

| Call | Behaviour |
|---------|---------|
| `undo()` | one state back; `false` once the oldest state is current |
| `redo()` | one state forward; `false` once the newest state is current |
| `can_undo()` / `can_redo()` | whether those two would return `true` |
| `current()` | the state the cursor is on; throws `std::logic_error` when `index()` is 0 |
| `at(position)` | the state at that position, 0 being the oldest; throws `std::out_of_range` when out of range |

### Size and lifespan

| Call | Behaviour |
|---------|---------|
| `size()` / `empty()` | how many states are held |
| `clear()` | drops every state, leaving `index()` at 0 |
| `set_limit(n)` | holds at most n states, dropping the oldest ones; 0 means no limit, which is the default |
| `limit()` | the current limit |

## commandstack

### Pushing

| Call | Behaviour |
|---------|---------|
| `push(label, apply)` | one action, called with `undo_dir::Redo` now and `undo_dir::Undo` on undo |
| `push(label, redo, undo)` | two callbacks, one per direction, each taking no argument |
| `push(label, merge_id, apply)` | as above, folding into the step below when that step carries the same merge id |
| `push(label, merge_id, redo, undo)` | as above |

`label` is what `undo_label()` and `redo_label()` report. An empty action, or a pair with either
half empty, throws `std::invalid_argument`. A `merge_id` below 0 never folds; `no_merge` holds that
value.

Folding replaces the step below instead of adding a second one, so the action that folds in has to
redo and undo everything that step has done so far:

```cpp
const auto start = text.size();                  // where the run began
ur.push("typing", id_typing,
        [&text, start, newest]{ text.resize(start); text += newest; },
        [&text, start]      { text.resize(start); });
```

### Stepping

| Call | Behaviour |
|---------|---------|
| `undo()` | runs the action of the step below the cursor; `false` when there is none |
| `redo()` | runs the action of the step on the cursor; `false` when there is none |
| `can_undo()` / `can_redo()` | whether those two would return `true` |
| `undo_label()` | the label of the step `undo()` would run; empty when there is none |
| `redo_label()` | the label of the step `redo()` would run; empty when there is none |

### The history

| Call | Behaviour |
|---------|---------|
| `size()` | how many steps are held; a macro counts as one |
| `index()` | how many steps are applied |
| `empty()` | whether any step is held |
| `clear()` | drops every step |
| `set_limit(n)` / `limit()` | the limit described under undostack, counted in steps |

### Macros

```cpp
ur.macro_begin("fill 10 nodes");
for (auto* node : nodes)
    ur.push("fill", [node]{ node->fill(); }, [node]{ node->unfill(); });
ur.macro_end();
```

`macro_begin` and `macro_end` group every push made between them into one step, labelled with the
`macro_begin` label. Undoing that step runs its actions in reverse order. Macros may nest, and the
outermost one is the step. A macro that recorded nothing is dropped and does not become a step.

While a macro is open, `undo()` and `redo()` throw `std::logic_error`, and `can_undo()` and
`can_redo()` return `false`. `macro_end()` without a matching `macro_begin()` throws
`std::logic_error`.

### Clean state

| Call | Behaviour |
|---------|---------|
| `set_clean()` | marks the current position as clean |
| `is_clean()` | whether the cursor is on that position |

The mark is dropped when the step it points at is discarded, by a push that drops the redo chain or
by the limit.

## Notes

- `commandstack` stores callbacks; the objects they edit are visited and never copied.
- Every action of one `commandstack` shares one history, whatever object each of them edits.
- Nothing is serialized; a stack lives for as long as the object does.
- While a macro is open, `size()` already counts the step being built.

> [!WARNING]
> A merge id folds a push into the step below rather than adding a step, so an action that undoes
> only its own change leaves the earlier part of the folded run in place.
