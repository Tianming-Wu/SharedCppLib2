/*
    Structural Binding support template for multiple types.
    [[Warning]] This module is NOT TESTED.

    Three ways to support structured binding:

    1. Inherit from std::tuple:
       struct MyType : std::tuple<std::string, int, double> { ... };
       STRUCTURAL_BINDING_TUPLE(MyType, std::string, int, double)
       auto [a, b, c] = myType;

    2. With conversion operator:
       struct MyType {
           std::string a; int b; double c;
           operator std::tuple<std::string&, int&, double&>() {
               return {a, b, c};
           }
       };
       STRUCTURAL_BINDING_TUPLE(MyType, std::string, int, double)
       // Compiler will use the conversion operator to get the tuple

    3. Manual get functions:
       struct MyType { std::string a; int b; };
       template<> struct std::tuple_size<MyType> : std::integral_constant<size_t, 2> {};
       template<> struct std::tuple_element<0, MyType> { using type = std::string; };
       template<> struct std::tuple_element<1, MyType> { using type = int; };
       template<size_t N> decltype(auto) get(const MyType& m) { ... }
       template<size_t N> decltype(auto) get(MyType& m) { ... }
       template<size_t N> decltype(auto) get(MyType&& m) { ... }
*/

#pragma once

#include <tuple>
#include <utility>

// Allow user to generate structural binding support for their own types easily

// namespace std {

// // Generic tuple_element specialization for structural_binding_traits
// // This delegates to std::tuple to handle any N value correctly
// template<size_t N, typename _Binding_Type, typename... _Arg_Types>
// struct tuple_element<N, structural_binding_traits<_Binding_Type, _Arg_Types...>> {
//     using type = typename std::tuple_element<N, std::tuple<_Arg_Types...>>::type;
// };

// // Generic tuple_size specialization for structural_binding_traits
// template<typename _Binding_Type, typename... _Arg_Types>
// struct tuple_size<structural_binding_traits<_Binding_Type, _Arg_Types...>> 
//     : std::integral_constant<size_t, sizeof...(_Arg_Types)> {};

// } // namespace std

// // Forward declaration (will be specialized by macros)
// template<typename _Binding_Type, typename... _Arg_Types>
// struct structural_binding_traits;



// Macro 1: For types that inherit from std::tuple
// #define STRUCTURAL_BINDING_TUPLE(TYPE, ...) \
//     template<> \
//     struct structural_binding_traits<TYPE, __VA_ARGS__> {}; \
//     \
//     template<size_t N> \
//     decltype(auto) get(const TYPE& binding) { \
//         return std::get<N>(static_cast<const std::tuple<__VA_ARGS__>&>(binding)); \
//     } \
//     \
//     template<size_t N> \
//     decltype(auto) get(TYPE& binding) { \
//         return std::get<N>(static_cast<std::tuple<__VA_ARGS__>&>(binding)); \
//     } \
//     \
//     template<size_t N> \
//     decltype(auto) get(TYPE&& binding) { \
//         return std::get<N>(static_cast<std::tuple<__VA_ARGS__>&&>(binding)); \
//     } \
//     \
//     namespace std { \
//         template<> \
//         struct tuple_size<TYPE> : std::integral_constant<size_t, sizeof...(__VA_ARGS__)> {}; \
//         \
//         template<size_t N> \
//         struct tuple_element<N, TYPE> { \
//             using type = typename std::tuple_element<N, std::tuple<__VA_ARGS__>>::type; \
//         }; \
//     }

// Macro 2: For types with public members (backward compatibility alias)
// #define STRUCTURAL_BINDING(TYPE, ...) STRUCTURAL_BINDING_TUPLE(TYPE, __VA_ARGS__)




// Macros for easily define these things:

#define STRUCTURAL_BINDING_BEGIN namespace std {
#define STRUCTURAL_BINDING_END }

#define STRUCTURAL_BINDING_DEFINE_SIZE(TYPE, SIZE) \
    template<> struct tuple_size<TYPE> : std::integral_constant<size_t, SIZE> {};

#define STRUCTURAL_BINDING_DEFINE_ELEMENT(TYPE, INDEX, ELEMENT_TYPE) \
    template<> struct tuple_element<INDEX, TYPE> { using type = ELEMENT_TYPE; };

#define STRUCTURAL_BINDING_DEFINE_GET(TYPE, INDEX, MEMBER) \
    template<size_t I> \
    requires (I == INDEX) \
    decltype(auto) get(const TYPE& binding) { return (binding.MEMBER); } \
    template<size_t I> \
    requires (I == INDEX) \
    decltype(auto) get(TYPE& binding) { return (binding.MEMBER); } \
    template<size_t I> \
    requires (I == INDEX) \
    decltype(auto) get(TYPE&& binding) { return std::move(binding.MEMBER); }