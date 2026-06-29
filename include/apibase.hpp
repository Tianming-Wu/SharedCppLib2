#pragma once

#include <concepts>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "version.hpp"

// ─── Buffer size helper ────────────────────────────────────────────────

template<typename T>
constexpr size_t generic_buffer_size() {
    if constexpr (requires { { T::block_size } -> std::convertible_to<size_t>; }) {
        return static_cast<size_t>(T::block_size);
    }
    return static_cast<size_t>(4096);
}

// Forward declaration
// For api to work, actual header file must be included.
namespace std {
    class bytearray;
    class bytearray_view;
};