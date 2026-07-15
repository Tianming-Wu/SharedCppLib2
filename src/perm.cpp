#include "perm.hpp"

namespace scl2::perm {

// ============================================================
//  Unified API
// ============================================================

bool isElevated()
{
#ifdef OS_WINDOWS
    return isAdmin();
#elif defined(OS_ANDROID)
    return isRoot() || hasSuBinary();
#else
    return isRoot() || hasSudo();
#endif
}

bool isSystem()
{
#ifdef OS_WINDOWS
    return isNtSystem();
#else
    return isRoot();
#endif
}

// ============================================================
//  Elevation
// ============================================================

bool elevatable()
{
#ifdef OS_WINDOWS
    return true;
#elif defined(OS_ANDROID)
    return hasSuBinary();
#else
    return hasSudo();
#endif
}

ElevationResult elevate(const scl2::stringlist& args)
{
    if (!elevatable())
        return ElevationResult::NotPossible;

#ifdef OS_WINDOWS
    // TODO: ShellExecute with "runas" verb
    return ElevationResult::Error;
#elif defined(OS_ANDROID)
    if (geteuid() == 0)
        return ElevationResult::AlreadyElevated;

    std::string process_path = fs::read_symlink("/proc/self/exe").string();
    std::string command = "su -c \"" + process_path + "\" " + args.pack();

    if (system(command.c_str()) == 0)
        return ElevationResult::Success;
    else
        return ElevationResult::Denied;
#else
    // TODO: pkexec / sudo-based elevation for desktop Linux
    return ElevationResult::Error;
#endif
}

// ============================================================
//  Windows
// ============================================================

#ifdef OS_WINDOWS

bool isAdmin()
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin == TRUE;
}

bool isNtSystem()
{
    BOOL isSystem = FALSE;
    PSID systemGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 1, SECURITY_LOCAL_SYSTEM_RID,
                                 0, 0, 0, 0, 0, 0, 0, &systemGroup)) {
        CheckTokenMembership(nullptr, systemGroup, &isSystem);
        FreeSid(systemGroup);
    }
    return isSystem == TRUE;
}

// ============================================================
//  Unix (Linux / Android)
// ============================================================

#else // OS_UNIX

bool isRoot()
{
    return geteuid() == 0;
}

#ifdef OS_ANDROID

bool hasSuBinary()
{
    return fs::exists("/system/bin/su") || fs::exists("/system/xbin/su");
}

#else // non-Android Unix

bool hasSudo()
{
    return (system("sudo -n true 2>/dev/null") == 0);
}

#endif // OS_ANDROID
#endif // OS_WINDOWS (else)

} // namespace scl2::perm