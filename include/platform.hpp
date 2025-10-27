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

inline fs::path executable_path() {
    #ifdef OS_WINDOWS
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        return fs::path(path);
    #else
        return fs::read_symlink("/proc/self/exe");
    #endif
}

inline fs::path executable_dir() {
    return executable_path().parent_path();
}

// inline bool change_directory(const std::string& path) {
// #ifdef _WIN32
//     return _chdir(path.c_str()) == 0;
// #else
//     return chdir(path.c_str()) == 0;
// #endif
// }

inline std::string get_env(const std::string& name) {
#ifdef OS_WINDOWS
    char* value = nullptr;
    size_t len = 0;
    errno_t err = _dupenv_s(&value, &len, name.c_str());
    if (err != 0 || value == nullptr) {
        return "";
    }
    std::string result(value);
    free(value);
    return result;
#else
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : "";
#endif
}

inline bool set_env(const std::string& name, const std::string& value) {
#ifdef OS_WINDOWS
    return _putenv_s(name.c_str(), value.c_str()) == 0;
#else
    return setenv(name.c_str(), value.c_str(), 1) == 0;
#endif
}

}