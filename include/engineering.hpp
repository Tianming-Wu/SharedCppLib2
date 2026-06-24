/*
    Engineering literal operators for intergral and floating-point types

    These are for simplifying the usage of engineering notation in code,
    especially for sizes and data rates. For example, you can write 10_Mi instead of 10485760 for 10 mebibytes.
*/

#pragma once

#include <type_traits>

inline constexpr size_t operator"" _Ki(size_t in) { return in << 10; }
inline constexpr size_t operator"" _Mi(size_t in) { return in << 20; }
inline constexpr size_t operator"" _Gi(size_t in) { return in << 30; }
inline constexpr size_t operator"" _Ti(size_t in) { return in << 40; }
inline constexpr size_t operator"" _Pi(size_t in) { return in << 50; }
inline constexpr size_t operator"" _Ei(size_t in) { return in << 60; }
// higher version are not supported, since size_t is usually 64-bit at most

inline constexpr size_t operator"" _K(size_t in) { return in * 1000ULL; }
inline constexpr size_t operator"" _M(size_t in) { return in * 1000000ULL; }
inline constexpr size_t operator"" _G(size_t in) { return in * 1000000000ULL; }
inline constexpr size_t operator"" _T(size_t in) { return in * 1000000000000ULL; }
inline constexpr size_t operator"" _P(size_t in) { return in * 1000000000000000ULL; }
inline constexpr size_t operator"" _E(size_t in) { return in * 1000000000000000000ULL; }
// higher version are not supported, since size_t is usually 64-bit at most
