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

// ─── Static encryption: T::encrypt(data, key) ────────────────────────
template<typename T, typename key_type = __get_key_type<T>>
concept has_static_encryption = requires {
    requires std::is_class_v<T> && requires {
        { T::encrypt(std::declval<const std::bytearray&>(), std::declval<const key_type&>()) } -> std::same_as<std::bytearray>;
    };
};

template<typename T, typename key_type = __get_key_type<T>>
concept has_static_decryption = requires {
    requires std::is_class_v<T> && requires {
        { T::decrypt(std::declval<const std::bytearray&>(), std::declval<const key_type&>()) } -> std::same_as<std::bytearray>;
    };
};

// ─── Instance encryption: obj.encrypt(data) ─────────────────────────
template<typename T, typename key_type = __get_key_type<T>>
concept has_instance_encryption = requires {
    requires std::is_class_v<T> && requires {
        { std::declval<const T>().encrypt(std::declval<const std::bytearray&>()) } -> std::same_as<std::bytearray>;
    };
};

template<typename T, typename key_type = __get_key_type<T>>
concept has_instance_decryption = requires {
    requires std::is_class_v<T> && requires {
        { std::declval<const T>().decrypt(std::declval<const std::bytearray&>()) } -> std::same_as<std::bytearray>;
    };
};

template<typename T, typename key_type = __get_key_type<T>>
concept has_encryption_support = has_static_encryption<T> || has_instance_encryption<T>;

template<typename T, typename key_type = __get_key_type<T>>
concept has_decryption_support = has_static_decryption<T> || has_instance_decryption<T>;

// ─── Optional fixed key size ────────────────────────────────────────
// If a cipher exposes `static constexpr size_t key_size`, it can be queried.
// If absent, key size is treated as dynamic/unknown (e.g., xor_op accepts any key).
template<typename T>
concept has_fixed_key_size = requires {
    requires std::is_class_v<T> && requires {
        { T::key_size } -> std::convertible_to<size_t>;
    };
};

template<typename T>
requires has_encryption_support<T> || has_decryption_support<T>
constexpr size_t generic_key_size() {
    if constexpr (has_fixed_key_size<T>) {
        return static_cast<size_t>(T::key_size);
    } else {
        return 0; // 0 == dynamic / unknown
    }
}


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