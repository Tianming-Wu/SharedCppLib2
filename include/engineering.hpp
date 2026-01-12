/*
    Engineering literal operators for intergral and floating-point types
*/

#pragma once

#include <type_traits>

inline constexpr size_t operator"" Ki(size_t in) { return in << 10; }
inline constexpr size_t operator"" Mi(size_t in) { return in << 20; }
inline constexpr size_t operator"" Gi(size_t in) { return in << 30; }
inline constexpr size_t operator"" Ti(size_t in) { return in << 40; }
inline constexpr size_t operator"" Pi(size_t in) { return in << 50; }
inline constexpr size_t operator"" Ei(size_t in) { return in << 60; }
// higher version are not supported, since size_t is usually 64-bit at most

inline constexpr size_t operator"" K(size_t in) { return in * 1000ULL; }
inline constexpr size_t operator"" M(size_t in) { return in * 1000000ULL; }
inline constexpr size_t operator"" G(size_t in) { return in * 1000000000ULL; }
inline constexpr size_t operator"" T(size_t in) { return in * 1000000000000ULL; }
inline constexpr size_t operator"" P(size_t in) { return in * 1000000000000000ULL; }
inline constexpr size_t operator"" E(size_t in) { return in * 1000000000000000000ULL; }
// higher version are not supported, since size_t is usually 64-bit at most
