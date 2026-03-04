/*
    Debug helper module for SharedCppLib2.

*/

#pragma once

#include <iostream>
#include <sstream>

#include <platform.hpp>

class ods
{
public:
    ods();
    ~ods();

    template <typename T>
    requires requires(std::stringstream& ss, const T& value) { ss << value; }
    ods& operator<<(const T& value) {
        ss << value;
        return *this;
    }

    template <typename T>
    requires (
        requires(std::wstringstream& ss, const T& value) { ss << value; }
        && !requires(std::stringstream& ss, const T& value) { ss << value; }
    )
    ods& operator<<(const T& value) {
        ss << platform::wstringToString(value);
        return *this;
    }

    template <typename T>
    requires requires(const T& t) {
        requires std::is_class_v<T>;
        { t.serialize() } -> std::convertible_to<std::string>;
    }
    ods& operator<<(const T& value) {
        ss << value.serialize();
        return *this;
    }

private:
    std::stringstream ss;
};