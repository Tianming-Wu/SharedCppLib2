#include "bytearray.hpp"
#include "Base64.hpp"

namespace std {

std::string bytearray::toBase64() const
{
    if (this->empty()) return std::string();
    
    return Base64::encode(
        const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(this->data())),
        this->size()
    );
}

bytearray bytearray::fromBase64(const std::string& base64str)
{
    if (base64str.empty()) return bytearray();
    
    std::string decoded = Base64::decode(base64str);
    
    bytearray result;
    result.reserve(decoded.size());
    
    for (char c : decoded) {
        result.push_back(static_cast<byte>(c));
    }
    
    return result;
}

} // namespace std
