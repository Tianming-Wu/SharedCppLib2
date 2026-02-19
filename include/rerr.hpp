/*
    Rust-like error handling by return for C++23.

    Header-only.

    But of course without memory management in Rust.
    Can still use C++'s object-based memory management though.

    STL, why not? This whole project is heavily based on STL.
*/

#pragma once

#include <tuple>
#include <string>
#include <stdexcept>

// namespace rustlike // not in the current version yet

// Purely for error handling, replace something like bool
class rerr : public std::tuple<bool, std::string>
{
public:
    inline rerr(bool success, std::string message = "")
        : std::tuple<bool, std::string>(success, message) {}

    inline bool success() const { return std::get<0>(*this); }
    inline bool failure() const { return !success(); }
    inline std::string message() const { return std::get<1>(*this); }

    // Perfectly as dangerous as it is in rust
    inline void unwrap() const {
        if(!success()) throw std::runtime_error(message());
    }

};

// For functions that has a return value, but also want to return an error message.
template <typename _Type>
class rerr_val : public std::tuple<bool, _Type, std::string>
{
public:
    inline rerr_val(bool success, _Type value = {}, std::string message = "")
        : std::tuple<bool, _Type, std::string>(success, value, message) {}

    inline bool success() const { return std::get<0>(*this); }
    inline bool failure() const { return !success(); }
    inline _Type value() const { return std::get<1>(*this); }
    inline std::string message() const { return std::get<2>(*this); }

    // Perfectly as dangerous as it is in rust
    inline _Type unwrap() const {
        if(!success()) throw std::runtime_error(message());
        return value();
    }
};

// For functions that want multiple error codes.
template <typename _Type, typename _ErrType, _ErrType _ErrSuccessValue = static_cast<_ErrType>(0)>
class rerr_eval : public std::tuple<_ErrType, _Type, std::string>
{
public:
    inline rerr_eval(_ErrType error_code, _Type value = {}, std::string message = "")
        : std::tuple<_ErrType, _Type, std::string>(error_code, value, message) {}

    inline bool success() const { return std::get<0>(*this) == _ErrSuccessValue; }
    inline bool failure() const { return !success(); }
    inline bool error() const { return std::get<0>(*this); }
    inline _Type value() const { return std::get<1>(*this); }
    inline std::string message() const { return std::get<2>(*this); }

    // Perfectly as dangerous as it is in rust
    inline _Type unwrap() const {
        if(!success()) throw std::runtime_error(message());
        return value();
    }
};

#define RERROR(STR) rerr(false, STR)
#define RSUCCESS rerr(true)

// Non-member get functions for structured binding support
template<std::size_t N>
decltype(auto) get(rerr& r) {
    return std::get<N>(static_cast<std::tuple<bool, std::string>&>(r));
}

template<std::size_t N>
decltype(auto) get(const rerr& r) {
    return std::get<N>(static_cast<const std::tuple<bool, std::string>&>(r));
}

template<std::size_t N>
decltype(auto) get(rerr&& r) {
    return std::get<N>(static_cast<std::tuple<bool, std::string>&&>(r));
}

template<std::size_t N, typename _Type>
decltype(auto) get(rerr_val<_Type>& r) {
    return std::get<N>(static_cast<std::tuple<bool, _Type, std::string>&>(r));
}

template<std::size_t N, typename _Type>
decltype(auto) get(const rerr_val<_Type>& r) {
    return std::get<N>(static_cast<const std::tuple<bool, _Type, std::string>&>(r));
}

template<std::size_t N, typename _Type>
decltype(auto) get(rerr_val<_Type>&& r) {
    return std::get<N>(static_cast<std::tuple<bool, _Type, std::string>&&>(r));
}

template<std::size_t N, typename _Type, typename _ErrType, _ErrType _ErrSuccessValue>
decltype(auto) get(rerr_eval<_Type, _ErrType, _ErrSuccessValue>& r) {
    return std::get<N>(static_cast<std::tuple<_ErrType, _Type, std::string>&>(r));
}

template<std::size_t N, typename _Type, typename _ErrType, _ErrType _ErrSuccessValue>
decltype(auto) get(const rerr_eval<_Type, _ErrType, _ErrSuccessValue>& r) {
    return std::get<N>(static_cast<const std::tuple<_ErrType, _Type, std::string>&>(r));
}

template<std::size_t N, typename _Type, typename _ErrType, _ErrType _ErrSuccessValue>
decltype(auto) get(rerr_eval<_Type, _ErrType, _ErrSuccessValue>&& r) {
    return std::get<N>(static_cast<std::tuple<_ErrType, _Type, std::string>&&>(r));
}

// std namespace specializations for structured binding support
namespace std {
    template<>
    struct tuple_size<rerr> : integral_constant<size_t, 2> {};

    template<>
    struct tuple_element<0, rerr> { using type = bool; };

    template<>
    struct tuple_element<1, rerr> { using type = string; };

    template<typename _Type>
    struct tuple_size<rerr_val<_Type>> : integral_constant<size_t, 3> {};

    template<typename _Type>
    struct tuple_element<0, rerr_val<_Type>> { using type = bool; };

    template<typename _Type>
    struct tuple_element<1, rerr_val<_Type>> { using type = _Type; };

    template<typename _Type>
    struct tuple_element<2, rerr_val<_Type>> { using type = string; };

    template<typename _Type, typename _ErrType, _ErrType _ErrSuccessValue>
    struct tuple_size<rerr_eval<_Type, _ErrType, _ErrSuccessValue>> : integral_constant<size_t, 3> {};

    template<typename _Type, typename _ErrType, _ErrType _ErrSuccessValue>
    struct tuple_element<0, rerr_eval<_Type, _ErrType, _ErrSuccessValue>> { using type = _ErrType; };

    template<typename _Type, typename _ErrType, _ErrType _ErrSuccessValue>
    struct tuple_element<1, rerr_eval<_Type, _ErrType, _ErrSuccessValue>> { using type = _Type; };

    template<typename _Type, typename _ErrType, _ErrType _ErrSuccessValue>
    struct tuple_element<2, rerr_eval<_Type, _ErrType, _ErrSuccessValue>> { using type = string; };
}
