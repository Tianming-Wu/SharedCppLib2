#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cctype>
#include <iomanip>
#include <bit>
#include <cstdint>

namespace std {

template<typename> class basic_stringlist;
using stringlist = basic_stringlist<char>;
using wstringlist = basic_stringlist<wchar_t>;

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
        ::std::vector<std::byte>::insert(end(), src, src + sizeof(_Any));
    }

    template<typename _T>
    // requires (std::is_aggregate_v<_T>)
    _T convert_to() const {
        static_assert(std::is_trivially_copyable_v<_T>, "Type must be trivially copyable");
        if (size() != sizeof(_T)) throw std::runtime_error("bytearray::convert_to: type size mismatch");
        if (reinterpret_cast<uintptr_t>(data()) % alignof(_T) != 0) throw std::runtime_error("bytearray::convert_to: alignment mismatch");
        return *std::bit_cast<const _T*>(data());
    }

    // another shorter name for convert_to
    template<typename _T>
    _T as() const {
        return convert_to<_T>();
    }

    template<typename _T, typename _Tp>
    _T convert_to_constructer() {
        return _T(
        reinterpret_cast<const _Tp*>(this->data()),
        this->size() / sizeof(_Tp)
    );
    }

    inline const byte* rawData() const { return data(); }
    inline size_t rawSize() const { return size(); }

    byte at(size_t i) const;
    byte vat(size_t p, const byte &v = byte('\0')) const;

    void append(const bytearray &ba);
    void append(const byte &b);
    void append(const byte* pb, size_t size);
    void append(const char* str, size_t size);
    void append(const char* str);
    void append(uint8_t val);

    // special one for safe string storage. extract with bytearray_view::readString().
    void addString(const std::string& str);

    void reverse();

    void swap(bytearray &ba);
    void swap(size_t a, size_t b, size_t size = 1);

    std::bytearray& replace(size_t pos, size_t len, const bytearray &ba);
    std::bytearray& insert(size_t pos, const bytearray &ba);

    bytearray subarr(size_t begin, size_t size = -1) const;

    std::string tostdstring() const;
    std::stringlist tostringlist(const std::string& split = " ") const;
    std::wstring tostdwstring() const;
    std::wstringlist towstringlist(const std::wstring& split = L" ") const;
    std::string tohex() const;
    std::string tohex(size_t begin, size_t size = -1) const;
    std::string toEscapedString() const;
    std::string xtoEscapedString() const;

    std::u8string toUtf8() const;
    std::u16string toUtf16() const;
    std::u32string toUtf32() const;
#ifndef BYTEARRAY_NO_BASE64
    std::string toBase64() const;
#endif

    bool operator== (const bytearray &ba) const;

    std::bytearray operator << (size_t offset) const;
    std::bytearray operator >> (size_t offset) const;

    std::bytearray shiftLeft(size_t offset) const;
    std::bytearray shiftRight(size_t offset) const;

    std::bytearray rotateLeft(size_t offset) const;
    std::bytearray rotateRight(size_t offset) const;

    static bytearray fromHex(const std::string& hex);
    static bytearray fromRaw(const char* raw, size_t size);

    static bytearray fromUtf8(const std::u8string& utf8str);
    static bytearray fromUtf16(const std::u16string& utf16str);
    static bytearray fromUtf32(const std::u32string& utf32str);
#ifndef BYTEARRAY_NO_BASE64
    static bytearray fromBase64(const std::string& base64str);
#endif

    bool readFromStream(std::istream& is, size_t size);
    bool readAllFromStream(std::istream& is);
    bool readUntilDelimiter(std::istream& is, char delimiter = '\0');

    inline void writeRaw(std::ostream& os) const {
        os.write(reinterpret_cast<const char*>(data()), size());
    }

};

class bytearray_view
{
public:
    bytearray_view(const bytearray& data);

    // 穿透底层 bytearray 的信息
    inline size_t size() const { return ba.size(); }
    inline bool empty() const { return ba.empty(); }
    inline const byte* data() const { return ba.data(); }
    inline byte at(size_t i) const { return ba.at(i); }
    inline byte operator[](size_t i) const { return ba[i]; }
    inline bytearray subarr(size_t begin, size_t size = -1) const { 
        return ba.subarr(begin, size); 
    }

    // 游标操作
    bool available(size_t bytes) const;
    size_t remaining() const;
    void seek(size_t pos);
    void reset();
    size_t tell() const;

    template<typename _T>
    _T peek() const {
        static_assert(std::is_trivially_copyable_v<_T>);
        if (!available(sizeof(_T))) 
            throw std::out_of_range("bytearray_view: not enough data");
        _T result = ba.subarr(cursor, sizeof(_T)).convert_to<_T>();
        // cursor += sizeof(_T); // peek does not progress
        return result;
    }

    template<typename _T>
    _T read() const {
        cursor += sizeof(_T);
        return peek<_T>();
    }

    std::string peekString() const;
    std::string readString() const;

protected:
    mutable size_t cursor = 0;
    const bytearray& ba;
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
    
    // check if is file stream and in binary mode
    auto* file_stream = dynamic_cast<std::istream*>(&is);
    if (file_stream && (file_stream->flags() & std::ios::binary)) {
        // read the entire file content
        file_stream->seekg(0, std::ios::end);
        auto size = file_stream->tellg();
        file_stream->seekg(0, std::ios::beg);
        
        if (size > 0) {
            ba.resize(size);
            file_stream->read(reinterpret_cast<char*>(ba.data()), size);
        }
    }
    else if (is.flags() & std::ios_base::hex) {
        // hex text mode
        std::string hexInput;
        is >> hexInput;
        try {
            ba = bytearray::fromHex(hexInput);
        } catch (...) {
            is.setstate(std::ios::failbit);
        }
    } else {
        // normal text mode: read a "word"
        std::string strInput;
        is >> strInput;
        ba = bytearray(strInput);
    }
    return is;
}

} // namespace std
