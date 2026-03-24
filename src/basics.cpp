#include "basics.hpp"
#include <SharedCppLib2/version.hpp>

#include <iomanip>

namespace std {

// old implementation of to_string has been removed
    
} //namespace std

namespace scl2 {

std::string version() {
    return std::string(SHAREDCPPLIB2_VERSION);
}

std::string about() {
    return "SharedCppLib2 v" + version() + "\n"
           "A collection of modern C++ utility libraries\n"
           "Author: Tianming (https://github.com/Tianming-Wu)";
}

} // namespace scl2

std::string prettySize(size_t bytes, bool isi)
{
    const char* suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    size_t suffixIndex = 0;
    double count = static_cast<double>(bytes);
    
    if(isi) while(count >= 1000 && suffixIndex < sizeof(suffixes) / sizeof(suffixes[0]) - 1) {
        count /= 1000;
        ++suffixIndex;
    } 
    else while (count >= 1024 && suffixIndex < sizeof(suffixes) / sizeof(suffixes[0]) - 1) {
        count /= 1024;
        ++suffixIndex;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << count << " " << suffixes[suffixIndex];
    return oss.str();
}

std::wstring prettySizeW(size_t bytes, bool isi)
{
    const wchar_t* suffixes[] = {L"B", L"KB", L"MB", L"GB", L"TB", L"PB", L"EB"};
    size_t suffixIndex = 0;
    double count = static_cast<double>(bytes);
    
    if(isi) while(count >= 1000 && suffixIndex < sizeof(suffixes) / sizeof(suffixes[0]) - 1) {
        count /= 1000;
        ++suffixIndex;
    } 
    else while (count >= 1024 && suffixIndex < sizeof(suffixes) / sizeof(suffixes[0]) - 1) {
        count /= 1024;
        ++suffixIndex;
    }
    
    std::wostringstream oss;
    oss << std::fixed << std::setprecision(2) << count << L" " << suffixes[suffixIndex];
    return oss.str();
}
