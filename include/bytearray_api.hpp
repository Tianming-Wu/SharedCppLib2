#pragma once

#include "bytearray_api_forward.hpp"
#include "bytearray.hpp"

namespace scl2 {

// ── generic_dump / generic_load ────────────────────────────────────────

template<typename T>
scl2::bytearray generic_dump(const T& value) {
    if constexpr (__has_generic_dump_memberfx<T>) return value.dump();
    else return T::dump(value);
}

template<typename T>
T generic_load(scl2::bytearray& data) {
    if constexpr (__has_generic_load_memberfx<T>) { T v; v.load(data); return v; }
    else return T::load(data);
}

// ── gdump ─────────────────────────────────────────────────────────────

template<typename T>
requires (std::is_trivially_copyable_v<T> && !::scl2::has_generic_dump<T>)
scl2::bytearray gdump(const T& value) { return scl2::bytearray(value); }

template<typename T>
requires ::scl2::has_generic_dump<T>
scl2::bytearray gdump(const T& value) { return generic_dump(value); }

// ── gload ─────────────────────────────────────────────────────────────

template<typename _T>
requires (::scl2::trivially_copyable<_T> && !::scl2::has_generic_load<_T>)
_T gload(const scl2::bytearray& data) { return data.to<_T>(); }

template<typename T>
requires ::scl2::has_generic_load<T>
T gload(scl2::bytearray& data) { return generic_load<T>(data); }

// ── Container ─────────────────────────────────────────────────────────

template<typename T>
requires has_gdump_container<T> && (!::scl2::has_gdump<T>)
scl2::bytearray gdump(const T& c) { scl2::bytearray ba; ba.appendContainer(c); return ba; }

template<typename T>
requires has_gload_container<T> && (!::scl2::has_gload<T>)
T gload(scl2::bytearray& data) { return data.readContainer<T>(); }

// ── Pair ──────────────────────────────────────────────────────────────

template<::scl2::stl::is_pair T>
scl2::bytearray gdump(const T& p) { scl2::bytearray ba; ba.append(p.first); ba.append(p.second); return ba; }

template<::scl2::stl::is_pair T>
T gload(scl2::bytearray& data) {
    using F = std::remove_const_t<typename T::first_type>;
    using S = typename T::second_type;
    return T{gload<F>(data), gload<S>(data)};
}

} // namespace scl2