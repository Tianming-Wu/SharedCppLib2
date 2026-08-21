#include "platform.hpp"

namespace platform {

fs::path executable_path() {
    #ifdef OS_WINDOWS
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        return fs::path(path);
    #else
        return fs::read_symlink("/proc/self/exe");
    #endif
}

fs::path executable_dir() {
    return executable_path().parent_path();
}

std::string get_env(const std::string& name) {
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

bool set_env(const std::string& name, const std::string& value) {
#ifdef OS_WINDOWS
    return _putenv_s(name.c_str(), value.c_str()) == 0;
#else
    return setenv(name.c_str(), value.c_str(), 1) == 0;
#endif
}

fs::path findExecutableInPath(const std::string &name)
{
    std::string fullPath;
#ifdef OS_WINDOWS
    fullPath.reserve(260); // MAX_PATH on Windows is typically 260

    DWORD result = SearchPathA(
        nullptr,
        name.c_str(),
        ".exe",
        static_cast<DWORD>(fullPath.capacity()),
        fullPath.data(),
        nullptr
    );

    return (result > 0 && result < fullPath.capacity()) ? fs::path(fullPath.c_str()) : fs::path();
#else
    fullPath.reserve(4096); // Common PATH_MAX on Linux

    char* res = realpath(("/usr/bin/" + name).c_str(), fullPath.data());
    if (res != nullptr) {
        return fs::path(fullPath.c_str());
    }
#endif
    // Fallback: manually search in PATH
    // (use get_env so MSVC's _dupenv_s path handles the C4996 warning)
    std::string pathStr = platform::get_env("PATH");
    if (pathStr.empty()) {
        return fs::path();
    }

    size_t start = 0;
    size_t end = pathStr.find_first_of( ":;" );
    while (end != std::string::npos) {
        fs::path dir = pathStr.substr(start, end - start);
        fs::path candidate = dir / name;
        if (fs::exists(candidate) && fs::is_regular_file(candidate)) {
            return candidate;
        }
        start = end + 1;
        end = pathStr.find_first_of( ":;", start );
    }
    // Check the last segment after the final delimiter
    fs::path dir = pathStr.substr(start);
    fs::path candidate = dir / name;
    if (fs::exists(candidate) && fs::is_regular_file(candidate)) {
        return candidate;
    }
    return fs::path();
}



#ifdef OS_WINDOWS
namespace windows {

std::string TranslateError(DWORD errorCode) {
    if (errorCode == 0) {
        return "";
    }

    LPSTR messageBuffer = nullptr;
    
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0,
        nullptr
    );

    std::string result;
    
    if (size > 0 && messageBuffer != nullptr) {
        // 直接使用系统返回的消息，只移除换行符
        result.assign(messageBuffer, size);
        
        // 移除尾部换行符
        while (!result.empty() && 
               (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
    } else {
        result = "";  // 获取失败返回空字符串
    }

    if (messageBuffer != nullptr) {
        LocalFree(messageBuffer);
    }

    return result;
}


wargProvider::wargProvider() {
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
}

wargProvider::~wargProvider() {
    LocalFree(argv);
}

} // namespace windows
#else // linux
// platform::linux_os
namespace linux_os {


    
} // namespace platform::linux_os
#endif

} // namespace platform