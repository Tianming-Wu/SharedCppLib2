#pragma once
#include "apibase.hpp"

#include "basic_api.hpp"

namespace scl2 {

// Dump/load user-defined layer.

// For `scl2::bytearray dump() const`.
// Note: the probe calls dump() on a `const T&`, so a NON-const dump() is not
// detected at all — this is member const-qualification, which is a different
// rule from argument qualification conversion (a non-const object binding to a
// `const T&` parameter). A provider whose only dump() is non-const fails
// has_generic_dump entirely.
template<typename T>
concept __has_generic_dump_memberfx = requires(const T& v) {
    { v.dump() } -> std::same_as<scl2::bytearray>;
};

// For `static scl2::bytearray dump(const T&)`.
template<typename T>
concept __has_generic_static_dump_memberfx = requires {
    { T::dump(std::declval<const T&>()) } -> std::same_as<scl2::bytearray>;
};

// For `void load(const scl2::bytearray&)`.
// The probe passes a non-const lvalue, which binds to a `const bytearray&`
// parameter by qualification conversion — so either
// `load(const scl2::bytearray&)` (preferred; matches api.hpp) or
// `load(scl2::bytearray&)` satisfies this concept. The cursor is `mutable`
// and every reader is const, so the const form is fully functional.
template<typename T>
concept __has_generic_load_memberfx = requires(T& v) {
    { v.load(std::declval<scl2::bytearray&>()) } -> std::same_as<void>;
};

// For `static T load(const scl2::bytearray&)`. Same note as above.
template<typename T>
concept __has_generic_static_load_memberfx = requires {
    { T::load(std::declval<scl2::bytearray&>()) } -> std::same_as<T>;
};


template<typename T>
concept has_generic_dump = __has_generic_dump_memberfx<T> || __has_generic_static_dump_memberfx<T>;

template<typename T>
concept has_generic_load = __has_generic_load_memberfx<T> || __has_generic_static_load_memberfx<T>;

template<typename T>
concept has_generic_dump_load = has_generic_dump<T> && has_generic_load<T>;

#define scl2_check_generic_dump(T) static_assert(::scl2::has_generic_dump<T>, "Type " #T " does not support generic dumping");
#define scl2_check_generic_load(T) static_assert(::scl2::has_generic_load<T>, "Type " #T " does not support generic loading");
#define scl2_check_generic_dump_load(T) \
    static_assert(::scl2::has_generic_dump<T>, "Type " #T " does not support generic dumping"); \
    static_assert(::scl2::has_generic_load<T>, "Type " #T " does not support generic loading");

// ── Function declarations (implementations in bytearray_api.hpp) ─────
// These need scl2::bytearray to be a complete type, so they are defined
// in bytearray_api.hpp which includes bytearray.hpp first.

template<typename T>
scl2::bytearray generic_dump(const T& value);

template<typename T>
T generic_load(const scl2::bytearray& data);


// Autodetection layer

template<typename T>
concept has_gdump = ::scl2::has_generic_dump<T> || std::is_trivially_copyable_v<T>;

template<typename T>
requires (std::is_trivially_copyable_v<T> && !::scl2::has_generic_dump<T>)
scl2::bytearray gdump(const T& value);

template<typename T>
requires ::scl2::has_generic_dump<T>
scl2::bytearray gdump(const T& value);


template<typename T>
concept has_gload = ::scl2::has_generic_load<T> || std::is_trivially_copyable_v<T>;

template<typename _T>
requires (::scl2::trivially_copyable<_T> && !::scl2::has_generic_load<_T>)
_T gload(const scl2::bytearray& data);

template<typename T>
requires ::scl2::has_generic_load<T>
T gload(const scl2::bytearray& data);

// ── Nested container concepts ────────────────────────────────────────



// nested container support
// This is a very powerful feature that allows you to directly dump/load containers of supported types,
// and even nested containers like std::map<std::string, std::vector<int>>.
// And decode it only in one line.

namespace gdp_detail {
    // 基础定义：判断 T 是否为容器且其元素是否满足 dump/load 约束。
    //
    // 注意：必须用 void_t 做偏特化来探测 value_type，不能在同一个表达式里写
    // `requires { typename T::value_type; } && has_gload<typename T::value_type>`。
    // `&&` 在常量表达式里虽然是短路求值，但整个表达式仍然必须先良构，
    // 所以 `typename T::value_type` 对 int / char 这类没有 value_type 的类型
    // 会硬报错（C2825/C2039），而不是求值为 false。偏特化才是 SFINAE 友好的。
    template <typename T, typename = void>
    struct has_gdump_recursive : std::false_type {};

    template <typename T>
    struct has_gdump_recursive<T, std::void_t<typename T::value_type>>
        : std::bool_constant<::scl2::has_gdump<typename T::value_type>
                             || has_gdump_recursive<typename T::value_type>::value> {};

    template <typename T, typename = void>
    struct has_gload_recursive : std::false_type {};

    template <typename T>
    struct has_gload_recursive<T, std::void_t<typename T::value_type>>
        : std::bool_constant<::scl2::has_gload<typename T::value_type>
                             || has_gload_recursive<typename T::value_type>::value> {};
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
scl2::bytearray gdump(const T& container);

template<typename T>
requires has_gload_container<T> && (!::scl2::has_gload<T>)
T gload(const scl2::bytearray& data);


// pair support
template<::scl2::stl::is_pair T>
scl2::bytearray gdump(const T& pair);

template<::scl2::stl::is_pair T>
T gload(const scl2::bytearray& data);

} // namespace scl2