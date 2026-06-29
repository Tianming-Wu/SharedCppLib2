#pragma once
#include "apibase.hpp"
#include <istream>
#include <ostream>

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


template<typename T, typename key_type = __get_key_type<T>>
requires has_encryption_support<T>
std::bytearray generic_encrypt(const std::bytearray& data, const key_type& key) {
    if constexpr (has_static_encryption<T>) {
        return T::encrypt(data, key);
    } else {
        return T(key).encrypt(data);
    }
}

template<typename T, typename key_type = __get_key_type<T>>
requires has_decryption_support<T>
std::bytearray generic_decrypt(const std::bytearray& data, const key_type& key) {
    if constexpr (has_static_decryption<T>) {
        return T::decrypt(data, key);
    } else {
        return T(key).decrypt(data);
    }
}

#define scl2_check_encryption_support(T) static_assert(::scl2::has_encryption_support<T>, "Type " #T " does not support encryption");
#define scl2_check_decryption_support(T) static_assert(::scl2::has_decryption_support<T>, "Type " #T " does not support decryption");



/// @brief Direction for streamed cipher operations.
enum class cipher_dir : bool { Encrypt = true, Decrypt = false };

// ─── Streaming encryption concepts (instance-based) ───────────────────

template<typename T>
concept has_streamed_encryption = requires {
    requires std::is_class_v<T>
    && requires {
        typename T::stream_type;
        requires std::is_class_v<typename T::stream_type>
        && requires(typename T::stream_type h) {
            { typename T::stream_type(std::declval<const typename T::key_type&>(), cipher_dir::Encrypt) };
            h.update(std::declval<const std::bytearray&>());
            { h.end() } -> std::same_as<std::bytearray>;
        };
    };
};

#define scl2_check_streamed_encryption(T) static_assert(::scl2::has_streamed_encryption<T>, "Type " #T " does not support streamed encryption");

// ─── Streaming helper functions ───────────────────────────────────────

template<typename T>
requires has_streamed_encryption<T>
void encrypt_stream(std::istream& input, std::ostream& output,
                    const typename T::key_type& key) {
    constexpr size_t bufsz = generic_buffer_size<T>();
    typename T::stream_type cipher(key, cipher_dir::Encrypt);
    std::bytearray buffer(bufsz);

    while (input) {
        buffer.resize(bufsz);
        input.read(reinterpret_cast<char*>(buffer.data()), bufsz);
        buffer.resize(static_cast<size_t>(input.gcount()));
        auto out = cipher.update(buffer);
        if (!out.empty())
            output.write(reinterpret_cast<const char*>(out.data()), out.size());
    }
    auto last = cipher.end();
    if (!last.empty())
        output.write(reinterpret_cast<const char*>(last.data()), last.size());
}

template<typename T>
requires has_streamed_encryption<T>
void decrypt_stream(std::istream& input, std::ostream& output,
                    const typename T::key_type& key) {
    constexpr size_t bufsz = generic_buffer_size<T>();
    typename T::stream_type cipher(key, cipher_dir::Decrypt);
    std::bytearray buffer(bufsz);

    while (input) {
        buffer.resize(bufsz);
        input.read(reinterpret_cast<char*>(buffer.data()), bufsz);
        buffer.resize(static_cast<size_t>(input.gcount()));
        auto out = cipher.update(buffer);
        if (!out.empty())
            output.write(reinterpret_cast<const char*>(out.data()), out.size());
    }
    auto last = cipher.end();
    if (!last.empty())
        output.write(reinterpret_cast<const char*>(last.data()), last.size());
}

} // namespace scl2