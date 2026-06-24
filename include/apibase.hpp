#pragma once

#include <concepts>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "version.hpp"

// Forward declaration
// For api to work, actual header file must be included.
namespace std {
    class bytearray;
    class bytearray_view;
};