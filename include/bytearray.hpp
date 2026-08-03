/*
    A byte array class for handling raw binary data.
    classes:
        scl2::bytearray, scl2::bytearray_view
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

namespace scl2 {

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
// #undef std::byte
// typedef unsigned char std::byte;

// For a better fixed-size construction api which will later be added
// into the mainstream.
template<size_t ContentSize>
struct bytes
{
    const size_t content_size = ContentSize;
    std::byte data[ContentSize];
};

// simplify std::byte literal construction, so that users can write std::byte{0x08} as B(0x08) instead. Still explicit, but less verbose.
// The reason not to use std::byte is because of Microsoft's obnoxious definition of EVERYTHING
#ifndef BYTEARRAY_NODEFINE
    #define B(IN) std::byte{IN}
    #define PCB(IN) reinterpret_cast<const std::byte*>(&IN)
#endif

class bytearray : private std::vector<std::byte>
{
    using base_type = std::vector<std::byte>;
public:
    // ── Construction ──────────────────────────────────────────────────
    bytearray() = default;
    bytearray(const bytearray&) = default;
    bytearray(bytearray&&) noexcept = default;
    bytearray& operator=(const bytearray&) = default;
    bytearray& operator=(bytearray&&) noexcept = default;

    bytearray(std::byte b);                                   // single byte
    explicit bytearray(const std::string& str);               // raw data, no terminator
    explicit bytearray(const char* raw, size_t size);
    explicit bytearray(const std::byte* raw, size_t size);
    explicit bytearray(const void* raw, size_t size);
    explicit bytearray(size_t count, std::byte value);
    explicit bytearray(size_t count);                         // zeroed
    bytearray(std::initializer_list<std::byte> init);

    template<typename InputIt>
    bytearray(InputIt first, InputIt last) : base_type(first, last) {}

    template<typename _Any>
    requires (_is_bytearray_constructable<_Any>)
    explicit bytearray(const _Any& in) {
        const std::byte* src = reinterpret_cast<const std::byte*>(&in);
        base_type::assign(reinterpret_cast<const std::byte*>(src),
                          reinterpret_cast<const std::byte*>(src) + sizeof(_Any));
    }

    // ── Write: position-based ────────────────────────────────────────
    void insert(size_t pos, const bytearray& data) { base_type::insert(begin() + pos, data.begin(), data.end()); }
    void insert(size_t pos, const std::byte* data, size_t len) {
        base_type::insert(begin() + pos, reinterpret_cast<const std::byte*>(data),
                          reinterpret_cast<const std::byte*>(data) + len);
    }
    void insert(size_t pos, std::byte b) { insert(pos, &b, 1); }

    template<typename T>
    requires std::is_trivially_copyable_v<T>
    void insert(size_t pos, const T& data) {
        base_type::insert(begin() + pos, reinterpret_cast<const std::byte*>(&data),
                          reinterpret_cast<const std::byte*>(&data) + sizeof(T));
    }

    // ── Write: at write_pointer ──────────────────────────────────────
    void insert(const bytearray& data) { insert(write_pointer, data); }
    void insert(const std::byte* data, size_t len) { insert(write_pointer, data, len); }
    void insert(std::byte b) { insert(write_pointer, b); }

    template<typename T>
    requires std::is_trivially_copyable_v<T>
    void insert(const T& data) { insert(write_pointer, data); }

    // ── Write: append (at end) ───────────────────────────────────────
    void append(const bytearray& data) { insert(size(), data); }
    void append(const std::byte* data, size_t len) { insert(size(), data, len); }
    void append(std::byte b) { insert(size(), b); }

    template<typename T>
    requires std::is_trivially_copyable_v<T>
    void append(const T& data) { insert(size(), data); }

    // ── String write (uint32_t length-prefixed) ──────────────────────
    void insert(size_t pos, const std::string& str) {
        uint32_t len = static_cast<uint32_t>(str.size());
        insert<uint32_t>(pos, len);
        insert(pos + sizeof(len), reinterpret_cast<const std::byte*>(str.data()), str.size());
    }
    void insert(const std::string& str) { insert(write_pointer, str); }
    void append(const std::string& str) { insert(size(), str); }

    void insert(size_t pos, const std::wstring& str) {
        uint32_t len = static_cast<uint32_t>(str.size());
        insert<uint32_t>(pos, len);
        insert(pos + sizeof(len), reinterpret_cast<const std::byte*>(str.data()), str.size() * sizeof(wchar_t));
    }
    void insert(const std::wstring& str) { insert(write_pointer, str); }
    void append(const std::wstring& str) { insert(size(), str); }

    // ── Raw string write (no length prefix) ──────────────────────────
    void insertRawString(size_t pos, const std::string& str) {
        insert(pos, reinterpret_cast<const std::byte*>(str.data()), str.size());
    }
    void insertRawString(const std::string& str) { insertRawString(write_pointer, str); }
    void appendRawString(const std::string& str) { insertRawString(size(), str); }

    void insertRawWString(size_t pos, const std::wstring& str) {
        insert(pos, reinterpret_cast<const std::byte*>(str.data()), str.size() * sizeof(wchar_t));
    }
    void insertRawWString(const std::wstring& str) { insertRawWString(write_pointer, str); }
    void appendRawWString(const std::wstring& str) { insertRawWString(size(), str); }

    // ── Container write (trivially copyable elements) ────────────────
    template<typename ContainerType>
    requires requires { typename ContainerType::value_type; }
          && std::is_trivially_copyable_v<typename ContainerType::value_type>
    void insertContainer(size_t pos, const ContainerType& container) {
        insert<uint32_t>(pos, static_cast<uint32_t>(container.size()));
        insert<uint32_t>(pos + sizeof(uint32_t), static_cast<uint32_t>(sizeof(typename ContainerType::value_type)));
        insert(pos + 2 * sizeof(uint32_t),
               reinterpret_cast<const std::byte*>(container.data()),
               container.size() * sizeof(typename ContainerType::value_type));
    }

    // ── Container write (gdump elements) ─────────────────────────────
    template<typename _T>
    requires (!::scl2::stl::trivially_copyable_container<_T> && ::scl2::has_gdump_container<_T>)
    void appendContainer(const _T& in) {
        append<uint32_t>(static_cast<uint32_t>(in.size()));
        for (const auto& elem : in) {
            append(::scl2::gdump(elem));
        }
    }

    // ── Byte-level convenience ───────────────────────────────────────
    void insertByte(size_t pos, uint8_t byte) { insert(pos, static_cast<std::byte>(byte)); }
    void insertByte(uint8_t byte) { insert(write_pointer, static_cast<std::byte>(byte)); }
    void appendByte(uint8_t byte) { append(static_cast<std::byte>(byte)); }

    // ── Read (cursor-based) ──────────────────────────────────────────
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    T read() const {
        if (!available<T>()) throw std::out_of_range("bytearray::read: not enough data");
        T data;
        std::memcpy(&data, this->data() + read_pointer, sizeof(T));
        read_pointer += sizeof(T);
        return data;
    }

    // For generic_load types
    template<typename T>
    requires ::scl2::has_generic_load<T>
    T read() const {
        return ::scl2::generic_load<T>(*this);
    }

    // Mutable reference at cursor (unsafe: caller must ensure lifetime)
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    T& getref() {
        if (!available<T>()) throw std::out_of_range("bytearray::getref: not enough data");
        T& ref = *reinterpret_cast<T*>(this->data() + read_pointer);
        read_pointer += sizeof(T);
        return ref;
    }

    std::string readString() const {
        if (!available<uint32_t>()) throw std::out_of_range("bytearray::readString: not enough data for length");
        uint32_t length = read<uint32_t>();
        if (length == 0) return {};
        if (!bytesAvailable(length)) throw std::out_of_range("bytearray::readString: not enough data");
        std::string str(length, '\0');
        std::memcpy(&str[0], this->data() + read_pointer, length);
        read_pointer += length;
        return str;
    }

    std::wstring readWString() const {
        if (!available<uint32_t>()) throw std::out_of_range("bytearray::readWString: not enough data for length");
        uint32_t length = read<uint32_t>();
        if (length == 0) return {};
        if (!bytesAvailable(length * sizeof(wchar_t))) throw std::out_of_range("bytearray::readWString: not enough data");
        std::wstring str(length, L'\0');
        std::memcpy(&str[0], this->data() + read_pointer, length * sizeof(wchar_t));
        read_pointer += length * sizeof(wchar_t);
        return str;
    }

    std::string readRawString(size_t const charCount) const {
        if (charCount == 0) return {};
        if (!bytesAvailable(charCount)) throw std::out_of_range("bytearray::readRawString: not enough data");
        std::string str(charCount, '\0');
        std::memcpy(&str[0], this->data() + read_pointer, charCount);
        read_pointer += charCount;
        return str;
    }

    std::wstring readRawWString(size_t const charCount) const {
        if (charCount == 0) return {};
        if (!bytesAvailable(charCount * sizeof(wchar_t))) throw std::out_of_range("bytearray::readRawWString: not enough data");
        std::wstring str(charCount, L'\0');
        std::memcpy(&str[0], this->data() + read_pointer, charCount * sizeof(wchar_t));
        read_pointer += charCount * sizeof(wchar_t);
        return str;
    }

    bytearray readBytes(size_t const length) const {
        if (!bytesAvailable(length)) throw std::out_of_range("bytearray::readBytes: not enough data");
        bytearray result(reinterpret_cast<const std::byte*>(this->data() + read_pointer), length);
        read_pointer += length;
        return result;
    }

    // Container read (trivially copyable elements)
    template<typename ContainerType>
    requires requires { typename ContainerType::value_type; }
          && std::is_trivially_copyable_v<typename ContainerType::value_type>
    ContainerType readContainer() const {
        if (!available<uint32_t>()) throw std::out_of_range("bytearray::readContainer: not enough data for count");
        uint32_t count = read<uint32_t>();
        if (count == 0) return {};
        if (!available<uint32_t>()) throw std::out_of_range("bytearray::readContainer: not enough data for elem size");
        uint32_t elemSize = read<uint32_t>();
        if (elemSize != sizeof(typename ContainerType::value_type))
            throw std::runtime_error("bytearray::readContainer: element size mismatch");
        if (!bytesAvailable(count * elemSize)) throw std::out_of_range("bytearray::readContainer: not enough data");
        ContainerType container(count);
        std::memcpy(container.data(), this->data() + read_pointer, count * elemSize);
        read_pointer += count * elemSize;
        return container;
    }

    // Container read (gdump elements)
    template<typename _T>
    requires (!::scl2::stl::trivially_copyable_container<_T> && ::scl2::has_gdump_container<_T>)
    _T readContainer() {
        if (!available<uint32_t>()) throw std::out_of_range("bytearray::readContainer: not enough data for count");
        uint32_t count = read<uint32_t>();
        _T result;
        if constexpr (requires(_T& c) { c.reserve(size_t{}); }) result.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            ::scl2::stl::universal_insert(result, ::scl2::gload<typename _T::value_type>(*this));
        }
        return result;
    }

    // ── Cursor management ────────────────────────────────────────────
    static constexpr size_t seek_end = static_cast<size_t>(-1);

    void seekr(size_t pos) const {
        if (pos == seek_end) pos = size();
        if (pos > size()) throw std::out_of_range("bytearray::seekr: out of range");
        read_pointer = pos;
    }
    void seekw(size_t pos) {
        if (pos == seek_end) pos = size();
        if (pos > size()) throw std::out_of_range("bytearray::seekw: out of range");
        write_pointer = pos;
    }
    size_t tellr() const { return read_pointer; }
    size_t tellw() const { return write_pointer; }

    // ── Query ────────────────────────────────────────────────────────
    size_t size() const { return base_type::size(); }
    bool empty() const { return base_type::empty(); }
    void clear() { base_type::clear(); write_pointer = 0; read_pointer = 0; }

    const std::byte* data() const { return base_type::data(); }
    std::byte* data() { return base_type::data(); }

    std::byte at(size_t i) const { return base_type::at(i); }
    std::byte vat(size_t p, const std::byte& v = std::byte{0}) const {
        return p < size() ? at(p) : v;
    }

    // ── Element access (vector forwarding) ───────────────────────────
    std::byte& operator[](size_t i) { return base_type::operator[](i); }
    const std::byte& operator[](size_t i) const { return base_type::operator[](i); }
    std::byte& back() { return base_type::back(); }
    const std::byte& back() const { return base_type::back(); }
    std::byte& front() { return base_type::front(); }
    const std::byte& front() const { return base_type::front(); }

    void push_back(std::byte b) { base_type::push_back(b); }
    void resize(size_t n) { base_type::resize(n); }
    void resize(size_t n, std::byte v) { base_type::resize(n, v); }
    void reserve(size_t n) { base_type::reserve(n); }

    // ── Iterators ────────────────────────────────────────────────────
    using base_type::begin;
    using base_type::end;
    using base_type::cbegin;
    using base_type::cend;

    bool bytesAvailable(size_t length) const { return read_pointer + length <= size(); }
    size_t remaining() const { return size() > read_pointer ? size() - read_pointer : 0; }

    template<typename T>
    requires std::is_trivially_copyable_v<T>
    bool available() const { return bytesAvailable(sizeof(T)); }

    template<typename T>
    requires std::is_trivially_copyable_v<T>
    bool fits() const { return size() == sizeof(T); }

    void copy_from(const void* raw, size_t size);
    void copy_to(void* raw, size_t size) const;

    // ── Conversion (whole-content) ───────────────────────────────────
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    const T& as() const {
        if (!fits<T>()) throw std::out_of_range("bytearray::as: size mismatch");
        return *reinterpret_cast<const T*>(this->data());
    }

    template<typename T>
    requires std::is_trivially_copyable_v<T>
    T& as() {
        if (!fits<T>()) throw std::out_of_range("bytearray::as: size mismatch");
        return *reinterpret_cast<T*>(this->data());
    }

    template<typename T>
    requires std::is_trivially_copyable_v<T>
    T to() const {
        if (size() != sizeof(T)) throw std::runtime_error("bytearray::to: size mismatch");
        if (reinterpret_cast<uintptr_t>(data()) % alignof(T) != 0)
            throw std::runtime_error("bytearray::to: alignment mismatch");
        return *std::bit_cast<const T*>(data());
    }

    template<typename _T>
    requires (std::is_class_v<_T> && std::is_trivially_copyable_v<typename _T::value_type>)
    _T toContainer() const {
        using _Tp = typename _T::value_type;
        return _T(reinterpret_cast<const _Tp*>(this->data()), this->size() / sizeof(_Tp));
    }

    std::string toString() const;        // length-prefixed format
    std::wstring toWString() const;

    std::string toStdString() const;     // raw bytes as string
    std::wstring toStdWString() const;

    stringlist toStringlist(const std::string& split = " ") const;
    wstringlist toWStringlist(const std::wstring& split = L" ") const;

    std::string toHex() const;
    std::string toHex(size_t begin, size_t size = seek_end) const;
    std::string toEscapedString() const;
    std::string xtoEscapedString() const;

    std::u8string toUtf8() const;
    std::u16string toUtf16() const;
    std::u32string toUtf32() const;

    std::string toBase64() const;

    // ── Manipulation ─────────────────────────────────────────────────
    void reverse();
    void swap(bytearray& other);
    void swap(size_t a, size_t b, size_t len = 1);

    bytearray& replace(size_t pos, size_t len, const bytearray& data);
    bytearray& erase(size_t pos, size_t len);

    bytearray subarr(size_t begin, size_t n = seek_end) const;

    bytearray shiftLeft(size_t offset) const;
    bytearray shiftRight(size_t offset) const;
    bytearray rotateLeft(size_t offset) const;
    bytearray rotateRight(size_t offset) const;

    // ── Operators ────────────────────────────────────────────────────
    bool operator==(const bytearray& other) const;
    bool operator!=(const bytearray& other) const { return !(*this == other); }
    bytearray operator+(const bytearray& other) const;

    // ── Stream I/O ───────────────────────────────────────────────────
    bool readFromStream(std::istream& is, size_t size);
    bool readAllFromStream(std::istream& is);
    bool readUntilDelimiter(std::istream& is, char delimiter = '\0');
    void writeRaw(std::ostream& os) const {
        os.write(reinterpret_cast<const char*>(data()), static_cast<std::streamsize>(size()));
    }

    // ── Static factories ─────────────────────────────────────────────
    static bytearray fromTriviallyCopyable(const auto& data) { bytearray ba; ba.append(data); return ba; }

    static bytearray fromString(const std::string& str);       // length-prefixed
    static bytearray fromWString(const std::wstring& str);

    static bytearray fromStdString(const std::string& str);    // raw
    static bytearray fromStdWString(const std::wstring& str);

    static bytearray fromHex(const std::string& hex);
    static bytearray fromBase64(const std::string& base64);

    static bytearray fromRaw(const char* raw, size_t size);
    static bytearray fromRaw(const unsigned char* raw, size_t size);
    static bytearray fromPointer(const void* ptr);

    template<typename _T>
    static bytearray fromPointer(const _T* ptr) { return fromPointer(static_cast<const void*>(ptr)); }

    static bytearray fromUtf8(const std::u8string& utf8);
    static bytearray fromUtf16(const std::u16string& utf16);
    static bytearray fromUtf32(const std::u32string& utf32);

private:
    size_t write_pointer = 0;
    mutable size_t read_pointer = 0;
};

// ── bytearray_view (non-owning span, like string_view) ───────────────

class bytearray_view {
public:
    bytearray_view() : data_(nullptr), size_(0) {}
    bytearray_view(const bytearray& ba) : data_(ba.data()), size_(ba.size()) {}
    bytearray_view(const std::byte* data, size_t size) : data_(data), size_(size) {}
    bytearray_view(const bytearray&&) = delete;  // prevent dangling

    const std::byte* data() const { return data_; }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    std::byte operator[](size_t i) const { return data_[i]; }
    std::byte at(size_t i) const {
        if (i >= size_) throw std::out_of_range("bytearray_view::at: out of range");
        return data_[i];
    }

    bytearray subarr(size_t begin, size_t n = bytearray::seek_end) const;

    bool operator==(const bytearray_view& other) const;
    bool operator!=(const bytearray_view& other) const { return !(*this == other); }

private:
    const std::byte* data_;
    size_t size_;
};

// ── Stream operators ─────────────────────────────────────────────────

inline std::ostream& operator<<(std::ostream& os, const bytearray& ba) {
    ba.writeRaw(os);
    return os;
}

inline std::istream& operator>>(std::istream& is, bytearray& ba) {
    ba.clear();
    is.seekg(0, std::ios::end);
    auto sz = static_cast<size_t>(is.tellg());
    is.seekg(0, std::ios::beg);
    if (sz > 0) {
        ba = bytearray(sz);
        is.read(reinterpret_cast<char*>(ba.data()), static_cast<std::streamsize>(sz));
    }
    return is;
}

} // namespace scl2
