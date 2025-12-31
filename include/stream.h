#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <chrono>

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


template <typename CharT>
class basic_sclstream {
public:
    basic_sclstream() = default;
    virtual ~basic_sclstream() = default;

    virtual bool readyRead();
    virtual bool waitForReadyRead(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    std::basic_streambuf<CharT>& handle();
};