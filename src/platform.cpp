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

std::string wstringToString(const std::wstring &wstr)
{
#ifdef OS_WINDOWS
    if (wstr.empty()) {
        return std::string();
    }
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
#else
    // This method is deprecated in C++17 and removed in C++20, but for C++23 we can still use it.
    if (wstr.empty()) {
        return std::string();
    }
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
#endif
}

std::wstring stringToWstring(const std::string &str)
{
#ifdef OS_WINDOWS
    if (str.empty()) {
        return std::wstring();
    }
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), &wstrTo[0], size_needed);
    return wstrTo;
#else
    if (str.empty()) {
        return std::wstring();
    }
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str);
#endif
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
// platform::linux
namespace linux {


    
} // namespace platform::linux
#endif

} // namespace platform