#include "bytearray.hpp"
#include "stringlist.hpp"

namespace std {

byte bytearray::at(size_t i) const{
    return ::std::vector<byte>::at(i);
}

byte bytearray::vat(size_t p, const byte &v) const {
    return (p<size())?at(p):v;
}

void bytearray::append(const bytearray &ba) {
    for(const byte &b : ba) {
        push_back(b);
    }
}

void bytearray::append(const byte &b) {
    push_back(b);
}


void bytearray::append(const byte* pb, size_t size) {
    for(size_t i = 0; i < size; i++) {
        push_back(pb[i]);
    }
}

void bytearray::append(const char* str, size_t size) {
    append(reinterpret_cast<const byte*>(str), size);
}

void bytearray::append(const char* str) {
    if(str == nullptr) return;
    append(str, std::string(str).length());
}

void bytearray::append(uint8_t val) {
    append(static_cast<std::byte>(val));
}

void bytearray::addString(const std::string &str)
{
    append(std::bytearray(str.length()));
    append(str.data(), str.size());
}

void bytearray::reverse()
{
    std::reverse(begin(), end());
}

void bytearray::swap(bytearray &ba) {
    std::swap(*this, ba);
}

void bytearray::swap(size_t a, size_t b, size_t size) {
    for(size_t i = 0; i < size; i++) {
        std::swap(this->operator[](a+i), this->operator[](b+i));
    }
}

std::bytearray &bytearray::replace(size_t pos, size_t len, const bytearray &ba)
{
    if (pos > this->size()) throw std::out_of_range("bytearray::replace: position out of range");
    if (pos + len > this->size()) len = this->size() - pos; // adjust len to fit within bounds

    this->erase(this->begin() + pos, this->begin() + pos + len);
    this->::std::vector<::std::byte>::insert(this->begin() + pos, ba.begin(), ba.end());

    return *this;
}

std::bytearray &bytearray::insert(size_t pos, const bytearray &ba)
{
    if (pos > this->size()) throw std::out_of_range("bytearray::insert: position out of range");
    this->::std::vector<::std::byte>::insert(this->begin() + pos, ba.begin(), ba.end());
    return *this;
}

bytearray bytearray::subarr(size_t begin, size_t size) const {
    bytearray re;
    if(size == -1) size = this->size();
    for(size_t i = begin; i < size; i++) {
        re.push_back(at(i));
    }
    return re;
}

std::string bytearray::tostdstring() const {
    std::string re;
    for(const byte &b : *this) {
        re += static_cast<char>(b);
    }
    return re;
}

std::stringlist bytearray::tostringlist(const std::string& split) const {
    return std::stringlist(this->tostdstring(), split);
}

std::wstring bytearray::tostdwstring() const {
    if (this->empty()) return std::wstring();

    return std::wstring(
        reinterpret_cast<const wchar_t*>(this->data()),
        this->size() / sizeof(wchar_t)
    );
}

std::wstringlist bytearray::towstringlist(const std::wstring& split) const {
    return std::wstringlist(this->tostdwstring(), split);
}

std::string bytearray::tohex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (const byte &b : *this) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

std::string bytearray::tohex(size_t begin, size_t size) const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    if (size == -1) size = this->size();
    for (size_t i = begin; i < size; i++) {
        oss << std::setw(2) << static_cast<int>(at(i));
    }
    return oss.str();
}

std::string bytearray::toEscapedString() const {
    std::stringstream ss;
    for (byte b : *this) {
        if (isprint(static_cast<int>(b))) ss << static_cast<char>(b);
        else ss << "\\x" << std::hex << std::setw(2) << (int)b;
    }
    return ss.str();
}

/// @brief Upgraded version of escaped string, which contains more options.
/// @return escaped string.
std::string bytearray::xtoEscapedString() const
{
    std::stringstream ss;

    for (byte b : *this) {
        unsigned char c = static_cast<unsigned char>(b);
        switch (c) {
            case '\0': ss << "\\0"; break;
            case '\a': ss << "\\a"; break;
            case '\b': ss << "\\b"; break;
            case '\t': ss << "\\t"; break;
            case '\n': ss << "\\n"; break;
            case '\v': ss << "\\v"; break;
            case '\f': ss << "\\f"; break;
            case '\r': ss << "\\r"; break;
            case '\\\\': ss << "\\\\\\\\"; // 转义反斜杠
            default:
                if (isprint(c)) {
                    ss << static_cast<char>(c);
                } else {
                    ss << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
                }
                break;
        }
    }

    return ss.str();
}

std::u8string bytearray::toUtf8() const
{
    if (this->empty()) return std::u8string();
    
    return std::u8string(
        reinterpret_cast<const char8_t*>(this->data()),
        this->size()
    );
}

std::u16string bytearray::toUtf16() const
{
    if (this->empty()) return std::u16string();
    
    // Ensure size is multiple of sizeof(char16_t)
    if (this->size() % sizeof(char16_t) != 0) {
        throw std::runtime_error("bytearray::toUtf16: size must be multiple of 2 bytes");
    }
    
    return std::u16string(
        reinterpret_cast<const char16_t*>(this->data()),
        this->size() / sizeof(char16_t)
    );
}

std::u32string bytearray::toUtf32() const
{
    if (this->empty()) return std::u32string();
    
    // Ensure size is multiple of sizeof(char32_t)
    if (this->size() % sizeof(char32_t) != 0) {
        throw std::runtime_error("bytearray::toUtf32: size must be multiple of 4 bytes");
    }
    
    return std::u32string(
        reinterpret_cast<const char32_t*>(this->data()),
        this->size() / sizeof(char32_t)
    );
}

bool bytearray::operator== (const bytearray &ba) const {
    if(size() != ba.size()) return false;
    for(size_t i = 0; i < size(); i++) {
        if(at(i) != ba.at(i)) return false;
    }
    return true;
}

std::bytearray bytearray::operator<<(size_t offset) const { return shiftLeft(offset); }
std::bytearray bytearray::operator>>(size_t offset) const { return shiftRight(offset); }

std::bytearray bytearray::shiftLeft(size_t offset) const
{
    if (offset >= size()) {
        // 移位超过大小，返回全0数组
        return bytearray(size(), std::byte(0x00));
    }
    
    bytearray result(size(), std::byte(0x00));
    // 将 [offset, size) 的数据复制到 [0, size-offset)
    std::copy(begin() + offset, end(), result.begin());
    
    return result;
}

std::bytearray bytearray::shiftRight(size_t offset) const
{
    if (offset >= size()) {
        // 移位超过大小，返回全0数组
        return bytearray(size(), std::byte(0x00));
    }
    
    bytearray result(size(), std::byte(0x00));
    // 将 [0, size-offset) 的数据复制到 [offset, size)
    std::copy(begin(), end() - offset, result.begin() + offset);
    
    return result;
}

std::bytearray bytearray::rotateLeft(size_t offset) const
{
    if (empty() || offset == 0) return *this;
    
    offset %= size();  // 处理超过大小的旋转
    if (offset == 0) return *this;
    
    bytearray result;
    result.reserve(size());
    
    // 将 [offset, size) 复制到前面
    result.::std::vector<::std::byte>::insert(
        result.end(),
        begin() + offset, end()
    );
    // 将 [0, offset) 复制到后面
    result.::std::vector<::std::byte>::insert(
        result.end(),
        begin(), begin() + offset
    );
    
    return result;
}

std::bytearray bytearray::rotateRight(size_t offset) const
{
    if (empty() || offset == 0) return *this;
    
    offset %= size();  // 处理超过大小的旋转
    if (offset == 0) return *this;
    
    bytearray result;
    result.reserve(size());
    
    // 将 [size-offset, size) 复制到前面
    result.::std::vector<::std::byte>::insert(
        result.end(),
        end() - offset, end()
    );
    // 将 [0, size-offset) 复制到后面
    result.::std::vector<::std::byte>::insert(
        result.end(),
        begin(), end() - offset
    );
    
    return result;
}

bytearray::bytearray()
: vector<byte>()
{}

bytearray::bytearray(const bytearray &ba) {
    for (const byte &b : ba)
        push_back(b);
}

bytearray::bytearray(const std::string &str) {
    for (const char &c : str)
        push_back(static_cast<byte>(c));
}

bytearray::bytearray(const char *raw, size_t size) {
    for (size_t i = 0; i < size; i++)
        push_back(static_cast<byte>(raw[i]));
}

bytearray bytearray::fromHex(const std::string& hex) {
    if (hex.size() % 2 != 0) throw std::invalid_argument("[bytearray::fromHex()] Invalid hex string (odd length)");

    bytearray result;
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

bytearray bytearray::fromRaw(const char* raw, size_t size) {
    return bytearray(raw, size);
}

bytearray bytearray::fromUtf8(const std::u8string& utf8str)
{
    if (utf8str.empty()) return bytearray();
    
    bytearray result;
    result.reserve(utf8str.size());
    
    const byte* src = reinterpret_cast<const byte*>(utf8str.data());
    for (size_t i = 0; i < utf8str.size(); i++) {
        result.push_back(src[i]);
    }
    
    return result;
}

bytearray bytearray::fromUtf16(const std::u16string& utf16str)
{
    if (utf16str.empty()) return bytearray();
    
    bytearray result;
    size_t byte_size = utf16str.size() * sizeof(char16_t);
    result.reserve(byte_size);
    
    const byte* src = reinterpret_cast<const byte*>(utf16str.data());
    for (size_t i = 0; i < byte_size; i++) {
        result.push_back(src[i]);
    }
    
    return result;
}

bytearray bytearray::fromUtf32(const std::u32string& utf32str)
{
    if (utf32str.empty()) return bytearray();
    
    bytearray result;
    size_t byte_size = utf32str.size() * sizeof(char32_t);
    result.reserve(byte_size);
    
    const byte* src = reinterpret_cast<const byte*>(utf32str.data());
    for (size_t i = 0; i < byte_size; i++) {
        result.push_back(src[i]);
    }
    
    return result;
}

bool bytearray::readFromStream(std::istream &is, size_t size) {
    clear();
    if (size == 0) return true;
    
    resize(size);
    is.read(reinterpret_cast<char*>(data()), size);
    size_t bytes_read = static_cast<size_t>(is.gcount());
    
    if (bytes_read < size) {
        resize(bytes_read);
    }
    
    return bytes_read > 0 || size == 0;
}

bool bytearray::readAllFromStream(std::istream &is)
{
    clear();
    auto current_pos = is.tellg();
    
    is.seekg(0, std::ios::end);
    auto total_size = is.tellg();
    
    is.seekg(current_pos, std::ios::beg);
    
    auto remaining_size = total_size - current_pos;
    
    if (remaining_size <= 0) return false;
    
    resize(remaining_size);
    return static_cast<std::streamsize>(is.read(reinterpret_cast<char*>(data()), remaining_size).gcount()) == remaining_size;
}

bool bytearray::readUntilDelimiter(std::istream &is, char delimiter)
{
    clear();
    
    if (!is.good()) return false;
    
    std::string temp;
    char ch;
    while (is.get(ch) && ch != delimiter) {
        temp += ch;
    }
    
    if (!temp.empty() || (is.eof() && !temp.empty())) {
        *this = bytearray(temp);
        return true;
    }
    
    return false;
}


/*=============================================*\
|           bytearray_view functions            |
\*=============================================*/

// what the hell was that above?

bytearray_view::bytearray_view(const bytearray &data)
    : ba(data), cursor(0)
{}

bool bytearray_view::available(size_t bytes) const { return cursor + bytes <= ba.size(); }
size_t bytearray_view::remaining() const { return ba.size() - cursor; }
void bytearray_view::seek(size_t pos) { cursor = pos; }
void bytearray_view::reset() { cursor = 0; }
size_t bytearray_view::tell() const { return cursor; }

std::string bytearray_view::peekString() const
{
    std::string result;
    
    if(!available(sizeof(size_t))) throw std::out_of_range("bytearray_view: not enough size data");
    size_t str_size = ba.subarr(cursor, sizeof(size_t)).as<size_t>();
    
    if(!available(str_size)) throw std::out_of_range("bytearray_view: not enough string data");
    result.resize(str_size);

    std::memcpy(result.data(), ba.data() + sizeof(size_t), str_size);
    return result;
}

std::string bytearray_view::readString() const
{
    std::string result = peekString();
    cursor += sizeof(size_t) + result.size();
    return result;
}

} // namespace std