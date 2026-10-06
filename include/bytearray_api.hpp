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
T generic_load(const scl2::bytearray& data) {
    if constexpr (__has_generic_load_memberfx<T>) { T v; v.load(data); return v; }
    else return T::load(data);
}

// ── gdump ─────────────────────────────────────────────────────────────

template<typename T>
requires (std::is_trivially_copyable_v<T> && !::scl2::has_generic_dump<T>)
scl2::bytearray gdump(const T& value) { return scl2::bytearray::fromTrivialType(value); }

template<typename T>
requires ::scl2::has_generic_dump<T>
scl2::bytearray gdump(const T& value) { return generic_dump(value); }

// ── gload ─────────────────────────────────────────────────────────────

template<typename _T>
requires (::scl2::trivially_copyable<_T> && !::scl2::has_generic_load<_T>)
_T gload(const scl2::bytearray& data) { return data.to<_T>(); }

template<typename T>
requires ::scl2::has_generic_load<T>
T gload(const scl2::bytearray& data) { return generic_load<T>(data); }

// ── Container ─────────────────────────────────────────────────────────

// Element by element: count, then each element written with gdump().
template<typename T>
requires has_gdump_container<T> && (!::scl2::has_gdump<T>)
scl2::bytearray gdump(const T& c) { scl2::bytearray ba; ba.appendContainer(c); return ba; }

template<typename T>
requires has_gload_container<T> && (!::scl2::has_gload<T>)
T gload(const scl2::bytearray& data) { return data.readContainer<T>(); }

// One plain-copy block: count, element size, then the elements. A trivially copyable object
// stays on the raw-object path above.
template<typename T>
requires ::scl2::stl::restorable_trivially_copyable_container<T>
      && (!std::is_trivially_copyable_v<T>) && (!::scl2::has_gdump<T>)
scl2::bytearray gdump(const T& c) { scl2::bytearray ba; ba.insertContainer(0, c); return ba; }

template<typename T>
requires ::scl2::stl::restorable_trivially_copyable_container<T>
      && (!std::is_trivially_copyable_v<T>) && (!::scl2::has_gload<T>)
T gload(const scl2::bytearray& data) { return data.readContainer<T>(); }

// ── std::string / std::wstring ────────────────────────────────────────
// The whole value with a length prefix (fromString / toString), which is also the bytes a
// string gets when it sits inside a pair or a container.

scl2::bytearray gdump(const std::string& str) { return scl2::bytearray::fromString(str); }
scl2::bytearray gdump(const std::wstring& str) { return scl2::bytearray::fromWString(str); }

template<typename T>
requires std::same_as<T, std::string>
T gload(const scl2::bytearray& data) { return data.toString(); }

template<typename T>
requires std::same_as<T, std::wstring>
T gload(const scl2::bytearray& data) { return data.toWString(); }

// ── One element from the read cursor ──────────────────────────────────

template<typename T>
requires gdp_detail::cursor_readable<T>::value
T read_element(const scl2::bytearray& data) {
    using B = std::remove_cv_t<T>;
    if constexpr (gdp_detail::is_string_type<B>::value) {
        if constexpr (std::same_as<B, std::wstring>) return data.readWString();
        else return data.readString();
    } else if constexpr (std::is_trivially_copyable_v<B> || ::scl2::has_generic_load<B>) {
        return data.read<B>();
    } else if constexpr (::scl2::stl::is_pair<B>) {
        return gload<B>(data); // reads its two members from the cursor
    } else {
        return data.readContainer<B>();
    }
}

// ── Pair ──────────────────────────────────────────────────────────────

template<typename T>
requires ::scl2::stl::is_pair<T> && (!::scl2::has_gdump<T>) && gdp_detail::value_writable<T>
scl2::bytearray gdump(const T& p) {
    scl2::bytearray ba;
    ba.append(scl2::gdump(p.first));
    ba.append(scl2::gdump(p.second));
    return ba;
}

template<typename T>
requires ::scl2::stl::is_pair<T> && (!::scl2::has_gload<T>) && gdp_detail::value_readable<T>
T gload(const scl2::bytearray& data) {
    using F = std::remove_const_t<typename T::first_type>;
    using S = typename T::second_type;
    return T{ ::scl2::read_element<F>(data), ::scl2::read_element<S>(data) };
}

} // namespace scl2