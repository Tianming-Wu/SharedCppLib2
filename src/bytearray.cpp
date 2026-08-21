#include "bytearray.hpp"
#include "stringlist.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <limits>
#include <iomanip>

namespace scl2 {

bytearray::bytearray(std::byte b) { base_type::push_back(b); }

bytearray::bytearray(const std::string& str) {
    base_type::assign(reinterpret_cast<const std::byte*>(str.data()),
                      reinterpret_cast<const std::byte*>(str.data()) + str.size());
}

bytearray::bytearray(const char* raw, size_t sz) {
    if (raw) base_type::assign(reinterpret_cast<const std::byte*>(raw),
                                reinterpret_cast<const std::byte*>(raw) + sz);
}

bytearray::bytearray(const std::byte* raw, size_t sz) {
    if (raw) base_type::assign(raw, raw + sz);
}

bytearray::bytearray(const void* raw, size_t sz) {
    if (raw) base_type::assign(reinterpret_cast<const std::byte*>(raw),
                                reinterpret_cast<const std::byte*>(raw) + sz);
}

bytearray::bytearray(size_t count, std::byte value) : base_type(count, value) {}
bytearray::bytearray(size_t count) : base_type(count, std::byte{0}) {}
bytearray::bytearray(std::initializer_list<std::byte> init) : base_type(init) {}

void bytearray::copy_from(const void* raw, size_t sz) {
    if (!raw) throw std::invalid_argument("bytearray::copy_from: null pointer");
    clear(); base_type::resize(sz);
    std::memcpy(this->data(), raw, sz);
}

void bytearray::copy_to(void* raw, size_t sz) const {
    if (!raw) throw std::invalid_argument("bytearray::copy_to: null pointer");
    if (sz > this->size()) throw std::invalid_argument("bytearray::copy_to: size exceeds data");
    std::memcpy(raw, this->data(), sz);
}

std::string bytearray::toString() const {
    seekr(0); if (empty()) return {};
    uint32_t len = read<uint32_t>(); if (len == 0) return {};
    return std::string(reinterpret_cast<const char*>(this->data() + sizeof(uint32_t)), len);
}

std::wstring bytearray::toWString() const {
    seekr(0); if (empty()) return {};
    uint32_t len = read<uint32_t>(); if (len == 0) return {};
    std::wstring str(len, L'\0');
    std::memcpy(&str[0], this->data() + sizeof(uint32_t), len * sizeof(wchar_t));
    return str;
}

std::string bytearray::toStdString() const {
    if (empty()) return {};
    return std::string(reinterpret_cast<const char*>(this->data()), this->size());
}

std::wstring bytearray::toStdWString() const {
    if (empty()) return {};
    return std::wstring(reinterpret_cast<const wchar_t*>(this->data()), this->size() / sizeof(wchar_t));
}

scl2::stringlist bytearray::toStringlist(const std::string& split) const { return scl2::stringlist(toStdString(), split); }
scl2::wstringlist bytearray::toWStringlist(const std::wstring& split) const { return scl2::wstringlist(toStdWString(), split); }

std::string bytearray::toHex() const { return toHex(0, seek_end); }

std::string bytearray::toHex(size_t begin, size_t n) const {
    size_t end = (n == seek_end) ? size() : std::min(size(), begin + n);
    if (begin >= end) return {};
    std::ostringstream oss; oss << std::hex << std::setfill('0');
    for (size_t i = begin; i < end; ++i) oss << std::setw(2) << std::to_integer<int>(at(i));
    return oss.str();
}

std::string bytearray::toEscapedString() const {
    std::ostringstream oss;
    for (size_t i = 0; i < size(); ++i) {
        int c = std::to_integer<int>(at(i));
        if (c >= 0x20 && c <= 0x7E && c != '\\' && c != '"') oss << static_cast<char>(c);
        else oss << "\\x" << std::hex << std::setfill('0') << std::setw(2) << c;
    }
    return oss.str();
}

std::string bytearray::xtoEscapedString() const {
    std::ostringstream oss;
    for (size_t i = 0; i < size(); ++i) oss << "\\x" << std::hex << std::setfill('0') << std::setw(2) << std::to_integer<int>(at(i));
    return oss.str();
}

std::u8string bytearray::toUtf8() const { return std::u8string(reinterpret_cast<const char8_t*>(this->data()), this->size()); }
std::u16string bytearray::toUtf16() const { return std::u16string(reinterpret_cast<const char16_t*>(this->data()), this->size() / sizeof(char16_t)); }
std::u32string bytearray::toUtf32() const { return std::u32string(reinterpret_cast<const char32_t*>(this->data()), this->size() / sizeof(char32_t)); }

// Base64
static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static int b64_idx(char c) { if(c>='A'&&c<='Z')return c-'A'; if(c>='a'&&c<='z')return c-'a'+26; if(c>='0'&&c<='9')return c-'0'+52; if(c=='+')return 62; if(c=='/')return 63; return -1; }

std::string bytearray::toBase64() const {
    if (empty()) return {};
    std::string r; r.reserve(((size()+2)/3)*4);
    size_t k=0;
    for(size_t i=0;i+2<size();i+=3){
        uint32_t t=(uint32_t(std::to_integer<uint8_t>(at(i)))<<16)|(uint32_t(std::to_integer<uint8_t>(at(i+1)))<<8)|uint32_t(std::to_integer<uint8_t>(at(i+2)));
        r+=kB64[(t>>18)&0x3F]; r+=kB64[(t>>12)&0x3F]; r+=kB64[(t>>6)&0x3F]; r+=kB64[t&0x3F]; k=i+3;
    }
    size_t rem=size()-k;
    if(rem==1){uint32_t v=uint32_t(std::to_integer<uint8_t>(at(k)))<<16; r+=kB64[(v>>18)&0x3F]; r+=kB64[(v>>12)&0x3F]; r+="==";}
    else if(rem==2){uint32_t v=(uint32_t(std::to_integer<uint8_t>(at(k)))<<16)|(uint32_t(std::to_integer<uint8_t>(at(k+1)))<<8); r+=kB64[(v>>18)&0x3F]; r+=kB64[(v>>12)&0x3F]; r+=kB64[(v>>6)&0x3F]; r+='=';}
    return r;
}

bytearray bytearray::fromBase64(const std::string& s){
    if(s.empty())return{};
    size_t len=s.size(),pad=0;
    if(len>0&&s[len-1]=='='){++pad;--len;} if(len>0&&s[len-1]=='='){++pad;--len;}
    size_t outLen=(len/4)*3; if(pad==1)++outLen; else if(pad==2)outLen+=2;
    bytearray r(outLen); size_t oi=0;
    for(size_t i=0;i<len;i+=4){
        int i0=b64_idx(s[i]),i1=b64_idx(s[i+1]),i2=(i+2<len)?b64_idx(s[i+2]):0,i3=(i+3<len)?b64_idx(s[i+3]):0;
        if(i0<0||i1<0||(i+2<len&&i2<0)||(i+3<len&&i3<0)) throw std::invalid_argument("bytearray::fromBase64: invalid char");
        uint32_t t=(uint32_t(i0)<<18)|(uint32_t(i1)<<12)|(uint32_t(i2)<<6)|uint32_t(i3);
        if(oi<outLen)r[oi++]=std::byte{uint8_t((t>>16)&0xFF)};
        if(oi<outLen)r[oi++]=std::byte{uint8_t((t>>8)&0xFF)};
        if(oi<outLen)r[oi++]=std::byte{uint8_t(t&0xFF)};
    }
    return r;
}

void bytearray::reverse() { std::reverse(base_type::begin(), base_type::end()); }

void bytearray::swap(bytearray& o) { base_type::swap(o); std::swap(write_pointer,o.write_pointer); std::swap(read_pointer,o.read_pointer); }

void bytearray::swap(size_t a, size_t b, size_t n) { for(size_t i=0;i<n;++i) std::swap(base_type::operator[](a+i),base_type::operator[](b+i)); }

bytearray& bytearray::replace(size_t pos, size_t n, const bytearray& d){
    if(pos>size())throw std::out_of_range("bytearray::replace: pos out of range");
    if(pos+n>size())n=size()-pos;
    base_type::erase(begin()+pos,begin()+pos+n); base_type::insert(begin()+pos,d.begin(),d.end()); return *this;
}

bytearray& bytearray::erase(size_t pos, size_t n){
    if(pos>size())throw std::out_of_range("bytearray::erase: pos out of range");
    if(pos+n>size())n=size()-pos;
    base_type::erase(begin()+pos,begin()+pos+n); return *this;
}

bytearray bytearray::subarr(size_t begin, size_t n) const {
    if(begin>=size())return{}; size_t end=(n==seek_end)?size():std::min(size(),begin+n);
    return bytearray(this->data()+begin,end-begin);
}

bytearray bytearray::shiftLeft(size_t o) const { if(o>=size())return bytearray(size(),std::byte{0}); return subarr(o); }
bytearray bytearray::shiftRight(size_t o) const { if(o>=size())return bytearray(size(),std::byte{0}); bytearray r(o,std::byte{0}); r.append(this->data(),size()-o); return r; }
bytearray bytearray::rotateLeft(size_t o) const { if(empty()||o==0)return *this; o%=size(); bytearray r=subarr(o); r.append(subarr(0,o)); return r; }
bytearray bytearray::rotateRight(size_t o) const { if(empty()||o==0)return *this; return rotateLeft(size()-o%size()); }

bytearray bytearray::bitShiftLeft(size_t offset) const
{
    if (offset == 0) return *this;
    size_t byteShift = offset / 8;
    size_t bitShift = offset % 8;
    bytearray result(size(), std::byte{0});

    if (bitShift == 0) {
        // Whole-byte shift: one memcpy of the tail, the rest stays zero-filled.
        if (byteShift < size()) {
            std::memcpy(result.data(), data() + byteShift, size() - byteShift);
        }
        return result;
    }

    for (size_t i = 0; i < size(); ++i) {
        // at(i) moves to byte (i - byteShift); its low bits spill into the next byte.
        if (i >= byteShift) {
            result[i - byteShift] |= static_cast<std::byte>(std::to_integer<uint8_t>(at(i)) << bitShift);
        }
        if (bitShift > 0 && i >= byteShift + 1) {
            result[i - byteShift - 1] |= static_cast<std::byte>(std::to_integer<uint8_t>(at(i)) >> (8 - bitShift));
        }
    }

    return result;
}

bytearray bytearray::bitShiftRight(size_t offset) const
{
    if (offset == 0) return *this;
    size_t byteShift = offset / 8;
    size_t bitShift = offset % 8;
    bytearray result(size(), std::byte{0});

    if (bitShift == 0) {
        // Whole-byte shift: one memcpy of the head, the front stays zero-filled.
        if (byteShift < size()) {
            std::memcpy(result.data() + byteShift, data(), size() - byteShift);
        }
        return result;
    }

    for (size_t i = 0; i < size(); ++i) {
        // at(i) moves to byte (i + byteShift); its low bits spill into the next byte.
        if (i + byteShift < size()) {
            result[i + byteShift] |= static_cast<std::byte>(std::to_integer<uint8_t>(at(i)) >> bitShift);
        }
        if (bitShift > 0 && i + byteShift + 1 < size()) {
            result[i + byteShift + 1] |= static_cast<std::byte>(std::to_integer<uint8_t>(at(i)) << (8 - bitShift));
        }
    }

    return result;
}

bytearray bytearray::bitRotateLeft(size_t offset) const
{
    if (empty() || offset == 0) return *this;
    size_t byteShift = (offset / 8) % size(); // byte-level left rotation: b[i] -> (i - byteShift) mod size
    size_t bitShift = offset % 8;
    bytearray result(size(), std::byte{0});

    for (size_t i = 0; i < size(); ++i) {
        size_t newIndex = (i + size() - byteShift) % size();
        result[newIndex] |= static_cast<std::byte>(std::to_integer<uint8_t>(at(i)) << bitShift);
        if (bitShift > 0) {
            size_t nextIndex = (newIndex + 1) % size();
            result[nextIndex] |= static_cast<std::byte>(std::to_integer<uint8_t>(at(i)) >> (8 - bitShift));
        }
    }

    return result;
}

bytearray bytearray::bitRotateRight(size_t offset) const
{
    if (empty() || offset == 0) return *this;
    size_t byteShift = (offset / 8) % size(); // byte-level right rotation: b[i] -> (i + byteShift) mod size
    size_t bitShift = offset % 8;
    bytearray result(size(), std::byte{0});

    for (size_t i = 0; i < size(); ++i) {
        size_t newIndex = (i + byteShift) % size();
        result[newIndex] |= static_cast<std::byte>(std::to_integer<uint8_t>(at(i)) >> bitShift);
        if (bitShift > 0) {
            size_t nextIndex = (newIndex + 1) % size();
            result[nextIndex] |= static_cast<std::byte>(std::to_integer<uint8_t>(at(i)) << (8 - bitShift));
        }
    }

    return result;
}

bytearray bytearray::bitTakeLeft(size_t bitCount) const
{
    if (bitCount == 0) return {};

    size_t bytesTaken = bitCount / 8;
    size_t bitsTaken = bitCount % 8;

    if (bytesTaken >= size()) {
        return *this;
    }

    // Copy the leading bytes in one memcpy via subarr, then append the
    // masked partial byte if bitCount is not byte-aligned.
    bytearray result = subarr(0, bytesTaken);
    if (bitsTaken > 0) {
        uint8_t lastByte = std::to_integer<uint8_t>(at(bytesTaken));
        lastByte &= static_cast<uint8_t>(0xFF << (8 - bitsTaken)); // keep the high bitsTaken bits
        result.append(std::byte(lastByte));
    }

    return result;
}

bytearray bytearray::bitTakeRight(size_t bitCount) const
{
    if (bitCount == 0) return {};

    size_t bytesTaken = bitCount / 8;
    size_t bitsTaken = bitCount % 8;

    if (bytesTaken >= size()) {
        return *this;
    }

    // Copy the trailing bytes in one memcpy via subarr.
    bytearray result = subarr(size() - bytesTaken, bytesTaken);
    if (bitsTaken > 0) {
        // Take the low bitsTaken bits of the preceding byte, then prepend it.
        uint8_t lastByte = std::to_integer<uint8_t>(at(size() - 1 - bytesTaken));
        lastByte &= static_cast<uint8_t>(0xFF >> (8 - bitsTaken));
        result.insert(0, static_cast<std::byte>(lastByte));

        // Left-align: shift the whole result left by (8 - bitsTaken) bits so the
        // taken bits occupy the most-significant positions.
        size_t shift = 8 - bitsTaken;
        uint8_t carry = 0;
        for (size_t i = result.size(); i-- > 0; ) {
            uint16_t val = (static_cast<uint16_t>(std::to_integer<uint8_t>(result[i])) << shift) | carry;
            result[i] = static_cast<std::byte>(val & 0xFF);
            carry = static_cast<uint8_t>(val >> 8);
        }
    }

    return result;
}

bool bytearray::operator==(const bytearray& o) const { return size()==o.size()&&(empty()||std::memcmp(data(),o.data(),size())==0); }
bool bytearray::operator<(const bytearray& o) const {
    size_t minLen = std::min(size(), o.size());
    if (minLen > 0) { int c = std::memcmp(data(), o.data(), minLen); if (c != 0) return c < 0; }
    return size() < o.size();
}
bytearray bytearray::operator+(const bytearray& o) const { bytearray r=*this; r.append(o); return r; }

bool bytearray::readFromStream(std::istream& is, size_t n){ clear(); base_type::resize(n); is.read(reinterpret_cast<char*>(data()),std::streamsize(n)); return is.good()||is.eof(); }
bool bytearray::readAllFromStream(std::istream& is){ clear(); is.seekg(0,std::ios::end); auto sz=size_t(is.tellg()); is.seekg(0,std::ios::beg); if(sz==0)return true; base_type::resize(sz); is.read(reinterpret_cast<char*>(data()),std::streamsize(sz)); return is.good()||is.eof(); }
bool bytearray::readUntilDelimiter(std::istream& is, char delim){ clear(); char ch; while(is.get(ch)){if(ch==delim)return true; base_type::push_back(std::byte{uint8_t(ch)});} return !empty(); }

bytearray bytearray::fromString(const std::string& s){ bytearray b; b.append(s); return b; }
bytearray bytearray::fromWString(const std::wstring& s){ bytearray b; b.append(s); return b; }
bytearray bytearray::fromStdString(const std::string& s){ return bytearray(s); }
bytearray bytearray::fromStdWString(const std::wstring& s){ return bytearray(reinterpret_cast<const std::byte*>(s.data()),s.size()*sizeof(wchar_t)); }

bytearray bytearray::fromHex(const std::string& hex){
    bytearray r; r.reserve((hex.size()+1)/2); int hi=0; bool hf=false;
    auto hv=[](char c)->int{if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1;};
    for(char c:hex){int v=hv(c); if(v<0)continue; if(!hf){hi=v<<4;hf=true;}else{r.push_back(std::byte{uint8_t(hi|v)});hf=false;}}
    if(hf)r.push_back(std::byte{uint8_t(hi)}); return r;
}

bytearray bytearray::fromRaw(const char* raw, size_t sz){ return bytearray(raw,sz); }
bytearray bytearray::fromRaw(const unsigned char* raw, size_t sz){ return bytearray(reinterpret_cast<const void*>(raw),sz); }
bytearray bytearray::fromPointer(const void* p){ if(!p)return{}; return bytearray(p,sizeof(void*)); }
bytearray bytearray::fromUtf8(const std::u8string& s){ return bytearray(reinterpret_cast<const std::byte*>(s.data()),s.size()); }
bytearray bytearray::fromUtf16(const std::u16string& s){ return bytearray(reinterpret_cast<const std::byte*>(s.data()),s.size()*sizeof(char16_t)); }
bytearray bytearray::fromUtf32(const std::u32string& s){ return bytearray(reinterpret_cast<const std::byte*>(s.data()),s.size()*sizeof(char32_t)); }

bytearray bytearray_view::subarr(size_t begin, size_t n) const {
    if(begin>=size_)return{}; size_t end=(n==bytearray::seek_end)?size_:std::min(size_,begin+n);
    return bytearray(data_+begin,end-begin);
}
bool bytearray_view::operator==(const bytearray_view& o) const { return size_==o.size_&&(size_==0||std::memcmp(data_,o.data_,size_)==0); }

} // namespace scl2
