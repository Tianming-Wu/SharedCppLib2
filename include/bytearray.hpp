/*
    A byte array class for handling raw binary data.
    classes:
        std::bytearray, std::bytearray_view
    link target:
        SharedCppLib2::basic
*/
#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cctype>
#include <iomanip>
#include <bit>
#include <cstdint>
#include <initializer_list>

namespace std {

template<typename> class basic_stringlist;
using stringlist = basic_stringlist<char>;
using wstringlist = basic_stringlist<wchar_t>;

// now using std::byte. I'm not really satisfied, because it it so EXPLICIT. There are no any implicit conversions at all.
// Fine. I just need to warn the users to use append(std::byte{0x08}) instead of append(0x08), which is a fucking INTEGER.
// #undef byte
// typedef unsigned char byte;

class bytearray : public vector<byte>
{
public:
    bytearray();
    bytearray(const bytearray &ba);
    explicit bytearray(byte b);
    explicit bytearray(const std::string &str); // note: assumes raw data, does not include length and null terminator
    explicit bytearray(const char *raw, size_t size);
    explicit bytearray(const byte *raw, size_t size);
    explicit bytearray(size_t count, byte value);
    bytearray(std::initializer_list<byte> init);

    template<typename InputIt>
    bytearray(InputIt first, InputIt last) : vector<byte>(first, last) {}

    template<typename _Any>
    requires (std::is_trivially_copyable_v<_Any>)
    explicit bytearray(const _Any& in) {
        const std::byte* src = reinterpret_cast<const std::byte*>(&in);
        ::std::vector<std::byte>::insert(end(), src, src + sizeof(_Any));
    }

    template<typename _T>
    // requires (std::is_aggregate_v<_T>)
    requires (std::is_trivially_copyable_v<_T>)
    _T convert_to() const {
        if (size() != sizeof(_T)) throw std::runtime_error("bytearray::convert_to: type size mismatch");
        if (reinterpret_cast<uintptr_t>(data()) % alignof(_T) != 0) throw std::runtime_error("bytearray::convert_to: alignment mismatch");
        return *std::bit_cast<const _T*>(data());
    }

    // another shorter name for convert_to
    template<typename _T>
    requires (std::is_trivially_copyable_v<_T>)
    _T as() const {
        return convert_to<_T>();
    }

    // construct _T from raw data pointer and size, for some STL containers
    template<typename _T>
    requires ( std::is_class_v<_T> && std::is_trivially_copyable_v<typename _T::value_type> )
    _T convert_to_constructer() {
        using _Tp = typename _T::value_type;
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
    void append(byte b);
    void append(const byte* pb, size_t size);
    void append(const char* str, size_t size);
    void append(const char* str); // this is a little dangerous, be careful! it follows the null terminator.
    void append(uint8_t val);
    inline void append(int8_t val) { append(static_cast<uint8_t>(val)); }

    // fuck you std standard for no explicit parameters
    inline void append(uint16_t val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(uint16_t))); }
    inline void append(int16_t val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(int16_t))); }
    inline void append(uint32_t val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(uint32_t))); }
    inline void append(int32_t val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(int32_t))); }
    inline void append(uint64_t val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(uint64_t))); }
    inline void append(int64_t val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(int64_t))); }

    // explicit overloads for unsigned long and long (only when distinct from fixed-width types)
    template<typename T>
    requires (std::is_same_v<T, unsigned long> && 
              !std::is_same_v<unsigned long, uint32_t> && 
              !std::is_same_v<unsigned long, uint64_t>)
    inline void append(T val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(unsigned long))); }

    template<typename T>
    requires (std::is_same_v<T, long> && 
              !std::is_same_v<long, int32_t> && 
              !std::is_same_v<long, int64_t>)
    inline void append(T val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(long))); }

    inline void appendSize(size_t val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(size_t))); }

    // special one for safe string storage. extract with bytearray_view::readString().
    void addString(const std::string& str);

    // special one for containers of trivially copyable types
    template<typename _T>
    requires (std::is_trivially_copyable_v<typename _T::value_type>)
    void appendContainer(const _T& in) {
        using _Ty = typename _T::value_type;
        const std::byte* src = reinterpret_cast<const std::byte*>(in.data());

        const size_t count = in.size();
        const size_t elemSize = sizeof(_Ty);

        appendSize(count);
        appendSize(elemSize);
        ::std::vector<std::byte>::insert(end(), src, src + in.size() * sizeof(_Ty));
    }

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
    static bytearray fromRaw(const unsigned char* raw, size_t size);

    static bytearray fromUtf8(const std::u8string& utf8str);
    static bytearray fromUtf16(const std::u16string& utf16str);
    static bytearray fromUtf32(const std::u32string& utf32str);
#ifndef BYTEARRAY_NO_BASE64
    static bytearray fromBase64(const std::string& base64str);
#endif

    bool readFromStream(std::istream& is, size_t size);
    bool readAllFromStream(std::istream& is);
    bool readUntilDelimiter(std::istream& is, char delimiter = '\0');
    // bool readFromStreamAt(std::istream& is, size_t position, size_t size);
    // bool readFromStreamAtEnd(std::istream& is, size_t size);

    inline void writeRaw(std::ostream& os) const {
        os.write(reinterpret_cast<const char*>(data()), size());
    }

};

class bytearray_view
{
public:
    bytearray_view(const bytearray& data);
    bytearray_view(const bytearray&&) = delete; // prevent binding to temporaries

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
        _T result = peek<_T>();
        cursor += sizeof(_T);
        return result;
    }

    std::string peekString() const;
    std::string readString() const;

    std::bytearray readBytes(size_t size) const;
    std::bytearray peekBytes(size_t size) const;

    template<typename _T>
    requires (std::is_trivially_copyable_v<typename _T::value_type>)
    _T peekContainer() const {
        using _Ty = typename _T::value_type;
        
        if (!available(sizeof(size_t) * 2)) 
            throw std::out_of_range("bytearray_view::peekContainer: not enough data for metadata");
        
        // Create a single temporary view to read both metadata fields sequentially
        size_t count, elemSize;
        {
            bytearray temp = ba.subarr(cursor);
            bytearray_view temp_view(temp);
            count = temp_view.read<size_t>();
            elemSize = temp_view.read<size_t>();
        }

        if (elemSize != sizeof(_Ty)) 
            throw std::runtime_error("bytearray_view::peekContainer: element size mismatch");
        
        if (!available(sizeof(size_t) * 2 + count * sizeof(_Ty))) 
            throw std::out_of_range("bytearray_view::peekContainer: not enough data for elements");
        
        // Calculate data start position: cursor + metadata size
        const std::byte* data_start = ba.data() + cursor + sizeof(size_t) * 2;
        
        _T result;
        result.resize(count);
        std::memcpy(result.data(), data_start, count * sizeof(_Ty));
        return result;
    }

    template<typename _T>
    requires (std::is_trivially_copyable_v<typename _T::value_type>)
    _T readContainer() const {
        _T result = peekContainer<_T>();
        cursor += sizeof(size_t) * 2 + result.size() * sizeof(typename _T::value_type);
        return result;
    }

protected:
    mutable size_t cursor = 0;
    const bytearray& ba;
};


// These operators avoid any unwanted format used. It by default deal with raw data only.
// If user really need something like hex, they must do it explicitly.

inline ostream& operator<<(ostream& os, const std::bytearray& ba) {
    ba.writeRaw(os);
    return os;
}

inline istream& operator>>(istream& is, std::bytearray& ba) {
    ba.clear();
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
    } else {
        is.setstate(std::ios::failbit);
    }
    return is;
}

// inline std::ostream& operator<<(std::ostream& os, const bytearray& ba) {
//     if (os.flags() & std::ios_base::hex) {
//         return os << ba.tohex();
//     }
//     return os << ba.tostdstring();
// }


// inline ostream& operator<<(ostream& os, const std::bytearray& ba) {
//     if (os.flags() & ios_base::hex) {
//         ios_base::fmtflags original_flags = os.flags();
//         os << hex << setfill('0');
//         for (const auto& b : ba) {
//             os << setw(2) << static_cast<int>(b);
//         }
//         os.flags(original_flags);
//     } else {
//         os << ba.tostdstring();
//     }
//     return os;
// }

// inline std::istream& operator>>(std::istream& is, bytearray& ba) {
//     ba.clear();
    
//     // check if is file stream and in binary mode
//     auto* file_stream = dynamic_cast<std::istream*>(&is);
//     if (file_stream && (file_stream->flags() & std::ios::binary)) {
//         // read the entire file content
//         file_stream->seekg(0, std::ios::end);
//         auto size = file_stream->tellg();
//         file_stream->seekg(0, std::ios::beg);
        
//         if (size > 0) {
//             ba.resize(size);
//             file_stream->read(reinterpret_cast<char*>(ba.data()), size);
//         }
//     }
//     else if (is.flags() & std::ios_base::hex) {
//         // hex text mode
//         std::string hexInput;
//         is >> hexInput;
//         try {
//             ba = bytearray::fromHex(hexInput);
//         } catch (...) {
//             is.setstate(std::ios::failbit);
//         }
//     } else {
//         // normal text mode: read a "word"
//         std::string strInput;
//         is >> strInput;
//         ba = bytearray(strInput);
//     }
//     return is;
// }

} // namespace std
