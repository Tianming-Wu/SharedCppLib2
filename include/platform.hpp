#pragma once

#include <filesystem>
namespace fs = std::filesystem;

#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS
    #include <windows.h>
    #include <direct.h>
#else
    #define OS_UNIX
    #include <unistd.h>
    #include <sys/wait.h>
#endif

namespace platform {

fs::path executable_path();
fs::path executable_dir();

std::string get_env(const std::string& name);
bool set_env(const std::string& name, const std::string& value);

// Convert wide string to UTF-8 string
std::string wstringToString(const std::wstring& wstr);
// Convert UTF-8 string to wide string
std::wstring stringToWstring(const std::string& str);

/// @brief Find an executable in the system PATH
/// @param name the name of the executable
/// @return empty path if not found, full path if found
fs::path findExecutableInPath(const std::string& name);

#ifdef OS_WINDOWS
// platform::windows
namespace windows {

std::string TranslateError(DWORD errorCode);

inline std::string TranslateLastError() { return TranslateError(GetLastError()); }

class wargProvider {
public:
    wargProvider();
    ~wargProvider();

    int argc;
    LPWSTR* argv;
};

} // namespace platform::windows

#else // linux
// platform::linux
namespace linux {



} // namespace platform::linux
#endif

} // namespace platform
