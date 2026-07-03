/*
    Lazy Construct module for SharedCppLib2.

    This library is header only.

    [SCL_INDEPENDENT_MODULE]
*/

#pragma once

#include <type_traits>
#include <utility>

namespace scl2 {

// This is basically an RAII wrapper around any class type.
template<typename T, typename... Args>
requires std::is_constructible_v<T, Args...>
class lazy_construct {
public:
    lazy_construct() = default;
    ~lazy_construct() { destroy(); }

    T& get() {
        if (!instance) throw std::runtime_error("lazy_construct: instance not constructed yet");
        return *instance;    
    }

    const T& get() const {
        if (!instance) throw std::runtime_error("lazy_construct: instance not constructed yet");
        return *instance;    
    }

    void construct(Args&&... args) {
        if (instance) throw std::runtime_error("lazy_construct: instance already constructed");
        instance = new T(std::forward<Args>(args)...);
    }

    void destroy() {
        if (instance) {
            delete instance;
            instance = nullptr;
        }
    }

    void reconstruct(Args&&... args) {
        destroy();
        construct(std::forward<Args>(args)...);
    }

    bool available() const {
        return instance != nullptr;
    }

    // optional-like accessors
    explicit operator bool() const { return available(); }
    T& operator*() { return get(); }
    const T& operator*() const { return get(); }
    T* operator->() { return &get(); }
    const T* operator->() const { return &get(); }

private:
    T* instance = nullptr;
};

} // namespace scl2