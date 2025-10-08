#pragma once

#include <filesystem>
namespace fs = std::filesystem;

#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS
    #include <windows.h>
#else
    #define OS_UNIX
    #include <unistd.h>
#endif

inline fs::path executable_path() {
    #ifdef OS_WINDOWS
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        return fs::path(path);
    #else
        return fs::read_symlink("/proc/self/exe");
    #endif
}