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

    
    In short:
        Type -> serialize() -> string_type
        Type <- deserialize() <- string_type

        Type -> dump() -> bytearray
        Type <- load() <- bytearray

        Type -> save() -> file (not implemented yet)
        Type <- load() <- file

    
    Note: read/write part of the compatible layer is seperated, meaning
        that you do not need to have the other unused one if you do not
        need that direction.


    * This module is a compatibility layer.
*/

#pragma once
#include "apibase.hpp"

// you need this macro to activate API support in other libraries
#ifndef SHAREDCPPLIB2_API_SUPPORT
    #define SHAREDCPPLIB2_API_SUPPORT
#endif

#include "basic_api.hpp"
#include "bytearray_api.hpp"
#include "encryption_api.hpp"
#include "hash_api.hpp"

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

#define scl2_check_generic_serialize(T) static_assert(::scl2::has_generic_serialize<T>, "Type " #T " does not support generic serialization");
#define scl2_check_generic_deserialize(T) static_assert(::scl2::has_generic_deserialize<T>, "Type " #T " does not support generic deserialization");

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
// |           enum utilities compatible layer            |
// ========================================================

template<typename E>
concept is_enum_compatible = std::is_enum_v<E>;

template<typename E>
concept is_enum_ready = is_enum_compatible<E> && requires {
    false;
};


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