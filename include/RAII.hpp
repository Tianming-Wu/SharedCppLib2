#pragma once

#include <functional>
#include <type_traits>
#include <utility>

#if defined(_WIN32) || defined(OS_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define SCL2_RAII_RESTORE_LEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#define SCL2_RAII_RESTORE_NOMINMAX
#endif
#include <windows.h>
#ifdef SCL2_RAII_RESTORE_LEAN
#undef WIN32_LEAN_AND_MEAN
#undef SCL2_RAII_RESTORE_LEAN
#endif
#ifdef SCL2_RAII_RESTORE_NOMINMAX
#undef NOMINMAX
#undef SCL2_RAII_RESTORE_NOMINMAX
#endif
#endif

namespace scl2 {

/// Unique ownership of a resource and its deleter. Deleters must not throw.
/// Example: RAII<int> fd(handle, [](int& h) { close(h); });
template<class Resource, class Deleter = std::function<void(Resource&)>>
class RAII {
public:
    using resource_type = Resource;
    using deleter_type = Deleter;

    static_assert(std::is_nothrow_move_constructible_v<resource_type>);
    static_assert(std::is_nothrow_move_constructible_v<deleter_type>);

    explicit RAII(resource_type resource, deleter_type deleter = {})
        : resource_(std::move(resource)), deleter_(std::move(deleter)) {}

    RAII(const RAII&) = delete;
    RAII& operator=(const RAII&) = delete;

    RAII(RAII&& other) noexcept
        : resource_(std::move(other.resource_)), deleter_(std::move(other.deleter_)),
          owns_(std::exchange(other.owns_, false)) {}

    RAII& operator=(RAII&& other) noexcept
        requires (std::is_nothrow_move_assignable_v<resource_type> &&
                  std::is_nothrow_move_assignable_v<deleter_type>)
    {
        if (this != &other) {
            reset();
            resource_ = std::move(other.resource_);
            deleter_ = std::move(other.deleter_);
            owns_ = std::exchange(other.owns_, false);
        }
        return *this;
    }

    ~RAII() { reset(); }

    resource_type& get() noexcept { return resource_; }
    const resource_type& get() const noexcept { return resource_; }
    explicit operator bool() const noexcept { return owns_; }

    /// Relinquish ownership without calling the deleter.
    resource_type release() noexcept {
        owns_ = false;
        return std::move(resource_);
    }

    /// Release the currently owned resource, at most once.
    void reset() noexcept {
        if (!std::exchange(owns_, false)) return;
        if constexpr (requires { static_cast<bool>(deleter_); }) {
            if (!static_cast<bool>(deleter_)) return;
        }
        std::invoke(deleter_, resource_);
    }

private:
    resource_type resource_;
    [[no_unique_address]] deleter_type deleter_;
    bool owns_ = true;
};

#if defined(_WIN32) || defined(OS_WINDOWS)
struct handle_deleter {
    void operator()(HANDLE& handle) const noexcept {
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    }
};

using RAII_HANDLE = RAII<HANDLE, handle_deleter>;
#endif


} // namespace scl2
