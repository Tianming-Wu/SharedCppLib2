#include "bytearray.h"
#include "stringlist.hpp" 

byte ByteArray::at(size_t i) const{
    return std::vector<byte>::at(i);
}

byte ByteArray::vat(size_t p, const byte &v) const {
    return (p<size())?at(p):v;
}

void ByteArray::append(const ByteArray &ba) {
    for(const byte &b : ba) {
        push_back(b);
    }
}

void ByteArray::append(const byte &b) {
    push_back(b);
}


void ByteArray::append(const byte* pb, size_t size) {
    for(size_t i = 0; i < size; i++) {
        push_back(pb[i]);
    }
}

void ByteArray::append(const char* str, size_t size) {
    append(reinterpret_cast<const byte*>(str), size);
}

void ByteArray::append(const char* str) {
    append(str, std::string(str).length());
}

void ByteArray::reverse() {
    std::reverse(begin(), end());
}

void ByteArray::swap(ByteArray &ba) {
    std::swap(*this, ba);
}

void ByteArray::swap(size_t a, size_t b, size_t size) {
    for(size_t i = 0; i < size; i++) {
        std::swap(this->operator[](a+i), this->operator[](b+i));
    }
}

ByteArray ByteArray::subarr(size_t begin, size_t size) const {
    ByteArray re;
    if(size == -1) size = this->size();
    for(size_t i = begin; i < size; i++) {
        re.push_back(at(i));
    }
    return re;
}

std::string ByteArray::tostdstring() const {
    std::string re;
    for(const byte &b : *this) {
        re += static_cast<char>(b);
    }
    return re;
}

std::stringlist ByteArray::tostringlist(const std::string& split) const {
    return std::stringlist(this->tostdstring(), split);
}

std::string ByteArray::tohex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (const byte &b : *this) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

std::string ByteArray::tohex(size_t begin, size_t size) const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    if (size == -1) size = this->size();
    for (size_t i = begin; i < size; i++) {
        oss << std::setw(2) << static_cast<int>(at(i));
    }
    return oss.str();
}

std::string ByteArray::toEscapedString() const {
    std::stringstream ss;
    for (byte b : *this) {
        if (isprint(b)) ss << b;
        else ss << "\\x" << std::hex << std::setw(2) << (int)b;
    }
    return ss.str();
}

bool ByteArray::operator== (const ByteArray &ba) const {
    if(size() != ba.size()) return false;
    for(size_t i = 0; i < size(); i++) {
        if(at(i) != ba.at(i)) return false;
    }
    return true;
}

ByteArray::ByteArray()
: vector<byte>()
{}

ByteArray::ByteArray(const ByteArray &ba) {
    for (const byte &b : ba)
        push_back(b);
}

ByteArray::ByteArray(const std::string &str) {
    for (const char &c : str)
        push_back(static_cast<byte>(c));
}

ByteArray::ByteArray(const char *raw, size_t size) {
    for (size_t i = 0; i < size; i++)
        push_back(static_cast<byte>(raw[i]));
}

ByteArray ByteArray::fromHex(const std::string& hex) {
    if (hex.size() % 2 != 0) throw std::invalid_argument("[ByteArray::fromHex()] Invalid hex string (odd length)");

    ByteArray result;
    result.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        std::string byteStr = hex.substr(i, 2);
        char* end;
        unsigned long val = strtoul(byteStr.c_str(), &end, 16);
        if (*end != '\0' || val > 0xFF) {
            throw std::invalid_argument("Invalid hex byte: " + byteStr);
        }
        result.push_back(static_cast<byte>(val));
    }
    return result;
}

ByteArray ByteArray::fromRaw(const char* raw, size_t size) {
    return ByteArray(raw, size);
}