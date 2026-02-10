/*
    Basic utilities and definitions for SharedCppLib2.
    types:
        std::rect
    link target:
        SharedCppLib2::basic
*/
#pragma once

#include <string>
#include <sstream>
#include <type_traits>
#include <vector>

#include "version.hpp"

namespace std {

struct rect {
    int x, y, w, h;
    rect() {}
    rect(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}
};

[[deprecated]]
inline string itos(int d)
{
	stringstream ss; string stl;
	ss << d; ss >> stl;
	return stl;
}

[[deprecated]]
inline wstring itows(int d)
{
	wstringstream ss; wstring stl;
	ss << d; ss >> stl;
	return stl;
}

template<typename T>
requires requires(const T& t, std::stringstream& test_ss) { test_ss << t; }
std::string streamed_to_string(const T& value) {
    std::stringstream ss_;
    ss_ << value;
    return ss_.str();
}

#ifndef lower
inline std::string lower(const std::string& orig) {
    constexpr int offset = 'a' - 'A';
    std::string result = orig;
    for(char& c : result) {
        if(c <= 'Z' && c >= 'A') c += offset;
    }
    return result;
}
inline std::wstring lower(const std::wstring& orig) {
    constexpr int offset = L'a' - L'A';
    std::wstring result = orig;
    for(wchar_t& c : result) {
        if(c <= L'Z' && c >= L'A') c += offset;
    }
    return result;
}
#endif

#ifndef upper
inline std::string upper(const std::string& orig) {
    constexpr int offset = 'A' - 'a';
    std::string result = orig;
    for(char& c : result) {
        if(c <= 'z' && c >= 'a') c += offset;
    }
    return result;
}
inline std::wstring upper(const std::wstring& orig) {
    constexpr int offset = L'A' - L'a';
    std::wstring result = orig;
    for(wchar_t& c : result) {
        if(c <= L'z' && c >= L'a') c += offset;
    }
    return result;
}
#endif

inline bool charmatch(char c, std::string ms) {
    for(char cs : ms) if(c == cs) return true;
    return false;
}

inline bool charmatch(wchar_t c, std::wstring ms) {
    for(wchar_t cs : ms) if(c == cs) return true;
    return false;
}

inline size_t numberof(char c, std::string ms) {
    size_t result;
    for(char cs: ms) if(c == cs) result ++;
    return result;
}

inline size_t numberof(wchar_t c, std::wstring ms) {
    size_t result = 0;
    for(wchar_t cs: ms) if(c == cs) result ++;
    return result;
}

// #define runOnce(FN_NAME) static int FN_NAME##_runonce = FN_NAME()


}; // namespace std


#ifdef ENABLE_RUNONCE_X
#include <mutex>

class RunOnce {
public:
    template<typename Func, typename... Args>
    static void execute(Func&& fn, Args&&... args) {
        static std::once_flag flag;
        std::call_once(flag, std::forward<Func>(fn), std::forward<Args>(args)...);
    }
};

#define RUN_ONCE(FN, ...) RunOnce::execute(FN, ##__VA_ARGS__)
#endif

/// @brief Allow to scroll through bitwide flags using grammer `for (auto bit : enum_bitwiden(flags))`
/// @tparam E An enum type with bitwise flags, and & operator support.
/// @param value The enum value containing bitwise flags.
/// @return A vector of individual bit flags set in the value.
template<typename E>
requires std::is_enum_v<E>
std::vector<E> enum_bitwiden(E value) {
    using T = std::underlying_type_t<E>;
    std::vector<E> result;
    for (T bit = 1; bit != 0; bit <<= 1) {
        if (static_cast<T>(value) & bit) {
            result.push_back(static_cast<E>(bit));
        }
    }
    return result;
}

// Post a warning asking the user to disable windows.h min/max macros
// That was a terrible namespace pollution. You basically CANT ESCAPE because IT FUCKING POLLUTES ANY NAMESPACE
#if defined min || defined max
    #error "min and max macros are defined. If you included windows.h, consider defining NOMINMAX before including it to avoid conflicts with std::min and std::max. You may encounter problems like error C2589. It is the namespace pollution caused by windows.h."
#endif

template<typename E>
requires std::is_enum_v<E>
std::vector<E> enum_bitwiden_range(E value, size_t min, size_t max) {
    using T = std::underlying_type_t<E>;
    max = std::min(max, sizeof(T) * 8); // No exceed the number of bits in the underlying type
    std::vector<E> result;
    for (T bit = 1 << min; bit != 0 && bit < (1 << max); bit <<= 1) {
        if (static_cast<T>(value) & bit) {
            result.push_back(static_cast<E>(bit));
        }
    }
    return result;
}

// /// @brief Get the number of keys in the enum
// template<typename E>
// requires std::is_enum_v<E>
// constexpr size_t __sizeof_enum() {
//     for()
// }

#define Define_Enum_BitOperators(NAME) \
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

#define Define_Enum_BitOperators_Inclass(NAME) \
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

#define disable_copy(CLASS) \
    CLASS(const CLASS&) = delete; \
    CLASS& operator=(const CLASS&) = delete;

#define disable_move(CLASS) \
    CLASS(CLASS&&) = delete; \
    CLASS& operator=(CLASS&&) = delete;

#define enable_copy(CLASS) \
    CLASS(const CLASS&) = default; \
    CLASS& operator=(const CLASS&) = default;

#define enable_move(CLASS) \
    CLASS(CLASS&&) = default; \
    CLASS& operator=(CLASS&&) = default;

#define enable_copy_move(CLASS) \
    enable_copy(CLASS) \
    enable_move(CLASS)

#define disable_copy_move(CLASS) \
    disable_copy(CLASS) \
    disable_move(CLASS)

#define enable_copy_only(CLASS) \
    enable_copy(CLASS) \
    disable_move(CLASS)

#define enable_move_only(CLASS) \
    disable_copy(CLASS) \
    enable_move(CLASS)

namespace scl2 {
/// Get the version string of SharedCppLib2
/// @return Version in format "major.minor.patch"
std::string version();

/// Get detailed information about SharedCppLib2
/// @return Detailed version and build information
std::string about();
} // namespace scl2