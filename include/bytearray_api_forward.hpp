#pragma once
#include "apibase.hpp"

#include "basic_api.hpp"

namespace scl2 {

// Dump/load user-defined layer.

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
    { v.load(std::declval<const std::bytearray_view&>()) } -> std::same_as<void>;
};

// For `static T load(const std::bytearray&)`
template<typename T>
concept __has_generic_static_load_memberfx = requires {
    { T::load(std::declval<const std::bytearray_view&>()) } -> std::same_as<T>;
};


template<typename T>
concept has_generic_dump = __has_generic_dump_memberfx<T> || __has_generic_static_dump_memberfx<T>;

template<typename T>
concept has_generic_load = __has_generic_load_memberfx<T> || __has_generic_static_load_memberfx<T>;


#define scl2_check_generic_dump(T) static_assert(::scl2::has_generic_dump<T>, "Type " #T " does not support generic dumping");
#define scl2_check_generic_load(T) static_assert(::scl2::has_generic_load<T>, "Type " #T " does not support generic loading");
#define scl2_check_generic_dump_load(T) \
    static_assert(::scl2::has_generic_dump<T>, "Type " #T " does not support generic dumping"); \
    static_assert(::scl2::has_generic_load<T>, "Type " #T " does not support generic loading");


template<typename T>
std::bytearray generic_dump(const T& value) {
    if constexpr (__has_generic_dump_memberfx<T>) {
        return value.dump();
    } else if constexpr (__has_generic_static_dump_memberfx<T>) {
        return T::dump(value);
    }
}

template<typename T>
T generic_load(const std::bytearray_view& data) {
    if constexpr (__has_generic_load_memberfx<T>) {
        T value;
        value.load(data);
        return value;
    } else if constexpr (__has_generic_static_load_memberfx<T>) {
        return T::load(data);
    }
}


// Autodetection layer
// this layer is named "gdump" and "gload". The difference with dump/load is that it automatically supports the "trivially copyable" types.

template<typename T>
concept has_gdump = ::scl2::has_generic_dump<T> || std::is_trivially_copyable_v<T>;

template<typename T>
requires (std::is_trivially_copyable_v<T> && !::scl2::has_generic_dump<T>)
std::bytearray gdump(const T& value) {
    return std::bytearray(value);
}

template<typename T>
requires ::scl2::has_generic_dump<T> /* && (!std::is_trivially_copyable_v<T>) */
std::bytearray gdump(const T& value) {
    return generic_dump(value);
}

// If both are present, will use the "generic_dump" because it is user-defined.
// Should be handeled by SFINAE.
// template<typename T>
// requires std::is_trivially_copyable_v<T> && (::scl2::has_generic_dump<T>)
// std::bytearray gdump(const T& value) {
//     return generic_dump(value);
// }

// If both are not present, will fail (not supported), which is expected.


template<typename T>
concept has_gload = ::scl2::has_generic_load<T> || std::is_trivially_copyable_v<T>;


// I don't like the idea of keeping the bytearray version of this function.
// This api is not designed to behave like it can read something mid-way.
template<typename _T>
requires (::scl2::trivially_copyable<_T> && !::scl2::has_generic_load<_T>)
_T gload(const std::bytearray& data);

template<typename T>
requires ::scl2::has_generic_load<T> /* && (!::scl2::trivially_copyable<T>) */
T gload(const std::bytearray& data) {
    return generic_load<T>(data);
}

// If both are present, will use the "generic_load" because it is user-defined.
// template<typename T>
// requires ::scl2::trivially_copyable<T> && (::scl2::has_generic_load<T>)
// T gload(const std::bytearray& data) {
//     return generic_load<T>(data);
// }



// nested container support
// This is a very powerful feature that allows you to directly dump/load containers of supported types,
// and even nested containers like std::map<std::string, std::vector<int>>.
// And decode it only in one line.

namespace gdp_detail {
    // 前向声明，用于递归
    template <typename T>
    struct has_gdump_recursive;

    template <typename T>
    struct has_gload_recursive;

    // 基础定义：判断 T 是否为容器且其元素是否满足 dump/load 约束
    template <typename T>
    struct has_gdump_recursive {
        static constexpr bool value = requires {
            typename T::value_type;
        } && (::scl2::has_gdump<typename T::value_type> || 
              has_gdump_recursive<typename T::value_type>::value);
    };

    template <typename T>
    struct has_gload_recursive {
        static constexpr bool value = requires {
            typename T::value_type;
        } && (::scl2::has_gload<typename T::value_type> || 
              has_gload_recursive<typename T::value_type>::value);
    };
}

// template<typename T>
// concept has_gdump_container = requires(const T& v) {
//     typename T::value_type;
//     requires ::scl2::has_gdump<typename T::value_type> || ::scl2::has_gdump_container<typename T::value_type>;
// };

// template<typename T>
// concept has_gload_container = requires(T& v) {
//     typename T::value_type;
//     requires ::scl2::has_gload<typename T::value_type> || ::scl2::has_gload_container<typename T::value_type>;
// };

template<typename T>
concept has_gdump_container = gdp_detail::has_gdump_recursive<T>::value;

// 替换原有的 has_gload_container
template<typename T>
concept has_gload_container = gdp_detail::has_gload_recursive<T>::value;


template<typename T>
requires has_gdump_container<T> && (!::scl2::has_gdump<T>)
std::bytearray gdump(const T& container) {
    std::bytearray ba;
    ba.appendContainer(container);
    return ba;
}

template<typename T>
requires has_gload_container<T> && (!::scl2::has_gload<T>)
T gload(const std::bytearray_view& data);


// pair support. This is also very useful for working with maps.
template<::scl2::stl::is_pair T>
std::bytearray gdump(const T& pair) {
    std::bytearray ba;
    ba.append(pair.first);
    ba.append(pair.second);
    return ba;
}


template<::scl2::stl::is_pair T>
T gload(const std::bytearray_view& data) {
    using First = std::remove_const_t<typename T::first_type>;
    using Second = typename T::second_type;
    
    First f = gload<First>(data);
    Second s = gload<Second>(data);
    return T{f, s};
}

} // namespace scl2