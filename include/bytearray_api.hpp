#pragma once

#include "bytearray_api_forward.hpp"
#include "bytearray.hpp"

namespace scl2 {

template<typename _T>
requires (::scl2::trivially_copyable<_T> && !::scl2::has_generic_load<_T>)
_T gload(const std::bytearray& data) {
    return data.convert_to<_T>();
}

template<typename T>
requires has_gload_container<T> && (!::scl2::has_gload<T>)
T gload(const std::bytearray_view& data) {
    return data.readContainer<T>();
}

} // namespace scl2