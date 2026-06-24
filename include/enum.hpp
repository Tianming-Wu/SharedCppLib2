/*
    Enum module for SharedCppLib2.

    Enum: Normal enum type



    BitEnum: Enum type that is designed to be used as bit flags. It
             provides bitwise operators and some helper functions.

        Range behavior:
        - bitenum_iterator(value):
            Uses scl2_bitenum_traits(E, min, max) if provided, otherwise
            falls back to full underlying bitwidth [0, bitwidth).
        - bitenum_ranged_iterator(value, minVal, maxVal):
            Always uses caller-provided range [minVal, maxVal) and does not
            depend on traits.


*/

#pragma once

#include <type_traits>
#include <generator> // for coroutine
#include <optional>
#include <map>
#include <initializer_list>

// Defense against windows.h macros
// Who would ever use them nowadays?
// If you are working with C++23, which is the basic requirement for
// using this library, I believe that you should be using the ones
// in the std library, not those shitty macros that pollute everything.

#ifdef min
    #undef min
#endif

#ifdef max
    #undef max
#endif


namespace scl2 {

// Insert this at the end of your enum definition, to allow automatic
// detection of enum size. Not for bitenum.
/*
    For example:

    enum class MyEnum {
        ValueA,
        ValueB,
        ValueC,
        scl2_enum_size // This must be the last element, and it is not a real enum value.
    };

    Then scl2 can automatically detect that MyEnum has 3 valid values (0, 1, 2) and the size is 3.
    However, you must not assign any value manually, or make sure they started at 0 and are
    contiguous, since this is just a little trick to avoid editing the size manually wherever
    needed.
*/
#define scl2_enum_size _

// template<typename E>
// requires std::is_enum_v<E>
// struct bitenum_traits {
//     using underlying_type = std::underlying_type_t<E>;
//     static constexpr size_t bitwidth = sizeof(underlying_type) * 8;
// };


// template<typename E, size_t _min, size_t _max>
// requires std::is_enum_v<E>
// struct bitenum_traits {
//     static constexpr size_t bitwidth = sizeof(std::underlying_type_t<E>) * 8;
//     static constexpr size_t min = _min, max = _max;
// };

template<typename E>
requires std::is_enum_v<E>
struct bitenum_traits_lookup {
    static constexpr size_t bitwidth = sizeof(std::underlying_type_t<E>) * 8;
    static constexpr size_t min = 0;
    static constexpr size_t max = bitwidth;
};

#define scl2_bitenum_traits(NAME, MIN, MAX) \
    template<> struct ::scl2::bitenum_traits_lookup<NAME> { \
        static constexpr size_t bitwidth = sizeof(std::underlying_type_t<NAME>) * 8; \
        static constexpr size_t min = (MIN); \
        static constexpr size_t max = (MAX); \
        static_assert(min < bitwidth, "bitenum_traits min must be less than the bitwidth of the underlying type"); \
        static_assert(max <= bitwidth, "bitenum_traits max must be less than or equal to the bitwidth of the underlying type"); \
        static_assert(min < max, "bitenum_traits min must be less than max"); \
    };


// For developers:
/* Example:

enum class myFlags : uint32_t {
    FlagA = 1 << 0,
    FlagB = 1 << 1,
    FlagC = 1 << 2,
};

scl2_enum_bitop(myFlags) // to enable bitwise operators for myFlags
scl2_bitenum_traits(myFlags, 0, 3) // to specify the valid bit range for myFlags (0 to 2 in this case, since we have 3 flags)

Then scl2's bitenum utilities can recognize the range of your enum and work properly with it.
*/


/// @brief Allow to scroll through bitwide flags using grammer `for (auto bit : enum_bitwiden(flags))`
/// @tparam E An enum type with bitwise flags, and & operator support.
/// @param value The enum value containing bitwise flags.
/// @return A vector of individual bit flags set in the value.
template<typename E>
requires std::is_enum_v<E>
std::generator<E> bitenum_iterator(E value) {
    using T = std::underlying_type_t<E>;
    using U = std::make_unsigned_t<T>;

    constexpr size_t min = bitenum_traits_lookup<E>::min;
    constexpr size_t max = bitenum_traits_lookup<E>::max;

    const U raw = static_cast<U>(value);
    for (size_t bit_index = min; bit_index < max; ++bit_index) {
        const U bit = static_cast<U>(U{1} << bit_index);
        if ((raw & bit) != 0) {
            co_yield static_cast<E>(static_cast<T>(bit));
        }
    }
}

/// @brief Iterate set flags with caller-provided bit range [minVal, maxVal).
/// @tparam E An enum type with bitwise flags, and & operator support.
/// @param value The enum value containing bitwise flags.
/// @param minVal The inclusive minimum bit index.
/// @param maxVal The exclusive maximum bit index.
/// @return A generator of individual set bit flags in the given range.
template<typename E>
requires std::is_enum_v<E>
std::generator<E> bitenum_ranged_iterator(E value, size_t minVal, size_t maxVal) {
    using T = std::underlying_type_t<E>;
    using U = std::make_unsigned_t<T>;

    constexpr size_t bitwidth = sizeof(U) * 8;
    const size_t begin = (minVal < bitwidth) ? minVal : bitwidth;
    const size_t end = (maxVal < bitwidth) ? maxVal : bitwidth;

    const U raw = static_cast<U>(value);
    for (size_t bit_index = begin; bit_index < end; ++bit_index) {
        const U bit = static_cast<U>(U{1} << bit_index);
        if ((raw & bit) != 0) {
            co_yield static_cast<E>(static_cast<T>(bit));
        }
    }
}

// enum_bitwiden_range was a pre-migration legacy implementation.
// It is intentionally removed in favor of generator-based bitenum_iterator.


// For your bit flags.

// However, some operators such as |=, &=, ^= are not provided by
// the macro. Just use something like e = (e | FlagA) to get the same
// effect.
// This is one of the things that you want to be perfectly implicit,
// and make it easy while also safe.

#define scl2_enum_bitop(NAME) \
    inline constexpr NAME operator|(NAME a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(static_cast<T>(a) | static_cast<T>(b)); \
    } \
    inline constexpr NAME operator^(NAME a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(static_cast<T>(a) ^ static_cast<T>(b)); \
    } \
    inline constexpr NAME operator&(NAME a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(static_cast<T>(a) & static_cast<T>(b)); \
    } \
    inline constexpr NAME operator~(NAME a) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(~static_cast<T>(a)); \
    }

// Well the old name is kind of misleading.
#define scl2_bitenum_op(NAME) scl2_enum_bitop(NAME)

// For nested enum declarations.
#define scl2_enum_bitop_inclass(NAME) \
    friend inline constexpr NAME operator|(NAME a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(static_cast<T>(a) | static_cast<T>(b)); \
    } \
    friend inline constexpr NAME operator^(NAME a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(static_cast<T>(a) ^ static_cast<T>(b)); \
    } \
    friend inline constexpr NAME operator&(NAME a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(static_cast<T>(a) & static_cast<T>(b)); \
    } \
    friend inline constexpr NAME operator~(NAME a) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        return static_cast<NAME>(~static_cast<T>(a)); \
    }

#define scl2_bitenum_op_inclass(NAME) scl2_enum_bitop_inclass(NAME)

// Ex bitenum version: compared to normal version, includes &=, |=, ^=.
#define scl2_enum_bitopex(NAME) \
    scl2_enum_bitop(NAME)\
    inline constexpr NAME& operator|=(NAME &a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        a = static_cast<NAME>(static_cast<T>(a) | static_cast<T>(b)); \
        return a; \
    } \
    inline constexpr NAME& operator&=(NAME &a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        a = static_cast<NAME>(static_cast<T>(a) & static_cast<T>(b)); \
        return a; \
    } \
    inline constexpr NAME& operator^=(NAME &a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        a = static_cast<NAME>(static_cast<T>(a) ^ static_cast<T>(b)); \
        return a; \
    } \
    inline constexpr NAME& operator&=(NAME &a, NAME b) noexcept { \
        using T = std::underlying_type_t<NAME>; \
        a = static_cast<NAME>(static_cast<T>(a) & static_cast<T>(b)); \
        return a; \
    }

/*
    Automatic enum lookup map generation.

    This utility allows you to output enum values as strings, which
    is more readable than raw numeric values.

    Also try magic_enum library, it is much better than this and
    provides more features. This is just a built-in implementation,
    since this library cannot depend on any external libraries
    other than the C++ standard library.

    So, you can only insert values manually.
*/


// This determines the behavior when an enum value is not found in the map.
enum strenum_fallback_type {
    strenum_fallback_value = 0, // fallback to raw integer
    strenum_fallback_empty,     // fallback to empty string
    strenum_fallback_none,      // fallback to std::nullopt
};

template<typename E, typename CharT = char>
class strenum : public std::map<E, std::basic_string<CharT>> {
public:
    using string_type = std::basic_string<CharT>;
    using enum_type = E;

    strenum(
        std::initializer_list<std::pair<E, std::basic_string<CharT>>> list,
        strenum_fallback_type fallback = strenum_fallback_none
    ) : fallback_type(fallback)
    {
        for(auto& pair : list) {
            this->insert(pair);
        }
    }
    
    std::optional<string_type> to_string(E value) const {
        auto it = this->find(static_cast<E>(value));
        if (it != this->end()) {
            return it->second;
        } else {
            return fallback(value);
        }
    }

private:
    strenum_fallback_type fallback_type = strenum_fallback_none;

    std::optional<string_type> fallback(E val) const {
        switch (fallback_type) {
            case strenum_fallback_value:
                return std::to_string(static_cast<std::underlying_type_t<E>>(val));
            case strenum_fallback_empty:
                return string_type();
            case strenum_fallback_none:
            default:
                return std::nullopt;
        }
    }
};


enum bitstrenum_fallback_type {
    bitstrenum_fallback_value = 0, // fallback to raw integer
    bitstrenum_fallback_empty,     // fallback to empty string
    bitstrenum_fallback_none,      // fallback to std::nullopt
    bitstrenum_fallback_partial,   // fallback to partial string for known bits, and ignore unknown bits
    bitstrenum_fallback_bitset,    // fallback to a string representation of the bitset, e.g. "0b101010"
    bitstrenum_fallback_combined,  // fallback to something like ValueA | ValueC | 0b10000000 for known and unknown bits combined
    bitstrenum_fallback_combinedm, // same as combined, but use 1 << n to represent unknown bits.
};

template<typename E, typename CharT = char>
class bitstrenum : public std::map<E, std::basic_string<CharT>> {
public:
    using string_type = std::basic_string<CharT>;
    using enum_type = E;

    bitstrenum(
        std::initializer_list<std::pair<E, std::basic_string<CharT>>> list,
        bitstrenum_fallback_type fallback = bitstrenum_fallback_none
    ) : fallback_type(fallback)
    {
        for(auto& pair : list) {
            this->insert(pair);
        }
    }

    std::optional<string_type> to_string(E value) const {
        std::string result;
        bool has_unknown = false;
        for(auto flag : bitenum_iterator(static_cast<E>(value))) {
            auto it = this->find(flag);
            if (it != this->end()) {
                if (!result.empty()) {
                    result += " | ";
                }
                result += it->second;
            } else {
                has_unknown = true;
            }
        }
        if (result.empty() || has_unknown) {
            return fallback(value);
        }
        return std::make_optional(string_type(result));
    }

private:
    bitstrenum_fallback_type fallback_type = bitstrenum_fallback_none;

    std::optional<string_type> fallback(E val) const {
        switch (fallback_type) {
            case bitstrenum_fallback_value:
                return std::to_string(static_cast<std::underlying_type_t<E>>(val));
            case bitstrenum_fallback_empty:
                return string_type();
            case bitstrenum_fallback_none:
                return std::nullopt;
            case bitstrenum_fallback_partial: {
                std::string result;
                for(auto flag : bitenum_iterator(static_cast<E>(val))) {
                    auto it = this->find(flag);
                    if (it != this->end()) {
                        if (!result.empty()) {
                            result += " | ";
                        }
                        result += it->second;
                    }
                }
                return std::make_optional(string_type(result));
            }
            case bitstrenum_fallback_bitset: {
                using U = std::make_unsigned_t<std::underlying_type_t<E>>;
                U raw = static_cast<U>(val);
                std::string result = "0b";
                for (size_t i = 0; i < sizeof(U) * 8; ++i) {
                    result += ((raw & (U{1} << i)) != 0) ? '1' : '0';
                }
                return std::make_optional(string_type(result));
            }
            case bitstrenum_fallback_combined: {
                std::string result;
                for(auto flag : bitenum_iterator(static_cast<E>(val))) {
                    auto it = this->find(flag);
                    if (it != this->end()) {
                        if (!result.empty()) {
                            result += " | ";
                        }
                        result += it->second;
                    } else {
                        // For unknown bits, we can still represent them as a bitset
                        using U = std::make_unsigned_t<std::underlying_type_t<E>>;
                        U raw = static_cast<U>(flag);
                        std::string bitset = "0b";
                        for (size_t i = 0; i < sizeof(U) * 8; ++i) {
                            bitset += ((raw & (U{1} << i)) != 0) ? '1' : '0';
                        }
                        if (!result.empty()) {
                            result += " | ";
                        }
                        result += bitset;
                    }
                }
                return std::make_optional(string_type(result));
            }
            case bitstrenum_fallback_combinedm: {
                std::string result;
                for(auto flag : bitenum_iterator(static_cast<E>(val))) {
                    auto it = this->find(flag);
                    if (it != this->end()) {
                        if (!result.empty()) {
                            result += " | ";
                        }
                        result += it->second;
                    } else {
                        using U = std::make_unsigned_t<std::underlying_type_t<E>>;
                        U raw = static_cast<U>(flag);
                        for (size_t i = 0; i < sizeof(U) * 8; ++i) {
                            if ((raw & (U{1} << i)) != 0) {
                                if (!result.empty()) {
                                    result += " | ";
                                }
                                result += "1 << " + std::to_string(i);
                            }
                        }
                    }
                }
                return std::make_optional(string_type(result));
            }
            default:
                return std::nullopt;
        } // switch()
    }
};

// Fallback templates for SFINAE detection.  Return void so that
// scl2::to_string can test whether .to_string(value) is callable.
// When a user defines a map via scl2_strenum/scl2_bitstrenum, the
// non-template overload (preferred) returns a reference to the map.
template<typename E>
requires std::is_enum_v<E>
void get_strenum_map(E) {}

template<typename E>
requires std::is_enum_v<E>
void get_bitstrenum_map(E) {}

template<typename E, typename CharT = char>
requires std::is_enum_v<E>
inline std::optional<std::basic_string<CharT>> to_string(E value) {
    // ADL + unqualified lookup: try strenum first, then bitstrenum.
    // The non-template overload (from macro) returns a map reference;
    // the fallback template returns void, so .to_string() is invalid.
    if constexpr (requires { get_strenum_map(value).to_string(value); }) {
        return get_strenum_map(value).to_string(value);
    } else if constexpr (requires { get_bitstrenum_map(value).to_string(value); }) {
        return get_bitstrenum_map(value).to_string(value);
    } else {
        static_assert(sizeof(E) == 0, "No strenum or bitstrenum map defined for this enum type. Use scl2_strenum or scl2_bitstrenum macro.");
    }
}


// macros for better definition
// In-class enum support is currently not provided

// Helper to wrap each key-value pair.  Commas inside braces {} are NOT
// protected by the preprocessor, so each pair must be wrapped in a
// function-like macro call like scl2_pair(K, V).
#define scl2_pair(K, V) {K, V}

// NOTE: scl2_pair() is required to wrap each {K, V} entry because the
// C/C++ preprocessor does NOT treat braces {} as nesting delimiters —
// commas inside {} still split macro arguments.
#define scl2_strenum(NAME, FALLBACK, ...) \
    static inline const ::scl2::strenum<NAME> NAME##_strenum({__VA_ARGS__}, FALLBACK); \
    inline const ::scl2::strenum<NAME>& get_strenum_map(NAME) { \
        return NAME##_strenum; \
    }

#define scl2_bitstrenum(NAME, FALLBACK, ...) \
    static inline const ::scl2::bitstrenum<NAME> NAME##_bitstrenum({__VA_ARGS__}, FALLBACK); \
    inline const ::scl2::bitstrenum<NAME>& get_bitstrenum_map(NAME) { \
        return NAME##_bitstrenum; \
    }


// output grammer helper.

// template<typename E, typename CharT = char>
// requires std::is_enum_v<E>
// std::optional<std::basic_string<CharT>> to_string(E value) {
//     if constexpr () {
        
//     }
// }

/*
    Usage Example:

    enum class Color {
        Red,
        Green,
        Blue
    };
    scl2_strenum(Color, strenum_fallback_value,
        scl2_pair(Color::Red, "Red"),
        scl2_pair(Color::Green, "Green"),
        scl2_pair(Color::Blue, "Blue")
    )

    Then you can do:
    Color c = Color::Green;
    std::cout << scl2::to_string(c).value_or("Unknown") << std::endl; // Output: Green
*/


// Enum converter between different enum types
// For when you have to define two different enums with the same set of values
// but with different values.

template<typename E1, typename E2>
class enum_cvrt {
public:
    using enum_type_1 = E1;
    using enum_type_2 = E2;

    // It is suggested that two enums have the same underlying type,
    // but is not forced.

    enum_cvrt(std::initializer_list<std::pair<E1, E2>> list) {
        for (const auto& pair : list) {
            map12[pair.first] = pair.second;
            map21[pair.second] = pair.first;
        }
    }

    enum_type_2 convert(enum_type_1 value) const {
        auto it = map12.find(value);
        if (it != map12.end()) {
            return it->second;
        } else {
            throw std::out_of_range("Value not found in enum converter map");
        }
    }

    enum_type_1 convert(enum_type_2 value) const {
        auto it = map21.find(value);
        if (it != map21.end()) {
            return it->second;
        } else {
            throw std::out_of_range("Value not found in enum converter map");
        }
    }
private:
    std::map<E1, E2> map12;
    std::map<E2, E1> map21;

};



} // namespace scl2