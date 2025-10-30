#include "platform.hpp"

namespace platform {







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
#endif

} // namespace platform