#include "basics.hpp"
#include <SharedCppLib2/version.hpp>

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