# Enum - Enum Utilities

+ Name: enum
+ Namespace: `scl2`
+ Document Version: `1.0.0`

## CMake Info

| Item | Value |
|---------|---------|
| Namespace | `SharedCppLib2` |
| Library | Header-only (include directly) |

Include usage:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::basic)  # or any target that provides the include path
```

```cpp
#include <SharedCppLib2/enum.hpp>
```

## Description

The enum module provides a collection of utilities for working with C++ enumerations. It includes:

- **Enum size detection** — auto-detect the number of values in a contiguous enum
- **Bit flag operators** — generate `|`, `&`, `^`, `~` (and compound assignment) operators for bit flag enums
- **Bit flag iteration** — iterate over set bits in a bit flag enum using C++23 generators
- **Enum-to-string mapping** — map enum values to human-readable strings, with multiple fallback strategies for bit flags

## Quick Start

### Basic enum-to-string

```cpp
#include <SharedCppLib2/enum.hpp>

enum class Color {
    Red,
    Green,
    Blue
};

scl2_strenum(Color, {
    {Color::Red,   "Red"},
    {Color::Green, "Green"},
    {Color::Blue,  "Blue"}
}, strenum_fallback_value)

int main() {
    Color c = Color::Green;
    std::cout << scl2::to_string(c).value_or("Unknown") << std::endl;
    // Output: Green
    return 0;
}
```

### Bit flag enum with operators

```cpp
#include <SharedCppLib2/enum.hpp>

enum class Flags : uint32_t {
    None  = 0,
    Read  = 1 << 0,
    Write = 1 << 1,
    Exec  = 1 << 2,
};

scl2_enum_bitop(Flags)
scl2_bitenum_traits(Flags, 0, 3)

int main() {
    Flags f = Flags::Read | Flags::Write;

    for (Flags bit : scl2::bitenum_iterator(f)) {
        // Yields Flags::Read, then Flags::Write
    }

    return 0;
}
```

### Bit flag enum-to-string

```cpp
enum class Permissions : uint32_t {
    Read  = 1 << 0,
    Write = 1 << 1,
    Exec  = 1 << 2,
};

scl2_enum_bitop(Permissions)
scl2_bitenum_traits(Permissions, 0, 3)
scl2_bitstrenum(Permissions, {
    {Permissions::Read,  "Read"},
    {Permissions::Write, "Write"},
    {Permissions::Exec,  "Exec"},
}, bitstrenum_fallback_partial)

int main() {
    Permissions p = Permissions::Read | Permissions::Write;
    std::cout << scl2::to_string(p).value_or("") << std::endl;
    // Output: Read | Write
    return 0;
}
```

## Macros

### scl2_enum_size

Insert `scl2_enum_size` as the last enumerator of a contiguous enum (starting from 0) to allow automatic size detection. Not for bit flag enums.

```cpp
enum class MyEnum {
    ValueA,
    ValueB,
    ValueC,
    scl2_enum_size  // Must be last; not a real value
};
// MyEnum now has 3 valid values (0, 1, 2) and size = 3
```

> [!WARNING]
> All preceding enumerators must start at 0 and be contiguous. Do not assign explicit values.

### scl2_enum_bitop(NAME)

Generates `|`, `&`, `^`, `~` operators for the given enum type. Compound assignment operators (`|=`, `&=`, `^=`) are not provided — use `e = e | FlagA` instead.

### scl2_enum_bitopex(NAME)

Extended version of `scl2_enum_bitop` that also generates `|=`, `&=`, `^=` compound assignment operators.

### scl2_enum_bitop_inclass(NAME)

Same as `scl2_enum_bitop`, but generates `friend` functions for use inside class/struct scope.

### scl2_bitenum_traits(NAME, MIN, MAX)

Specializes `bitenum_traits_lookup` for a bit flag enum, specifying the valid bit index range `[MIN, MAX)`. This is used by `bitenum_iterator` and `bitstrenum` to determine which bits to iterate.

```cpp
scl2_bitenum_traits(MyFlags, 0, 4)  // Valid bit indices: 0, 1, 2, 3
```

### scl2_strenum(NAME, LIST, FALLBACK)

Convenience macro to create a `strenum` map for enum-to-string conversion.

```cpp
scl2_strenum(MyEnum, {
    {MyEnum::A, "ValueA"},
    {MyEnum::B, "ValueB"},
}, strenum_fallback_value)
```

### scl2_bitstrenum(NAME, LIST, FALLBACK)

Convenience macro to create a `bitstrenum` map for bit flag enum-to-string conversion.

```cpp
scl2_bitstrenum(MyFlags, {
    {MyFlags::FlagA, "FlagA"},
    {MyFlags::FlagB, "FlagB"},
}, bitstrenum_fallback_partial)
```

## Core API

### Iterator functions

#### bitenum_iterator

```cpp
template<typename E>
requires std::is_enum_v<E>
std::generator<E> bitenum_iterator(E value);
```

Iterates over set bits in a bit flag enum value. Uses `bitenum_traits_lookup<E>` to determine the valid bit range. Returns a C++23 `std::generator` that yields each individual set flag.

**Usage:**
```cpp
for (auto bit : scl2::bitenum_iterator(flags)) {
    // process each set flag
}
```

#### bitenum_ranged_iterator

```cpp
template<typename E>
requires std::is_enum_v<E>
std::generator<E> bitenum_ranged_iterator(E value, size_t minVal, size_t maxVal);
```

Same as `bitenum_iterator`, but with caller-provided bit index range `[minVal, maxVal)`. Does not depend on traits.

### String mapping classes

#### strenum

```cpp
template<typename E, typename CharT = char>
class strenum : public std::map<E, std::basic_string<CharT>>;
```

Maps enum values to strings. For non-bit flag enums.

**Constructor:**
```cpp
strenum(
    std::initializer_list<std::pair<E, std::basic_string<CharT>>> list,
    strenum_fallback_type fallback = strenum_fallback_none
);
```

**Methods:**

- `to_string(E value)` — Returns `std::optional<string_type>` with the mapped string, or fallback.

**Fallback types (`strenum_fallback_type`):**

| Value | Behavior |
|-------|----------|
| `strenum_fallback_value` | Return raw integer as string |
| `strenum_fallback_empty` | Return empty string |
| `strenum_fallback_none` | Return `std::nullopt` |

#### bitstrenum

```cpp
template<typename E, typename CharT = char>
class bitstrenum : public std::map<E, std::basic_string<CharT>>;
```

Maps bit flag enum values to strings. Iterates over set bits and joins known flag names with ` | `.

**Constructor:**
```cpp
bitstrenum(
    std::initializer_list<std::pair<E, std::basic_string<CharT>>> list,
    bitstrenum_fallback_type fallback = bitstrenum_fallback_none
);
```

**Fallback types (`bitstrenum_fallback_type`):**

| Value | Behavior |
|-------|----------|
| `bitstrenum_fallback_value` | Return raw integer |
| `bitstrenum_fallback_empty` | Return empty string |
| `bitstrenum_fallback_none` | Return `std::nullopt` |
| `bitstrenum_fallback_partial` | Return string for known bits only, ignore unknown |
| `bitstrenum_fallback_bitset` | Return `0b...` bitset string |
| `bitstrenum_fallback_combined` | Return `KnownName | 0b...` for known + unknown |
| `bitstrenum_fallback_combinedm` | Return `KnownName | 1 << n` for known + unknown |

### Free function

#### to_string

```cpp
template<typename E, typename CharT = char>
requires std::is_enum_v<E>
inline std::optional<std::basic_string<CharT>> to_string(E value);
```

Converts an enum value to its string representation using the registered `strenum` or `bitstrenum` map via ADL.

```cpp
auto s = scl2::to_string(myValue).value_or("?");
```
