#include "bytearray.hpp"
#include "Base64.hpp"

namespace std {

#ifndef BYTEARRAY_NO_BASE64

std::string bytearray::toBase64() const
{
    if (this->empty()) return std::string();
    
    return Base64::encode(*this);
}

bytearray bytearray::fromBase64(const std::string& base64str)
{
    if (base64str.empty()) return bytearray();
    
    bytearray result = Base64::decode(base64str);
    return result;
}

#endif

} // namespace std
