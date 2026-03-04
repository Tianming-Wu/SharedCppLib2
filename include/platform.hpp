/*
    Platform abstraction layer for cross-platform compatibility.
    Provides functions to get executable path, environment variables, etc.
    classes:
        platform
    link target:
        SharedCppLib2::platform

    Note that some apis are only available on specific platforms, since they makes
    no sense on other platforms. Those apis are under platform::windows or platform::linux
    namespaces.
*/
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

// Get Argument in WinMain(), only Unicode is supported
// because Windows only provided unicode api for the required
// function. You can use __argc and __argv anyway, that's totally fine.
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

/*
    This part is to simplify the main entry point definition,
    if you need a console version and gui version of your application
    at the same time.

    Not finished and is currently unusable.
*/

// #ifdef OS_WINDOWS
//     #define UNIVERSIAL_MAIN_ENTRY_POINTA \
//         INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow) { \
//             int argc = __argc; \
//             char** argv = __argv;
    
//     #define UNIVERSIAL_MAIN_ENTRY_POINTW \
//         INT WINAPI WinMainW(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow) { \
//             platform::windows::wargProvider wargs; \
//             int argc = wargs.argc; \
//             LPWSTR* argv = wargs.argv;
    
//     #define UNIVERSIAL_MAIN_END_POINT \
//         LocalFree(argv);
// #else 
//     #define UNIVERSIAL_MAIN_ENTRY_POINTA \
//         int main(int argc, char** argv) {

//     #define UNIVERSIAL_MAIN_ENTRY_POINTW \
//         int wmain(int argc, wchar_t** argv) {

//     #define UNIVERSIAL_MAIN_END_POINT
// #endif



} // namespace platform