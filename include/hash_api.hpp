#pragma once
#include "apibase.hpp"

/// Hashing API

// Optional fixed digest-size metadata.
// If a hash provider exposes `static constexpr size_t result_size`,
// API can use it for stricter validation.
// If absent, digest size is treated as dynamic/unknown.

namespace scl2 {

template<typename T>
concept __has_hash_result_size = requires {
    requires std::is_class_v<T>
    && requires {
        { T::result_size } -> std::convertible_to<size_t>;
    };
};

// For classes that has static member function `static std::bytearray hash(const std::bytearray& data);`
template<typename T>
concept __has_hashing_support = requires {
    requires std::is_class_v<T>
    && requires {
        { T::hash(std::declval<const std::bytearray&>()) } -> std::same_as<std::bytearray>;
    };
};

template<typename T>
concept has_hashing_support = __has_hashing_support<T>;

template<typename T>
concept has_fixed_hash_result_size = has_hashing_support<T> && __has_hash_result_size<T>;

template<typename T>
requires has_hashing_support<T>
std::bytearray generic_hash(const std::bytearray& data) {
    return T::hash(data);
}

template<typename T>
requires has_hashing_support<T>
constexpr size_t generic_hash_result_size() {
    if constexpr (has_fixed_hash_result_size<T>) {
        return static_cast<size_t>(T::result_size);
    } else {
        return static_cast<size_t>(0); // 0 == dynamic / unknown
    }
}

#define scl2_check_hashing_support(T) static_assert(::scl2::has_hashing_support<T>, "Type " #T " does not support hashing");

} // namespace scl2