/*
    Exception module for SharedCppLib2.

    This is a unified definition file for common SharedCppLib2
    exceptions.
*/

#pragma once

#include <stdexcept>

namespace scl2 {

class exception : public std::exception {
public:
    exception() noexcept = default;
    explicit exception(const std::string& message) noexcept : msg(message) {}
    const char* what() const noexcept override {
        return msg.c_str();
    }
private:
    std::string msg;
};

class insufficient_privileges_exception : public exception {
public:
    insufficient_privileges_exception() noexcept : exception("Insufficient privileges to perform this operation.") {}
};



} // namespace scl2