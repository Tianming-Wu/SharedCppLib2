#include "bytearray.hpp"
#include "base64.hpp"

namespace scl2 {

#ifndef BYTEARRAY_NO_BASE64

std::string bytearray::toBase64() const
{
    if (this->empty()) return std::string();
    
    return base64::encode(*this);
}

bytearray bytearray::fromBase64(const std::string& base64str)
{
    if (base64str.empty()) return bytearray();
    
    bytearray result = base64::decode(base64str);
    return result;
}

#endif

} // namespace std
