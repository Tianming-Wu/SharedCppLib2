#pragma once
#include "apibase.hpp"

namespace scl2 {


// Note: this is an internal support layer for encryption and hashing
// API, for a unified interface for encryption and hashing functions.

// Note it is not for classes to support encryption or hashing, but the
// encryption / hashing provider provide these apis so user can easily
// switch between different providers, also maybe implement their own
// provider by implementing these concepts.

template<typename T>
requires requires() { typename T::key_type; }
using __get_key_type = typename T::key_type;

// For classes that has static member function `static std::bytearray encrypt(const std::bytearray& data, const key_type& key);`
template<typename T, typename key_type = __get_key_type<T>>
concept __has_encryption_support = requires {
    requires std::is_class_v<T>
    && requires {
        { T::encrypt(std::declval<const std::bytearray&>(), std::declval<const key_type&>()) } -> std::same_as<std::bytearray>;
    };
};

// For classes that has static member function `static std::bytearray decrypt(const std::bytearray& data, const key_type& key);`
template<typename T, typename key_type = __get_key_type<T>>
concept __has_decryption_support = requires {
    requires std::is_class_v<T>
    && requires {
        { T::decrypt(std::declval<const std::bytearray&>(), std::declval<const key_type&>()) } -> std::same_as<std::bytearray>;
    };
};

template<typename T, typename key_type = __get_key_type<T>>
concept has_encryption_support = __has_encryption_support<T>;

template<typename T, typename key_type = __get_key_type<T>>
concept has_decryption_support = __has_decryption_support<T>;


#define scl2_check_encryption_support(T) static_assert(::scl2::has_encryption_support<T>, "Type " #T " does not support encryption");
#define scl2_check_decryption_support(T) static_assert(::scl2::has_decryption_support<T>, "Type " #T " does not support decryption");



// The below streamed encryption and decryption API definition is incomplete and is not usable.
// It might change in any future version. Do not use them.

// Streamed encryption api definition, which allows breaking large amount of data into chunks.:
// Is a class, and has these functions:
// - `static bool begin(std::bytearray& state, const key_type& key);`
// - `static bool update(std::bytearray& state, const std::bytearray& chunk);`
// - `static std::bytearray end(std::bytearray& state);`

template<typename T>
concept __has_streamed_encrpytion_begin = requires {
    requires std::is_class_v<T>
    && requires {
        { T::begin(std::declval<std::bytearray&>(), std::declval<const __get_key_type<T>&>()) } -> std::same_as<bool>;
    };
};

template<typename T>
concept __has_streamed_encryption_update = requires {
    requires std::is_class_v<T>
    && requires {
        { T::update(std::declval<std::bytearray&>(), std::declval<const std::bytearray&>()) } -> std::same_as<bool>;
    };
};

template<typename T>
concept __has_streamed_encryption_end = requires {
    requires std::is_class_v<T>
    && requires {
        { T::end(std::declval<std::bytearray&>()) } -> std::same_as<std::bytearray>;
    };
};

template<typename T>
concept has_streamed_encryption_support = requires {
    __has_streamed_encrpytion_begin<T> && __has_streamed_encryption_update<T> && __has_streamed_encryption_end<T>;
};

#define scl2_check_streamed_encryption_support(T) static_assert(::scl2::has_streamed_encryption_support<T>, "Type " #T " does not support streamed encryption");


// Streamed decryption api definition:
// Is a class, and has these functions:
// - `static bool begin(std::bytearray& state, const key_type& key);`
// - `static bool update(std::bytearray& state, const std::bytearray& chunk);`
// - `static std::bytearray end(std::bytearray& state);`

} // namespace scl2