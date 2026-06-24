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
#include <cstring>
#include <bit>
#include <cstdint>
#include <initializer_list>
#include <type_traits>
// #include <experimental/scope>

#include "basics.hpp"

#include "basic_api.hpp"
#include "bytearray_api_forward.hpp"

namespace std {

template<typename> class basic_stringlist;
using stringlist = basic_stringlist<char>;
using wstringlist = basic_stringlist<wchar_t>;

class bytearray;

template<typename _T>
concept _is_bytearray_constructable =
    std::is_trivially_copyable_v<std::remove_cvref_t<_T>>
    && !std::is_pointer_v<std::remove_cvref_t<_T>>
    && !std::is_array_v<std::remove_cvref_t<_T>>
    && !std::is_same_v<std::remove_cvref_t<_T>, bytearray>;

// now using std::byte. I'm not really satisfied, because it it so EXPLICIT. There are no any implicit conversions at all.
// Fine. I just need to warn the users to use append(std::byte{0x08}) instead of append(0x08), which is a fucking INTEGER.
// #undef byte
// typedef unsigned char byte;

// For a better fixed-size construction api which will later be added
// into the mainstream.
template<size_t ContentSize>
struct bytes
{
    const size_t content_size = ContentSize;
    std::byte data[ContentSize];
};

// simplify byte literal construction, so that users can write std::byte{0x08} as B(0x08) instead. Still explicit, but less verbose.
// The reason not to use BYTE is because of Microsoft's obnoxious definition of EVERYTHING
#ifndef BYTEARRAY_NODEFINE
    #define B(IN) std::byte{IN}
    #define PCB(IN) reinterpret_cast<const std::byte*>(&IN)
#endif

class bytearray : public vector<byte>
{
    typedef ::std::vector<byte> vector_type;
public:
    bytearray();
    bytearray(const bytearray &ba);
    bytearray(byte b); // Single-Element Constructor is fine to be implicit, since std::byte is safe.
    explicit bytearray(const std::string &str); // note: assumes raw data, does not include length and null terminator
    explicit bytearray(const char *raw, size_t size);
    explicit bytearray(const byte *raw, size_t size);
    explicit bytearray(const void *raw, size_t size);
    explicit bytearray(size_t count, byte value);
    explicit bytearray(size_t count);
    bytearray(std::initializer_list<byte> init);

    template<typename InputIt>
    bytearray(InputIt first, InputIt last) : vector<byte>(first, last) {}

    template<typename _Any>
    requires (_is_bytearray_constructable<_Any>)
    explicit bytearray(const _Any& in)
    {
        const std::byte* src = reinterpret_cast<const std::byte*>(&in);
        vector_type::assign(src, src + sizeof(std::remove_cvref_t<_Any>));
    }

    // Alignment related things are not yet tested. Furthur tests are required before
    // saying that this is safe and works for all trivially copyable types. Use with caution.
    template<typename _T>
    // requires (std::is_aggregate_v<_T>)
    requires (std::is_trivially_copyable_v<_T>)
    _T convert_to() const {
        if (size() != sizeof(_T)) throw std::runtime_error("bytearray::convert_to: type size mismatch");
        if (reinterpret_cast<uintptr_t>(data()) % alignof(_T) != 0)
            throw std::runtime_error("bytearray::convert_to: alignment mismatch");
        return *std::bit_cast<const _T*>(data());
    }

    // another shorter name for convert_to
    template<typename _T>
    requires (std::is_trivially_copyable_v<_T>)
    _T as() const {
        return convert_to<_T>();
    }

    // construct _T from raw data pointer and size, for some STL containers
    // This is for the fixed-size element.
    // Warning: is NOT COMPATIBLE with addContainer(). That one should be used with _view::readContainer() instead.
    template<typename _T>
    requires ( std::is_class_v<_T> && std::is_trivially_copyable_v<typename _T::value_type> )
    _T toContainer() const {
        using _Tp = typename _T::value_type;
        return _T(
            reinterpret_cast<const _Tp*>(this->data()),
            this->size() / sizeof(_Tp)
        );
    }

    void copy_from(const void* raw, size_t size);
    void copy_to(void* raw, size_t size) const;

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
    inline void append(int16_t val)  { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(int16_t)));  }
    inline void append(uint32_t val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(uint32_t))); }
    inline void append(int32_t val)  { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(int32_t)));  }
    inline void append(uint64_t val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(uint64_t))); }
    inline void append(int64_t val)  { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(int64_t)));  }

    inline void append(bool val)     { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(bool)));     }

    // explicit overloads for unsigned long and long (only when distinct from fixed-width types)
    template<typename T>
    requires (std::is_same_v<T, unsigned long> && 
              !std::is_same_v<unsigned long, uint32_t> && 
              !std::is_same_v<unsigned long, uint64_t>)
    inline void append(T val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(unsigned long))); }

    // All arithmetic types will be merged into this template in a future version.
    // template<typename T>
    // requires (std::is_arithmetic_v<T> &&
    //           !std::is_same_v<T, long> && 
    //           !std::is_same_v<T, unsigned long> && 
    //           !std::is_same_v<unsigned long, uint32_t> && 
    //           !std::is_same_v<unsigned long, uint64_t>)
    // inline void append(T val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(T))); }

    template<typename T>
    requires (std::is_same_v<T, long> && 
              !std::is_same_v<long, int32_t> && 
              !std::is_same_v<long, int64_t>)
    inline void append(T val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(long))); }

    // special handle for enum types.
    template<typename E>
    requires std::is_enum_v<E>
    inline void append(E val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(E))); }

    // appending size_t (to avoid ambiguity with everything else)
    inline void appendSize(size_t val) { append(bytearray(reinterpret_cast<const byte*>(&val), sizeof(size_t))); }

    // special one for safe string storage. extract with bytearray_view::readString().
    void addString(const std::string& str);
    void addWString(const std::wstring& wstr);

    // special one for containers of trivially copyable types
    template<::scl2::stl::trivially_copyable_container _T>
    void appendContainer(const _T& in) {
        using _Ty = typename _T::value_type;
        const std::byte* src = reinterpret_cast<const std::byte*>(in.data());

        const size_t count = in.size();
        const size_t elemSize = sizeof(_Ty);

        appendSize(count);
        appendSize(elemSize);
        vector_type::insert(end(), src, src + in.size() * sizeof(_Ty));
    }

    template<typename _T>
    requires (!::scl2::stl::trivially_copyable_container<_T> && ::scl2::has_gdump_container<_T>)
    void appendContainer(const _T& in) {
        using _Ty = typename _T::value_type;
        const std::byte* src = reinterpret_cast<const std::byte*>(in.data());

        const size_t count = in.size();
        // element size is not fixed.

        appendSize(count);

        for(const auto& elem : in) {
            // gdump is responsible for distribution here.
            append(::scl2::gdump(elem));
        }
    }

    void reverse();

    void swap(bytearray &ba);
    void swap(size_t a, size_t b, size_t size = 1);

    std::bytearray& replace(size_t pos, size_t len, const bytearray &ba);
    std::bytearray& insert(size_t pos, const bytearray &ba);

    bytearray subarr(size_t begin, size_t size = -1) const;

    std::string toStdString() const;
    std::stringlist toStringlist(const std::string& split = " ") const;
    std::wstring toStdWString() const;
    std::wstringlist toWStringlist(const std::wstring& split = L" ") const;
    std::string toHex() const;
    std::string toHex(size_t begin, size_t size = -1) const;
    std::string toEscapedString() const;
    std::string xtoEscapedString() const;

    std::u8string toUtf8() const;
    std::u16string toUtf16() const;
    std::u32string toUtf32() const;
#ifndef BYTEARRAY_NO_BASE64
    std::string toBase64() const;
#endif

    bool operator== (const bytearray &ba) const;

    std::bytearray operator+ (const bytearray &ba) const;

    std::bytearray operator << (size_t offset) const;
    std::bytearray operator >> (size_t offset) const;

    std::bytearray shiftLeft(size_t offset) const;
    std::bytearray shiftRight(size_t offset) const;

    std::bytearray rotateLeft(size_t offset) const;
    std::bytearray rotateRight(size_t offset) const;

    static bytearray fromHex(const std::string& hex);
    static bytearray fromRaw(const char* raw, size_t size);
    static bytearray fromRaw(const unsigned char* raw, size_t size);
    static bytearray fromPointer(const void* ptr);

    template<typename _T>
    static bytearray fromPointer(const _T* ptr) {
        return fromPointer(static_cast<const void*>(ptr));
    }

    static bytearray fromStdString(const std::string& str);
    static bytearray fromStdWString(const std::wstring& wstr);

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
    explicit bytearray_view(const bytearray& data);
    bytearray_view(const bytearray&&) = delete; // prevent binding to temporaries

    enable_move_only(bytearray_view) // Disable copy and allow move

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
    requires std::is_trivially_copyable_v<_T>
    _T peek() const {
        size_t orig_cursor = cursor;
        // auto guard = std::scope_exit([&] { this->cursor = orig_cursor; });
        // return read<_T>();

        _T value = read<_T>();
        cursor = orig_cursor;
        return value;
    }

    template<typename _T>
    requires std::is_trivially_copyable_v<_T>
    _T read() const {
        if (!available(sizeof(_T))) 
            throw std::out_of_range("bytearray_view: not enough data");
        return ba.subarr(cursor, sizeof(_T)).convert_to<_T>();
    }

    template<typename _T>
    requires ::scl2::has_generic_load<_T>
    _T read() const {
        // we can't check the size of _T here.
        return ::scl2::generic_load<_T>(*this);
    }

    template<typename _T>
    requires ::scl2::has_generic_load<_T>
    _T peek() const {
        size_t orig_cursor = cursor;
        // auto guard = std::scope_exit([&] { this->cursor = orig_cursor; });
        // return read<_T>();
        
        _T value = read<_T>();
        cursor = orig_cursor;
        return value;
    }

    std::string peekString() const;
    std::string readString() const;

    std::wstring peekWString() const;
    std::wstring readWString() const;

    std::bytearray readBytes(size_t size) const;
    std::bytearray peekBytes(size_t size) const;

    template<typename _T>
    requires ::scl2::stl::trivially_copyable_container<_T>
    _T readContainer() const {
        using _Ty = typename _T::value_type;
        
        if (!available(sizeof(size_t) * 2)) 
            throw std::out_of_range("bytearray_view::peekContainer: not enough data for metadata");
        
        // Create a single temporary view to read both metadata fields sequentially
        size_t count, elemSize;
        count = read<size_t>();
        elemSize = read<size_t>();

        if (elemSize != sizeof(_Ty)) 
            throw std::runtime_error("bytearray_view::peekContainer: element size mismatch");
        
        if (!available(sizeof(size_t) * 2 + count * sizeof(_Ty))) 
            throw std::out_of_range("bytearray_view::peekContainer: not enough data for elements");
        
        // Calculate data start position: cursor + metadata size
        
        _T result;
        result.resize(count);
        std::memcpy(result.data(), ba.data() + cursor, count * sizeof(_Ty));
        cursor += sizeof(size_t) * 2 + count * sizeof(_Ty);
        return result;
    }

    // read for this type will be implemented instead of peek.
    // since by the dynamic size of the element, calculating the size afterwards and maintaining the cursor will be painful.
    // peek is relatively easy since we can just store the original cursor, and restore it after reading.
    // peek is mostly useless anyway.
    template<typename _T>
    requires (!::scl2::stl::trivially_copyable_container<_T> && ::scl2::has_gdump_container<_T>)
    _T readContainer() const {
        if (!available(sizeof(size_t))) 
            throw std::out_of_range("bytearray_view::peekContainer: not enough data for size metadata");
        
        size_t count = read<size_t>();
        // total_size check is not possible here because the element size is not fixed.

        _T result;

        // optimization
        if constexpr (requires(_T& c) { c.reserve(size_t{}); }) {
            result.reserve(count);
        }
        

        for (size_t i = 0; i < count; ++i) {
            // gload is responsible for distribution here.
            ::scl2::stl::universal_insert(result, ::scl2::gload<typename _T::value_type>(*this));
        }

        return result;
    }

    template<typename _T>
    requires ::scl2::stl::trivially_copyable_container<_T>
    _T peekContainer() const {
        size_t orig_cursor = cursor;
        // auto guard = std::scope_exit([&] { this->cursor = orig_cursor; });
        // return readContainer<_T>();

        _T value = readContainer<_T>();
        cursor = orig_cursor;
        return value;
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
    if (file_stream) {
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
