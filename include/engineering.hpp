/*
    Engineering literal operators for intergral and floating-point types

    These are for simplifying the usage of engineering notation in code,
    especially for sizes and data rates. For example, you can write 10_Mi instead of 10485760 for 10 mebibytes.
*/

#pragma once

#include <cstddef>
#include <type_traits>

// fix gcc requiring unsigned long long as literal input
typedef unsigned long long eng_t;

inline constexpr size_t operator"" _Ki(eng_t in) { return in << 10; }
inline constexpr size_t operator"" _Mi(eng_t in) { return in << 20; }
inline constexpr size_t operator"" _Gi(eng_t in) { return in << 30; }
inline constexpr size_t operator"" _Ti(eng_t in) { return in << 40; }
inline constexpr size_t operator"" _Pi(eng_t in) { return in << 50; }
inline constexpr size_t operator"" _Ei(eng_t in) { return in << 60; }
// higher version are not supported, since eng_t is usually 64-bit at most

inline constexpr size_t operator"" _K(eng_t in) { return in * 1000ULL; }
inline constexpr size_t operator"" _M(eng_t in) { return in * 1000000ULL; }
inline constexpr size_t operator"" _G(eng_t in) { return in * 1000000000ULL; }
inline constexpr size_t operator"" _T(eng_t in) { return in * 1000000000000ULL; }
inline constexpr size_t operator"" _P(eng_t in) { return in * 1000000000000000ULL; }
inline constexpr size_t operator"" _E(eng_t in) { return in * 1000000000000000000ULL; }
// higher version are not supported, since eng_t is usually 64-bit at most
