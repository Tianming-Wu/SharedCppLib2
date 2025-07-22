#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cctype>
#include <iomanip>
#include <bit>

namespace std {
class stringlist;

// #undef byte
// typedef unsigned char byte;

class bytearray : public vector<byte>
{
public:

    using vector<byte>::vector;
    // bytearray(bytearray&) = default;
    // bytearray(bytearray&&) = default;

    bytearray();
    bytearray(const bytearray &ba);
    bytearray(const std::string &str);
    bytearray(const char *raw, size_t size);

    template<typename _Any>
    bytearray(const _Any& in) {
        static_assert(std::is_trivially_copyable_v<_Any>,  "bytearray: T must be trivially copyable");
        const std::byte* src = reinterpret_cast<const std::byte*>(&in);
        insert(end(), src, src + sizeof(_Any));
    }


    template<typename _T>
    requires (std::is_aggregate_v<_T>)
    _T convert_to() const {
        static_assert(std::is_trivially_copyable_v<_T>, "Type must be trivially copyable");
        if (size() != sizeof(_T)) throw std::runtime_error("bytearray::convert_to: type size mismatch");
        if (reinterpret_cast<uintptr_t>(data()) % alignof(_T) != 0) throw std::runtime_error("bytearray::convert_to: alignment mismatch");
        return *std::bit_cast<const _T*>(data());
    }

    inline const byte* rawData() const { return data(); }
    inline size_t rawSize() const { return size(); }

    byte at(size_t i) const;
    byte vat(size_t p, const byte &v = byte('0')) const;

    void append(const bytearray &ba);
    void append(const byte &b);
    void append(const byte* pb, size_t size);
    void append(const char* str, size_t size);
    void append(const char* str);

    void reverse();

    void swap(bytearray &ba);
    void swap(size_t a, size_t b, size_t size = 1);

    bytearray subarr(size_t begin, size_t size = -1) const;

    std::string tostdstring() const;
    std::stringlist tostringlist(const std::string& split = " ") const;
    std::string tohex() const;
    std::string tohex(size_t begin, size_t size = -1) const;
    std::string toEscapedString() const;

    bool operator== (const bytearray &ba) const;

    static bytearray fromHex(const std::string& hex);
    static bytearray fromRaw(const char* raw, size_t size);

    inline void writeRaw(std::ostream& os) const {
        os.write(reinterpret_cast<const char*>(data()), size());
    }

};

// inline std::ostream& operator<<(std::ostream& os, const bytearray& ba) {
//     if (os.flags() & std::ios_base::hex) {
//         return os << ba.tohex();
//     }
//     return os << ba.tostdstring();
// }


inline ostream& operator<<(ostream& os, const std::bytearray& ba) {
    if (os.flags() & ios_base::hex) {
        ios_base::fmtflags original_flags = os.flags();
        os << hex << setfill('0');
        for (const auto& b : ba) {
            os << setw(2) << static_cast<int>(b);
        }
        os.flags(original_flags);
    } else {
        os << ba.tostdstring();
    }
    return os;
}


inline std::istream& operator>>(std::istream& is, bytearray& ba) {
    ba.clear();
    if (is.flags() & std::ios_base::hex) {
        std::string hexInput;
        is >> hexInput;
        try {
            ba = bytearray::fromHex(hexInput);
        } catch (...) {
            is.setstate(std::ios::failbit);
        }
    } else {
        std::string strInput;
        is >> strInput;
        ba = bytearray(strInput);
    }
    return is;
}

} // namespace std