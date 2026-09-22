#include "perm.hpp"

#ifdef OS_WINDOWS
#  include "string.hpp"   // scl2::str_to_wstr
#  include <shellapi.h>    // ShellExecuteExW
#endif

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
    // 用 ShellExecuteEx + "runas" 重新启动自己，由 UAC 弹框完成提权。
    // 新进程启动后本进程应立即退出（返回 Success 表示已启动）。
    if (isElevated())
        return ElevationResult::AlreadyElevated;

    wchar_t exe_path[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) == 0)
        return ElevationResult::Error;

    // 参数交给 stringlist::pack() 处理引号，再转成宽字符
    std::wstring params;
    if (args.size() > 0)
        params = scl2::str_to_wstr(args.pack());

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"runas";          // 触发提权（UAC）
    sei.lpFile = exe_path;
    sei.lpParameters = params.empty() ? nullptr : params.c_str();
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei)) {
        // 用户在 UAC 对话框中选择“否”
        return GetLastError() == ERROR_CANCELLED
                   ? ElevationResult::Denied
                   : ElevationResult::Error;
    }
    return ElevationResult::Success;
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