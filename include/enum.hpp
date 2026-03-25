/*
    Enum module for SharedCppLib2.

    Enum: Normal enum type



    BitEnum: Enum type that is designed to be used as bit flags. It
             provides bitwise operators and some helper functions.

        Range behavior:
        - bitenum_iterator(value):
            Uses scl2_bitenum_traits(E, min, max) if provided, otherwise
            falls back to full underlying bitwidth [0, bitwidth).
        - bitenum_ranged_iterator(value, minVal, maxVal):
            Always uses caller-provided range [minVal, maxVal) and does not
            depend on traits.


*/

#pragma once

#include <type_traits>
#include <generator> // for coroutine

// Defense against windows.h macros
// Who would ever use them nowadays?
// If you are working with C++23, which is the basic requirement for
// using this library, I believe that you should be using the ones
// in the std library, not those shitty macros that pollute everything.

#ifdef min
    #undef min
#endif

#ifdef max
    #undef max
#endif


namespace scl2 {

// template<typename E>
// requires std::is_enum_v<E>
// struct bitenum_traits {
//     using underlying_type = std::underlying_type_t<E>;
//     static constexpr size_t bitwidth = sizeof(underlying_type) * 8;
// };


template<typename E, size_t _min, size_t _max>
requires std::is_enum_v<E>
struct bitenum_traits {
    static constexpr size_t bitwidth = sizeof(std::underlying_type_t<E>) * 8;
    static constexpr size_t min = _min, max = _max;
};

template<typename E>
requires std::is_enum_v<E>
struct bitenum_traits_lookup {
    static constexpr size_t bitwidth = sizeof(std::underlying_type_t<E>) * 8;
    static constexpr size_t min = 0;
    static constexpr size_t max = bitwidth;
};

#define scl2_bitenum_traits(NAME, MIN, MAX) \
    template<> struct ::scl2::bitenum_traits_lookup<NAME> { \
        static constexpr size_t bitwidth = sizeof(std::underlying_type_t<NAME>) * 8; \
        static constexpr size_t min = (MIN); \
        static constexpr size_t max = (MAX); \
        static_assert(min < bitwidth, "bitenum_traits min must be less than the bitwidth of the underlying type"); \
        static_assert(max <= bitwidth, "bitenum_traits max must be less than or equal to the bitwidth of the underlying type"); \
        static_assert(min < max, "bitenum_traits min must be less than max"); \
    };


// For developers:
/* Example:

enum class myFlags : uint32_t {
    FlagA = 1 << 0,
    FlagB = 1 << 1,
    FlagC = 1 << 2,
};

scl2_enum_bitop(myFlags) // to enable bitwise operators for myFlags
scl2_bitenum_traits(myFlags, 0, 3) // to specify the valid bit range for myFlags (0 to 2 in this case, since we have 3 flags)

Then scl2's bitenum utilities can recognize the range of your enum and work properly with it.
*/


/// @brief Allow to scroll through bitwide flags using grammer `for (auto bit : enum_bitwiden(flags))`
/// @tparam E An enum type with bitwise flags, and & operator support.
/// @param value The enum value containing bitwise flags.
/// @return A vector of individual bit flags set in the value.
template<typename E>
requires std::is_enum_v<E>
std::generator<E> bitenum_iterator(E value) {
    using T = std::underlying_type_t<E>;
    using U = std::make_unsigned_t<T>;

    constexpr size_t min = bitenum_traits_lookup<E>::min;
    constexpr size_t max = bitenum_traits_lookup<E>::max;

    const U raw = static_cast<U>(value);
    for (size_t bit_index = min; bit_index < max; ++bit_index) {
        const U bit = static_cast<U>(U{1} << bit_index);
        if ((raw & bit) != 0) {
            co_yield static_cast<E>(static_cast<T>(bit));
        }
    }
}

/// @brief Iterate set flags with caller-provided bit range [minVal, maxVal).
/// @tparam E An enum type with bitwise flags, and & operator support.
/// @param value The enum value containing bitwise flags.
/// @param minVal The inclusive minimum bit index.
/// @param maxVal The exclusive maximum bit index.
/// @return A generator of individual set bit flags in the given range.
template<typename E>
requires std::is_enum_v<E>
std::generator<E> bitenum_ranged_iterator(E value, size_t minVal, size_t maxVal) {
    using T = std::underlying_type_t<E>;
    using U = std::make_unsigned_t<T>;

    constexpr size_t bitwidth = sizeof(U) * 8;
    const size_t begin = (minVal < bitwidth) ? minVal : bitwidth;
    const size_t end = (maxVal < bitwidth) ? maxVal : bitwidth;

    const U raw = static_cast<U>(value);
    for (size_t bit_index = begin; bit_index < end; ++bit_index) {
        const U bit = static_cast<U>(U{1} << bit_index);
        if ((raw & bit) != 0) {
            co_yield static_cast<E>(static_cast<T>(bit));
        }
    }
}

// enum_bitwiden_range was a pre-migration legacy implementation.
// It is intentionally removed in favor of generator-based bitenum_iterator.


// For your bit flags.

// However, some operators such as |=, &=, ^= are not provided by
// the macro. Just use something like e = (e | FlagA) to get the same
// effect.
// This is one of the things that you want to be perfectly implicit,
// and make it easy while also safe.

#define scl2_enum_bitop(NAME) \
    inline constexpr NAME operator|(NAME a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(static_cast<T>(a) | static_cast<T>(b)); \
    } \
    inline constexpr NAME operator^(NAME a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(static_cast<T>(a) ^ static_cast<T>(b)); \
    } \
    inline constexpr NAME operator&(NAME a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(static_cast<T>(a) & static_cast<T>(b)); \
    } \
    inline constexpr NAME operator~(NAME a) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(~static_cast<T>(a)); \
    }

#define scl2_enum_bitop_inclass(NAME) \
    friend inline constexpr NAME operator|(NAME a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(static_cast<T>(a) | static_cast<T>(b)); \
    } \
    friend inline constexpr NAME operator^(NAME a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(static_cast<T>(a) ^ static_cast<T>(b)); \
    } \
    friend inline constexpr NAME operator&(NAME a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(static_cast<T>(a) & static_cast<T>(b)); \
    } \
    friend inline constexpr NAME operator~(NAME a) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(~static_cast<T>(a)); \
    }

} // namespace scl2