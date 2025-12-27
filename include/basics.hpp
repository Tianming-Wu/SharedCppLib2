#pragma once

#include <string>
#include <sstream>

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