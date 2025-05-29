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


}; // namespace std