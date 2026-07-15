/*
    Permission module for SharedCppLib2.

    Unified API for checking and requesting elevated privileges.

    Platform correspondence:
        Windows:  isAdmin() ≈ Unix: isRoot() / hasSudo()
        Windows:  isNtSystem() ≡ Unix: isRoot() (uid==0)
        Android:  isRoot() + hasSuBinary()
*/

#pragma once

#include <cstdint>
#include "platform.hpp"
#include "stringlist.hpp"

namespace scl2::perm {

// --- Unified API (all platforms) ---

/// @brief Whether the process has elevated (admin/root) privileges or can obtain them.
bool isElevated();

/// @brief Whether running as the highest system account (NT AUTHORITY\\SYSTEM / uid==0).
bool isSystem();

// --- Elevation ---

enum class ElevationResult : uint8_t {
    Success = 0,          // Elevated successfully; caller should exit.
    AlreadyElevated = 1,  // Already running with target privileges.
    Denied = 2,           // User denied or authentication failed.
    NotPossible = 3,      // Elevation not possible on this platform / config.
    Error = 4,            // Other error.
};

/// @brief Whether elevation is possible in the current environment.
/// If false, elevate() will always return NotPossible.
bool elevatable();

/// @brief Attempt to restart the current process with elevated privileges.
/// @param args Arguments to pass to the restarted process.
/// @return ElevationResult indicating the outcome.
ElevationResult elevate(const scl2::stringlist& args = {});

// --- Platform-specific API ---

#ifdef OS_WINDOWS
    bool isAdmin();       // Member of Administrators group
    bool isNtSystem();    // Running as NT AUTHORITY\\SYSTEM
#else
    bool isRoot();        // uid == 0 (all Unix including Android)
    #ifdef OS_ANDROID
        bool hasSuBinary();   // su binary present on system
    #else
        bool hasSudo();       // Can use sudo (non-Android Unix)
    #endif
#endif

} // namespace scl2::perm