/*
    This is the standard interface definition file for SharedCppLib2 library.
*/

#pragma once
#include <concepts>
#include <string>
#include "version.hpp"

// you need this macro to activate API support in other libraries
#ifndef SHAREDCPPLIB2_API_SUPPORT
    #define SHAREDCPPLIB2_API_SUPPORT
#endif

namespace scl2 {

template<typename T>
concept hasSerializeSupport = requires(typename T::string_type& s, const T& v) {
    requires requires { v.deserialize(s); } || requires { v.deserialise(s); };
} || requires(typename T::string_type& s) {
    requires requires { T::deserialize(s); } || requires { T::deserialise(s); };
};

template<typename T>
requires hasSerializeSupport<T>
T __deserialize_function(T& value) {
    using string_type = typename T::string_type;
    string_type s;
    if constexpr (requires { value.deserialize(s); }) {
        value.deserialize(s);
    } else if constexpr (requires { value.deserialise(s); }) {
        value.deserialise(s);
    } else if constexpr (requires { T::deserialize(s); }) {
        T::deserialize(s);
    } else if constexpr (requires { T::deserialise(s); }) {
        T::deserialise(s);
    }
    return value;
}

template<typename T>
requires hasSerializeSupport<T>
void __serialize_function(const T& value, typename T::string_type& s) {
    if constexpr (requires { value.serialize(s); }) {
        value.serialize(s);
    } else if constexpr (requires { value.serialise(s); }) {
        value.serialise(s);
    } else if constexpr (requires { T::serialize(s); }) {
        T::serialize(s);
    } else if constexpr (requires { T::serialise(s); }) {
        T::serialise(s);
    }
}

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