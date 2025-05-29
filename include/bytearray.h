#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cctype>
#include <iomanip>

namespace std {
class stringlist;
}

typedef unsigned char byte;

class ByteArray : public std::vector<byte>
{
public:

    using vector<byte>::vector;
    // ByteArray(ByteArray&) = default;
    // ByteArray(ByteArray&&) = default;

    ByteArray();
    ByteArray(const ByteArray &ba);
    ByteArray(const std::string &str);
    ByteArray(const char *raw, size_t size);

    inline const byte* rawData() const { return data(); }
    inline size_t rawSize() const { return size(); }

    byte at(size_t i) const;

    byte vat(size_t p, const byte &v = 0) const;

    void append(const ByteArray &ba);

    void append(const byte &b);

    void append(const byte* pb, size_t size);

    void append(const char* str, size_t size);
    void append(const char* str);

    void reverse();

    void swap(ByteArray &ba);

    void swap(size_t a, size_t b, size_t size = 1);

    ByteArray subarr(size_t begin, size_t size = -1) const;

    std::string tostdstring() const;

    std::stringlist tostringlist(const std::string& split = " ") const;

    std::string tohex() const;

    std::string tohex(size_t begin, size_t size = -1) const;

    std::string toEscapedString() const;

    bool operator== (const ByteArray &ba) const;

    static ByteArray fromHex(const std::string& hex);

    static ByteArray fromRaw(const char* raw, size_t size);

    inline void writeRaw(std::ostream& os) const {
        os.write(reinterpret_cast<const char*>(data()), size());
    }

};

inline std::ostream& operator<<(std::ostream& os, const ByteArray& ba) {
    if (os.flags() & std::ios_base::hex) {
        return os << ba.tohex();
    }
    return os << ba.tostdstring();
}

inline std::istream& operator>>(std::istream& is, ByteArray& ba) {
    ba.clear();
    if (is.flags() & std::ios_base::hex) {
        std::string hexInput;
        is >> hexInput;
        try {
            ba = ByteArray::fromHex(hexInput);
        } catch (...) {
            is.setstate(std::ios::failbit);
        }
    } else {
        std::string strInput;
        is >> strInput;
        ba = ByteArray(strInput);
    }
    return is;
}