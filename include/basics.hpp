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

// Deprecated: use scl2::Rect in scltypes.hpp instead.
// Will be removed in future versions.
struct rect {
    int x, y, w, h;
    rect() {}
    rect(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}
};

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

#endif // lower

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

#endif // upper

// Return true if the character c is in the string ms, otherwise return false.
inline bool charmatch(char c, std::string ms) {
    for(char cs : ms) if(c == cs) return true;
    return false;
}

// Return true if the character c is in the string ms, otherwise return false.
inline bool charmatch(wchar_t c, std::wstring ms) {
    for(wchar_t cs : ms) if(c == cs) return true;
    return false;
}

// Return the number of occurrences of character c in the string ms.
inline size_t numberof(char c, std::string ms) {
    size_t result = 0;
    for(char cs: ms) if(c == cs) result ++;
    return result;
}

// Return the number of occurrences of character c in the string ms.
inline size_t numberof(wchar_t c, std::wstring ms) {
    size_t result = 0;
    for(wchar_t cs: ms) if(c == cs) result ++;
    return result;
}

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


/** @brief Format size_type to a more readable string, like "1.5 KB" instead of "1536 bytes".
 * @param bytes The size in bytes to format.
 * @param isi If true, use 1000 as the divisor (SI units), otherwise use 1024 (binary units). Default is false (binary units).
*/
std::string prettySize(size_t bytes, bool isi = false);

/** @brief Format size_type to a more readable string, like "1.5 KB" instead of "1536 bytes".
 * @param bytes The size in bytes to format.
 * @param isi If true, use 1000 as the divisor (SI units), otherwise use 1024 (binary units). Default is false (binary units).
*/
std::wstring prettySizeW(size_t bytes, bool isi = false);


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