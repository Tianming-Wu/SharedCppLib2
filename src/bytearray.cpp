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

void bytearray::reverse() {
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

bool bytearray::operator== (const bytearray &ba) const {
    if(size() != ba.size()) return false;
    for(size_t i = 0; i < size(); i++) {
        if(at(i) != ba.at(i)) return false;
    }
    return true;
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

} // namespace std