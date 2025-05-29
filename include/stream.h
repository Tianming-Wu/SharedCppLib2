#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

namespace streamobj {

template<typename T>
struct stream_wrapper {
    const T& ref;
    explicit stream_wrapper(const T& obj) : ref(obj) {}
};

template<typename T>
stream_wrapper<T> stream(const T& obj) {
    return stream_wrapper<T>(obj);
}

}