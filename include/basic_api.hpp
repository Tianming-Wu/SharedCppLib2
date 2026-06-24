#pragma once
#include "apibase.hpp"

/*
    This file contains some basic implementation that should not belong to any module.
*/

namespace scl2 {

template<typename T>
concept trivially_copyable = std::is_trivially_copyable_v<T>;

template<typename T> // T is the iterator class itself for this, not the container.
concept is_iterator_compatible = requires(T a) {
    { *a } -> std::convertible_to<typename T::value_type>;
    { ++a } -> std::same_as<T&>;
    { a != a } -> std::convertible_to<bool>;
};

// use std::ranges::range<T> instead.

// template<typename T>
// concept is_container_range_loop_compatible = requires(T a) {
//     typename T::value_type;
//     { a.begin() } -> std::input_iterator;
//     { a.end() } -> std::input_iterator;
// };

/*
    API support for STL intergration.
    This is not STL-only, it also supports any container that has the necessary members.
    For the members to use with API, I just choose the mature STL design, so I don't need to design one,
    and also being compatible for almost all std namespace containers out-of-the-box.
*/
namespace stl {

// This only checks the structure of the container, to make sure all the keys necessary for the use are present.
template<typename T>
concept is_container_data_compatible = requires(T a) {
    false;
};


// This is a very basic check.
template<typename T>
concept is_container = requires(T a) {
    typename T::value_type;
    std::begin(a); // range-based for loop compatible
    std::end(a); // note that this is not actually used, it is only used to identify a container.
};


// Check if the container's value type is trivially copyable.
// In the api support we need to know seperately if the container is trivially copyable or not,
// since I would not give up the benifit of single plain-copying for trivially copyable types.
template<typename T>
concept trivially_copyable_container = requires(T a) {
    typename T::value_type;
    requires std::is_trivially_copyable_v<typename T::value_type>;
};


template<typename T>
concept universal_insertable = requires(T c, typename T::value_type v) {
    { c.push_back(v) };
} || requires (T c, typename T::value_type v) {
    { c.insert(v) };
};

template<typename T>
void universal_insert(T& container, typename T::value_type&& value) {
    if constexpr (requires(T c, typename T::value_type v) { c.push_back(v); }) {
        container.push_back(std::forward<typename T::value_type>(value));
    } else if constexpr (requires (T c, typename T::value_type v) { c.insert(v); }) {
        container.insert(std::forward<typename T::value_type>(value));
    } else {
        static_assert(false, "Type does not support universal insertion");
    }
}

// std::pair support.
// This enable us to work with std::map and std::unordered_map out-of-the-box.

template<typename T>
concept is_pair = requires(T t) {
    typename T::first_type;
    typename T::second_type;
    { t.first } -> std::same_as<const typename T::first_type&>;
    { t.second } -> std::same_as<typename T::second_type&>;
};


} // namespace stl

} // namespace scl2