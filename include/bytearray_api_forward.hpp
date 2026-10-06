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



// ── What gdump() / gload() accept ─────────────────────────────────────
// Every overload is constrained by exactly the precondition of its body: a type that satisfies
// a constraint compiles, and a type whose body cannot handle it does not satisfy the
// constraint. Elements are what makes this recursive.
//
// The previous version only asked whether `value_type` was trivially copyable, so std::list,
// std::map, std::string and ordered_map all looked like "plain copyable blocks" while every
// branch then refused them inside the template.

namespace gdp_detail {

    // std::string / std::wstring: written and read whole, with a length prefix.
    template <typename T> struct is_string_type : std::false_type {};
    template <> struct is_string_type<std::string> : std::true_type {};
    template <> struct is_string_type<std::wstring> : std::true_type {};

    // Handled as one value by one of the paths above: the type's own dump()/load(), one raw
    // trivially copyable object, one string, or one plain-copy container block.
    template <typename T>
    struct has_own_writer : std::bool_constant<
        ::scl2::has_gdump<std::remove_cv_t<T>>
        || std::is_trivially_copyable_v<std::remove_cv_t<T>>
        || is_string_type<std::remove_cv_t<T>>::value
        || ::scl2::stl::restorable_trivially_copyable_container<std::remove_cv_t<T>>> {};

    template <typename T>
    struct has_own_reader : std::bool_constant<
        ::scl2::has_gload<std::remove_cv_t<T>>
        || std::is_trivially_copyable_v<std::remove_cv_t<T>>
        || is_string_type<std::remove_cv_t<T>>::value
        || ::scl2::stl::restorable_trivially_copyable_container<std::remove_cv_t<T>>> {};

    // A container the element-wise path walks: range-for and size(), and not one contiguous
    // plain-copy block (that one is written and read as a whole).
    //
    // value_type has to be probed through void_t partial specializations below: writing
    // `requires { typename T::value_type; } && has_gdump<typename T::value_type>` in one
    // expression is a hard error for a type without value_type, not a false.
    template <typename T>
    concept elementwise_writer = ::scl2::stl::is_container<T>
        && (!::scl2::stl::trivially_copyable_container<T>)
        && requires(const T& c) { c.size(); };

    // ...and what reading it back needs: default constructible, insertable, and every element
    // readable from the cursor (cursor_readable).
    template <typename T>
    concept elementwise_reader = elementwise_writer<T>
        && std::default_initializable<T>
        && ::scl2::stl::universal_insertable<T>;

    template <typename T, typename = void> struct writable : std::false_type {};
    template <typename T, typename = void> struct readable : std::false_type {};
    template <typename T, typename = void> struct writable_pair : std::false_type {};
    template <typename T, typename = void> struct readable_pair : std::false_type {};

    // One value of T can be written / read: one of the paths above, a pair of those, or a
    // container of those (that is the recursion).
    template <typename T>
    concept value_writable = has_own_writer<T>::value
        || writable<T>::value || writable_pair<T>::value;

    template <typename T>
    concept value_readable = has_own_reader<T>::value
        || readable<T>::value || readable_pair<T>::value;

    // What read_element<T>() needs: bytearray::read<T>() (raw, or the type's own load()), a
    // string, a pair, or a nested container read with readContainer<T>().
    template <typename T, typename = void>
    struct cursor_readable
        : std::bool_constant<has_own_reader<T>::value || readable_pair<T>::value> {};

    template <typename T>
    struct cursor_readable<T, std::void_t<typename T::value_type>>
        : std::bool_constant<has_own_reader<T>::value || readable_pair<T>::value
            || (elementwise_reader<T> && cursor_readable<typename T::value_type>::value)> {};

    template <typename T>
    struct writable<T, std::void_t<typename T::value_type>>
        : std::bool_constant<elementwise_writer<T> && value_writable<typename T::value_type>> {};

    template <typename T>
    struct readable<T, std::void_t<typename T::value_type>>
        : std::bool_constant<elementwise_reader<T>
            && cursor_readable<typename T::value_type>::value> {};

    // std::map's value_type shape: first is const, which is what is_pair() checks.
    template <typename T>
    struct writable_pair<T, std::void_t<typename T::first_type, typename T::second_type>>
        : std::bool_constant<::scl2::stl::is_pair<T>
            && value_writable<typename T::first_type>
            && value_writable<typename T::second_type>> {};

    template <typename T>
    struct readable_pair<T, std::void_t<typename T::first_type, typename T::second_type>>
        : std::bool_constant<::scl2::stl::is_pair<T>
            && cursor_readable<typename T::first_type>::value
            && cursor_readable<typename T::second_type>::value> {};

} // namespace gdp_detail

// A container gdump() / gload() handle themselves, element by element.
template<typename T>
concept has_gdump_container = gdp_detail::writable<T>::value;

template<typename T>
concept has_gload_container = gdp_detail::readable<T>::value;


template<typename T>
requires has_gdump_container<T> && (!::scl2::has_gdump<T>)
scl2::bytearray gdump(const T& container);

template<typename T>
requires has_gload_container<T> && (!::scl2::has_gload<T>)
T gload(const scl2::bytearray& data);


// A contiguous container of trivially copyable elements: count, element size, then the whole
// block at once. A trivially copyable object is still written as itself, so it stays on the
// path above.
template<typename T>
requires ::scl2::stl::restorable_trivially_copyable_container<T>
      && (!std::is_trivially_copyable_v<T>) && (!::scl2::has_gdump<T>)
scl2::bytearray gdump(const T& container);

template<typename T>
requires ::scl2::stl::restorable_trivially_copyable_container<T>
      && (!std::is_trivially_copyable_v<T>) && (!::scl2::has_gload<T>)
T gload(const scl2::bytearray& data);


// std::string / std::wstring, length prefixed.
scl2::bytearray gdump(const std::string& str);
scl2::bytearray gdump(const std::wstring& str);

template<typename T>
requires std::same_as<T, std::string>
T gload(const scl2::bytearray& data);

template<typename T>
requires std::same_as<T, std::wstring>
T gload(const scl2::bytearray& data);


// One element from the read cursor — the counterpart of the append(gdump(element)) that
// bytearray::appendContainer() writes, for containers that are not one plain-copy block.
template<typename T>
requires gdp_detail::cursor_readable<T>::value
T read_element(const scl2::bytearray& data);


// pair support
template<typename T>
requires ::scl2::stl::is_pair<T> && (!::scl2::has_gdump<T>) && gdp_detail::value_writable<T>
scl2::bytearray gdump(const T& pair);

template<typename T>
requires ::scl2::stl::is_pair<T> && (!::scl2::has_gload<T>) && gdp_detail::value_readable<T>
T gload(const scl2::bytearray& data);

} // namespace scl2