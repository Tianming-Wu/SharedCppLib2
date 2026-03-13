/*
    This is the standard interface definition file for SharedCppLib2 library.

    For your compatible layer to work with SharedCppLib2:

    These are the "compatible layers" that if your type supports, SharedCppLib2
    can automatically recognize them and use them for simplicity and efficiency:  

    - Structure serialization and deserialization (string-type):
        These forms are supported for both:
            1. Member functions:
                void serialize() const;
                void deserialize() const;
            2. Static member functions:
                static std::string serialize(const T& value);
                static T deserialize(const std::string& data);
        For the library to recognize, you should have string_type member type
        defined in your class, and the serialize/deserialize functions should return/take that type.
        It's not actually limited to std::string, you can use std::wstring, u8string, or even a custom
        type that has nothing to do with string. It just need to be consistent.

    - Structure to bytearray conversion:
        Internal feature-id: directread/directwrite
        These forms are supported for both:
            1. Member functions:
                std::bytearray dump() const;
                void load(const std::bytearray& data);
            2. Static member functions:
                static std::bytearray dump(const T& value);
                static T load(const std::bytearray& data);
        These functions will be matched in order of member functions first,
        then static member functions, and the first match will be used.
        If dump() is supported, it will be automatically called for non-trivially
        copyable types when constructing to a bytearray.
        If load() is supported, it will be automatically called for non-trivially
        copyable types when converting from a bytearray.

    
    Note: read/write part of the compatible layer is seperated, meaning
        that you do not need to have the other unused one if you do not
        need that direction.

*/

#pragma once
#include <concepts>
#include <string>
#include "version.hpp"

// you need this macro to activate API support in other libraries
#ifndef SHAREDCPPLIB2_API_SUPPORT
    #define SHAREDCPPLIB2_API_SUPPORT
#endif

// Forward declaration
// For api to work, actual bytearray.hpp must be included.
namespace std {
    class bytearray;
};

namespace scl2 {


// ========================================================
// |             stringlist compatible layer              |
// ========================================================

// Auto match string type (type 1)
// For containers that has string_type member
template <typename T>
requires requires() { typename T::string_type; }
using __get_string_type = typename T::string_type;

// Auto match string type (type 2)
// For containers that has 
// template <typename T>
// 

template<typename T, typename Ret>
concept ret_string_type = requires {
    requires std::convertible_to<__get_string_type<T>, Ret>;
};

template<typename T>
concept void_or_bool = requires {
    requires std::same_as<T, void> || std::same_as<T, bool>;
};

// Match serialize function: `string_type serialize() const`;
template<typename T>
concept __has_generic_serialize_memberfx = requires(const T& v) {
    requires requires { { v.serialize() } -> ret_string_type<T>; }
    || requires { { v.serialise() } -> ret_string_type<T>; };
};

// Match static serialize function: string_type serialize(const T& value);
template<typename T>
concept __has_generic_static_serialize_memberfx = requires {
    requires requires { { T::serialize(std::declval<const T&>()) } -> ret_string_type<T>; }
    || requires { { T::serialise(std::declval<const T&>()) } -> ret_string_type<T>; };
};

// Match deserialize function: `void deserialize(const string_type& s)`
// or `bool deserialize(const string_type& s)` (for error handling)
template<typename T>
concept __has_generic_deserialize_memberfx = requires(T& v) {
    requires requires { { v.deserialize(std::declval<const __get_string_type<T>&>()) } -> void_or_bool; }
    || requires { { v.deserialise(std::declval<const __get_string_type<T>&>()) } -> void_or_bool; };
};

// Match static deserialize function: `T deserialize(const string_type& s)`
// Error handling not supported in api for this case.
template<typename T>
concept __has_generic_static_deserialize_memberfx = requires {
    requires requires { { T::deserialize(std::declval<const typename T::string_type&>()) } -> std::same_as<T>; }
    || requires { { T::deserialise(std::declval<const typename T::string_type&>()) } -> std::same_as<T>; };
};


// Middle-layer wrappers

template<typename T>
concept has_generic_serialize = __has_generic_serialize_memberfx<T> || __has_generic_static_serialize_memberfx<T>;

template<typename T>
concept has_generic_deserialize = __has_generic_deserialize_memberfx<T> || __has_generic_static_deserialize_memberfx<T>;


// Generic wrapper functions that will be used in API implementations.
// You can use them in your own code as well for convenience.

template<typename T, typename string_type = __get_string_type<T>>
requires has_generic_serialize<T>
string_type generic_serialize(const T& value) {
    if constexpr (__has_generic_serialize_memberfx<T>) {
        if constexpr (requires { value.serialize(std::declval<const __get_string_type<T>&>()); }) {
            return value.serialize();
        } else if constexpr (requires { value.serialise(std::declval<const __get_string_type<T>&>()); }) {
            return value.serialise();
        }
    } else if constexpr (__has_generic_static_serialize_memberfx<T>) {
        if constexpr (requires { T::serialize(std::declval<const T&>()); }) {
            return T::serialize(value);
        } else if constexpr (requires { T::serialise(std::declval<const T&>()); }) {
            return T::serialise(value);
        }
    }
}


// For deserialization, we cannot support error handling for static member functions
template<typename T, typename string_type = __get_string_type<T>>
requires has_generic_deserialize<T>
T generic_deserialize(const string_type& s) {
    if constexpr (__has_generic_deserialize_memberfx<T>) {
        T value;
        if constexpr (requires { value.deserialize(s); }) {
            if constexpr (std::same_as<decltype(value.deserialize(s)), bool>) {
                if (!value.deserialize(s)) {
                    throw std::runtime_error("Deserialization failed for " + std::string(typeid(T).name()));
                }
            } else {
                value.deserialize(s);
            }
        } else if constexpr (requires { value.deserialise(s); }) {
            if constexpr (std::same_as<decltype(value.deserialise(s)), bool>) {
                if (!value.deserialise(s)) {
                    throw std::runtime_error("Deserialization failed for " + std::string(typeid(T).name()));
                }
            } else {
                value.deserialise(s);
            }
        }
        return value;
    } else if constexpr (__has_generic_static_deserialize_memberfx<T>) {
        if constexpr (requires { T::deserialize(s); }) {
            return T::deserialize(s);
        } else if constexpr (requires { T::deserialise(s); }) {
            return T::deserialise(s);
        }
    }
}


// No-except version
template <typename T, typename string_type = __get_string_type<T>>
T generic_deserialize_noexcept(const string_type& s) noexcept {
    try {
        return generic_deserialize<T, string_type>(s);
    } catch (...) {
        if constexpr (std::is_default_constructible_v<T>) {
            return T{};
        } else {
            std::terminate();
        }
    }
}



// ========================================================
// |             bytearray compatible layer               |
// ========================================================

// For `std::bytearray dump() const`.
template<typename T>
concept __has_generic_dump_memberfx = requires(const T& v) {
    { v.dump() } -> std::same_as<std::bytearray>;
};

// For `static std::bytearray dump(const T&)`.
template<typename T>
concept __has_generic_static_dump_memberfx = requires {
    { T::dump(std::declval<const T&>()) } -> std::same_as<std::bytearray>;
};

// For `void load(const std::bytearray&)`
template<typename T>
concept __has_generic_load_memberfx = requires(T& v) {
    { v.load(std::declval<const std::bytearray&>()) } -> std::same_as<void>;
};

// For `static T load(const std::bytearray&)`
template<typename T>
concept __has_generic_static_load_memberfx = requires {
    { T::load(std::declval<const std::bytearray&>()) } -> std::same_as<T>;
};


template<typename T>
concept has_generic_dump = __has_generic_dump_memberfx<T> || __has_generic_static_dump_memberfx<T>;

template<typename T>
concept has_generic_load = __has_generic_load_memberfx<T> || __has_generic_static_load_memberfx<T>;


template<typename T>
std::bytearray generic_dump(const T& value) {
    if constexpr (__has_generic_dump_memberfx<T>) {
        return value.dump();
    } else if constexpr (__has_generic_static_dump_memberfx<T>) {
        return T::dump(value);
    }
}

template<typename T>
T generic_load(const std::bytearray& data) {
    if constexpr (__has_generic_load_memberfx<T>) {
        T value;
        value.load(data);
        return value;
    } else if constexpr (__has_generic_static_load_memberfx<T>) {
        return T::load(data);
    }
}



// ========================================================
// |             Encryption and hashing API               |
// ========================================================

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

// For namespaces that has function `std::bytearray encrypt(const std::bytearray& data, const key_type& key);`
template<typename T, typename key_type = __get_key_type<T>>
concept __has_encryption_support_namespace = requires {
    requires !std::is_class_v<T>
    && requires {
        { T::encrypt(std::declval<const std::bytearray&>(), std::declval<const key_type&>()) } -> std::same_as<std::bytearray>;
    };
};

// For namespaces that has function `std::bytearray decrypt(const std::bytearray& data, const key_type& key);`
template<typename T, typename key_type = __get_key_type<T>>
concept __has_decryption_support_namespace = requires {
    requires !std::is_class_v<T>
    && requires {
        { T::decrypt(std::declval<const std::bytearray&>(), std::declval<const key_type&>()) } -> std::same_as<std::bytearray>;
    };
};

template<typename T, typename key_type = __get_key_type<T>>
concept has_encryption_support = __has_encryption_support<T> || __has_encryption_support_namespace<T>;

template<typename T, typename key_type = __get_key_type<T>>
concept has_decryption_support = __has_decryption_support<T> || __has_decryption_support_namespace<T>;


///TODO: Design a streamed-encryption API, and add the support concept here as well.
// This is required for encrypting/decrypting large data.


// For classes that has static member function `static std::bytearray hash(const std::bytearray& data);`
template<typename T>
concept __has_hashing_support = requires {
    requires std::is_class_v<T>
    && requires {
        { T::hash(std::declval<const std::bytearray&>()) } -> std::same_as<std::bytearray>;
    };
};

// For namespaces that has function `std::bytearray hash(const std::bytearray& data);`
template<typename T>
concept __has_hashing_support_namespace = requires {
    requires !std::is_class_v<T>
    && requires {
        { T::hash(std::declval<const std::bytearray&>()) } -> std::same_as<std::bytearray>;
    };
};

template<typename T>
concept has_hashing_support = __has_hashing_support<T> || __has_hashing_support_namespace<T>;


// API versioning
// These aren't necessary for it to work,
// And isn't used in any module yet.

// Also, compatible cannot just be "same".
// We theoritically used "SameMajorVersion", but that
// is just not the actural case.

#define current_api_version static constexpr auto api_version = SHAREDCPPLIB2_VERSION;

template<typename T>
concept api_version_compatible = requires {
    std::is_class_v<T>
    && requires {
        { T::api_version } -> std::convertible_to<std::string>;
    }
    && requires {
        { std::string(T::api_version) == SHAREDCPPLIB2_VERSION };
    };
};

} // namespace scl2