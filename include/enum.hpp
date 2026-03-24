/*
    Enum module for SharedCppLib2.

    Enum: Normal enum type



    BitEnum: Enum type that is designed to be used as bit flags. It
             provides bitwise operators and some helper functions.


*/

#pragma once

#include <type_traits>

namespace scl2 {

template<typename E>
requires std::is_enum_v<E>
struct bitenum_traits {
    using underlying_type = std::underlying_type_t<E>;
    static constexpr size_t bitwidth = sizeof(underlying_type) * 8;
};




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