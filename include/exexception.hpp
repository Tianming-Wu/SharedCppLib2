/*
    Extended exception module for SharedCppLib2.

    Usage:
        exexception is not completed.

        texexception allow user to take an reference to an arbitrary value with the exception (message).
            throw texexception<int>(42, "The answer to the Ultimate Question of Life, The Universe, and Everything");
*/
#pragma once

#include <stdexcept>

class exexception : public std::exception
{
public:
    exexception() noexcept = default;
    explicit exexception(const std::string& message) noexcept : std::exception(message.c_str()) {}
    virtual ~exexception() noexcept = default;
};


template <typename T>
class texexception : public std::exception
{
public:
    texexception() noexcept = default;
    explicit texexception(const T& value, const std::string& message) noexcept : _value(value), std::exception(message.c_str()) {}
    virtual ~texexception() noexcept = default;

    virtual const T& value() const noexcept {
        return _value;
    }

private:
    T _value;
};