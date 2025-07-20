#pragma once

#include <string>
#include <sstream>

namespace std {

struct rect {
    int x, y, w, h;
    rect() {}
    rect(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}
};

inline string itos(int d)
{
	stringstream ss; string stl;
	ss << d; ss >> stl;
	return stl;
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
#endif

inline bool charmatch(char c, std::string ms) {
    for(char cs : ms) if(c == cs) return true;
    return false;
}

}; // namespace std