/*
    High precision integer (hpint)

    namespace: none
    classes: hpint, hpfloat, hpfrac*
*/

#pragma once
#include <stdint.h>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>

namespace hpcalc {
enum specialState {
    None, Infinite, Nan
};
}


class hpislot {
public:
    hpislot() : value(0) {}
    hpislot(uint8_t v) : value(v) {}

    uint8_t apply() { uint8_t t = value; value = 0; return t; }
    uint8_t data() const { return value; }

    hpislot& operator=(const hpislot& _comp) {
        value = _comp.value;
        return *this;
    }

    hpislot operator+(const hpislot& _comp) const {
        uint8_t t = value + _comp.value;
        if(t > 9) t = 0;
        return hpislot(t);
    }

    hpislot operator-(const hpislot& _comp) const {
        uint8_t t = value - _comp.value;
        if(t < 0) t = 0;
        return hpislot(t);
    }

    hpislot operator*(const hpislot& _comp) const {
        uint8_t t = value * _comp.value;
        if(t > 9) t = 0;
        return hpislot(t);
    }

    hpislot operator/(const hpislot& _comp) const {
        if(_comp.value == 0) return hpislot(0);
        uint8_t t = value / _comp.value;
        return hpislot(t);
    }

    hpislot operator%(const hpislot& _comp) const {
        if(_comp.value == 0) return hpislot(0);
        uint8_t t = value % _comp.value;
        return hpislot(t);
    }

    bool operator==(const hpislot& _comp) const { return value == _comp.value; }
    bool operator!=(const hpislot& _comp) const { return value != _comp.value; }
    bool operator>(const hpislot& _comp) const { return value > _comp.value; }
    bool operator<(const hpislot& _comp) const { return value < _comp.value; }

private:
    uint8_t value;
};

class hpint {
public:
    hpint() {}
    hpint(int32_t integer) { make(std::to_string(integer)); }
    hpint(uint32_t integer) { make(std::to_string(integer)); }
    hpint(hpcalc::specialState state) : special(state), negative(false) {}

    void make(std::string t) {
        if(t.empty()) { slots.clear(); return; }
        if(t.at(0) == '-') {
            negative = true;
            t.erase(0, 1);
        } else negative = false;
        if(t.at(0) == '+') t.erase(0, 1);

        slots.reserve(t.length());
        for(size_t i = t.length()-1; i >= 0; i--) {
            hpislot slot = std::clamp(t[i]-'0', 0, 9);
            if(slot != t[i]-'0') throw std::runtime_error("hpint::make: invalid character");
            slots.push_back(slot);
        }
    }

    inline size_t length() const { return slots.size(); }
    inline hpislot at(size_t i) const { return slots.at(i); }
    inline hpislot& operator[](size_t i) { return slots[i]; }

    bool operator==(const hpint& _comp) const {
        return (length()==_comp.length()) && [&] {
            for(size_t i = 0; i < length(); i++) { if(slots[i] != _comp.at(i)) return false; } return true;
        }();
    }

    hpint operator+(const hpint& _comp) const {
        if (special == hpcalc::Nan || _comp.special == hpcalc::Nan) return hpint(hpcalc::Nan);
        if (special == hpcalc::Infinite || _comp.special == hpcalc::Infinite) return hpint(hpcalc::Infinite);
    
        if (negative == _comp.negative) {
            hpint result(0);
            size_t maxLength = std::max(length(), _comp.length());
            result.slots.resize(maxLength);
    
            for (size_t i = 0; i < maxLength; i++) {
                hpislot a = (i < length()) ? at(i) : hpislot(0);
                hpislot b = (i < _comp.length()) ? _comp.at(i) : hpislot(0);
                result.slots[i] = a + b;
            }
            result.negative = negative;
            return result.simplify();
        } else {
            return *this - (-_comp);
        }
    }

    hpint operator-(const hpint& _comp) const {
        if (special == hpcalc::Nan || _comp.special == hpcalc::Nan) return hpint(hpcalc::Nan);
        if (special == hpcalc::Infinite && _comp.special == hpcalc::Infinite) return hpint(0);
        if (special == hpcalc::Infinite) return hpint(hpcalc::Infinite);
        if (_comp.special == hpcalc::Infinite) return - hpint(hpcalc::Infinite);
    
        if (negative != _comp.negative) {
            return *this + (-_comp);
        } else {
            if (*this < _comp) {
                return -(_comp - *this);
            }
    
            hpint result(0);
            size_t maxLength = std::max(length(), _comp.length());
            result.slots.resize(maxLength);
    
            for (size_t i = 0; i < maxLength; i++) {
                hpislot a = (i < length()) ? at(i) : hpislot(0);
                hpislot b = (i < _comp.length()) ? _comp.at(i) : hpislot(0);
                result.slots[i] = a - b;
            }
            result.negative = negative;
            return result.simplify();
        }
    }
    
    hpint operator*(const hpint& _comp) const {
        if (special == hpcalc::Nan || _comp.special == hpcalc::Nan) return hpint(hpcalc::Nan);
        if (special == hpcalc::Infinite || _comp.special == hpcalc::Infinite) {
            if (isZero() || _comp.isZero()) return hpint(hpcalc::Nan);
            return hpint(hpcalc::Infinite).setNegative(negative != _comp.negative);
        }
    
        hpint result(0);
        size_t maxLength = length() + _comp.length();
        result.slots.resize(maxLength);
    
        for (size_t i = 0; i < length(); i++) {
            for (size_t j = 0; j < _comp.length(); j++) {
                result.slots[i + j] = result.slots[i + j] + at(i) * _comp.at(j);
            }
        }
        result.negative = (negative != _comp.negative);
        return result.simplify();
    }
    
    hpint operator/(const hpint& _comp) const {
        if (special == hpcalc::Nan || _comp.special == hpcalc::Nan) return hpint(hpcalc::Nan);
        if (_comp.isZero()) return hpint(hpcalc::Nan);
        if (special == hpcalc::Infinite) return hpint(hpcalc::Infinite).setNegative(negative != _comp.negative);
        if (_comp.special == hpcalc::Infinite) return hpint(0);
    
        // Simplified division logic (integer division)
        hpint result(0);
        hpint remainder = *this;
        result.negative = (negative != _comp.negative);
    
        while (remainder >= _comp) {
            remainder = remainder - _comp;
            result = result + hpint(1);
        }
        return result.simplify();
    }
    
    hpint operator%(const hpint& _comp) const {
        if (special == hpcalc::Nan || _comp.special == hpcalc::Nan) return hpint(hpcalc::Nan);
        if (_comp.isZero()) return hpint(hpcalc::Nan);
        if (special == hpcalc::Infinite) return hpint(hpcalc::Nan);
        if (_comp.special == hpcalc::Infinite) return *this;
    
        hpint remainder = *this;
        while (remainder >= _comp) {
            remainder = remainder - _comp;
        }
        return remainder;
    }

    bool operator>(const hpint& _comp) const {
        if(length() > _comp.length()) return true;
        if(length() < _comp.length()) return false;

        for(size_t i = length(); i > 0; i--) {
            if(at(i-1) > _comp.at(i-1)) return true;
            if(at(i-1) < _comp.at(i-1)) return false;
        }
        return false;
    }

    bool operator<(const hpint& _comp) const {
        if(length() < _comp.length()) return true;
        if(length() > _comp.length()) return false;

        for(size_t i = length(); i > 0; i--) {
            if(at(i-1) < _comp.at(i-1)) return true;
            if(at(i-1) > _comp.at(i-1)) return false;
        }
        return false;
    }

    bool operator>=(const hpint& _comp) const { return !(*this < _comp); }
    bool operator<=(const hpint& _comp) const { return !(*this > _comp); }
    bool operator!=(const hpint& _comp) const { return !(*this == _comp); }

    hpint operator+=(const hpint& _comp) { return (*this = *this + _comp); }
    hpint operator-=(const hpint& _comp) { return (*this = *this - _comp); }
    hpint operator*=(const hpint& _comp) { return (*this = *this * _comp); }
    hpint operator/=(const hpint& _comp) { return (*this = *this / _comp); }
    hpint operator%=(const hpint& _comp) { return (*this = *this % _comp); }

    hpint operator++() { return (*this += hpint(1)); }
    hpint operator--() { return (*this -= hpint(1)); }

    hpint operator<<(int shift) const {
        hpint result(0);
        result.slots.resize(length() + shift);
        std::copy(slots.begin(), slots.end(), result.slots.begin() + shift);
        return result;
    }

    hpint operator>>(int shift) const {
        if(shift >= length()) return hpint(0);
        hpint result(0);
        result.slots.resize(length() - shift);
        std::copy(slots.begin() + shift, slots.end(), result.slots.begin());
        return result;
    }

    hpint& simplify() {
        while(length() > 0 && at(length()-1) == 0) {
            slots.pop_back();
        }
        return *this;
    }

    bool isZero() const {
        if(length() == 0) return true;
        if(length() == 1 && at(0) == 0) return true;
        return false;
    }

    hpint operator-() const {
        if (special == hpcalc::Nan) return hpint(hpcalc::Nan); // 负的 NaN 仍然是 NaN
        if (special == hpcalc::Infinite) {
            hpint result(hpcalc::Infinite);
            result.negative = !negative; // 反转符号
            return result;
        }
        hpint result = *this;
        result.negative = !negative; // 反转符号
        return result;
    }
    
    bool isNegative() const { return negative; }
    
    hpint& setNegative(bool isNegative) {
        negative = isNegative;
        return *this;
    }

    bool isInfinite() const { return special == hpcalc::Infinite; }
    bool isNan() const { return special == hpcalc::Nan; }

    std::string to_string() const {
        std::string result;
        if(negative) result += '-';
        if(length() == 0) return "0";
        if(special == hpcalc::Infinite) return "inf";
        if(special == hpcalc::Nan) return "nan";
        for(size_t i = length(); i > 0; i--) {
            result += std::to_string(at(i-1).data());
        }
        return result;
    }

    static hpint from_string(const std::string& str) {
        hpint result(0);
        result.make(str);
        return result;
    }


protected:
    std::vector<hpislot> slots;
    bool negative;
    hpcalc::specialState special = hpcalc::None;
};

std::istream& operator>>(std::istream& in, hpint& _comp) {
    std::string str;
    in >> str;
    _comp.make(str);
    return in;
}

std::ostream& operator<<(std::ostream& out, const hpint& _comp) {
    out << _comp.to_string();
    return out;
}


class hpfloat {

public:
    hpfloat() : integer(0), fraction(0) {}
    hpfloat(int32_t integer, int32_t fraction) : integer(integer), fraction(fraction) {}
    hpfloat(int32_t integer) : integer(integer), fraction(0) {}
    hpfloat(const hpint& integer, const hpint& fraction) : integer(integer), fraction(fraction) {}
    hpfloat(const hpint& integer) : integer(integer), fraction(0) {}

    hpfloat(float value) { make(std::to_string(value)); }
    hpfloat(double value) { make(std::to_string(value)); }

    hpfloat(hpcalc::specialState state) : integer(state), fraction(state), negative(false) {}

    void make(const std::string& str) {
        size_t dotPos = str.find('.');
        if(dotPos != std::string::npos) {
            integer.make(str.substr(0, dotPos));
            fraction.make(str.substr(dotPos + 1));
        } else {
            integer.make(str);
            fraction = hpint(0);
        }
    }

    static hpfloat from_string(const std::string& str) {
        hpfloat result;
        result.make(str);
        return result;
    }

    std::string to_string() const {
        std::string result = integer.to_string();
        if(fraction.length() > 0) {
            result += '.';
            result += fraction.to_string();
        }
        return result;
    }

    // 加法运算符
    hpfloat operator+(const hpfloat& _comp) const {
        if (special == hpcalc::Nan || _comp.special == hpcalc::Nan) return hpfloat(hpcalc::Nan);
        if (special == hpcalc::Infinite || _comp.special == hpcalc::Infinite) {
            if (special == hpcalc::Infinite && _comp.special == hpcalc::Infinite) {
                return (negative == _comp.negative) ? *this : hpfloat(hpcalc::Nan);
            }
            return (special == hpcalc::Infinite) ? *this : _comp;
        }

        hpfloat result;
        result.integer = integer + _comp.integer;
        result.fraction = fraction + _comp.fraction;

        // 如果小数部分溢出，进位到整数部分
        if (result.fraction.length() > fraction.length()) {
            result.integer += hpint(1);
            result.fraction.simplify();
        }

        return result;
    }

    // 减法运算符
    hpfloat operator-(const hpfloat& _comp) const {
        if (special == hpcalc::Nan || _comp.special == hpcalc::Nan) return hpfloat(hpcalc::Nan);
        if (special == hpcalc::Infinite || _comp.special == hpcalc::Infinite) {
            if (special == hpcalc::Infinite && _comp.special == hpcalc::Infinite) {
                return (negative == _comp.negative) ? hpfloat(0) : hpfloat(hpcalc::Nan);
            }
            return (special == hpcalc::Infinite) ? *this : -_comp;
        }

        hpfloat result;
        result.integer = integer - _comp.integer;
        result.fraction = fraction - _comp.fraction;

        // 如果小数部分借位，调整整数部分
        if (result.fraction.isNegative()) {
            result.integer -= hpint(1);
            result.fraction += hpint(10); // 假设小数部分是以 10 为基数
        }

        return result;
    }

    // 乘法运算符
    hpfloat operator*(const hpfloat& _comp) const {
        if (special == hpcalc::Nan || _comp.special == hpcalc::Nan) return hpfloat(hpcalc::Nan);
        if (special == hpcalc::Infinite || _comp.special == hpcalc::Infinite) {
            if (isZero() || _comp.isZero()) return hpfloat(hpcalc::Nan);
            return hpfloat(hpcalc::Infinite).setNegative(negative != _comp.negative);
        }

        hpfloat result;
        result.integer = integer * _comp.integer;
        result.fraction = (integer * _comp.fraction) + (fraction * _comp.integer);

        return result;
    }

    // 除法运算符
    hpfloat operator/(const hpfloat& _comp) const {
        if (special == hpcalc::Nan || _comp.special == hpcalc::Nan) return hpfloat(hpcalc::Nan);
        if (_comp.isZero()) return hpfloat(hpcalc::Nan);
        if (special == hpcalc::Infinite) return hpfloat(hpcalc::Infinite).setNegative(negative != _comp.negative);
        if (_comp.special == hpcalc::Infinite) return hpfloat(0);

        hpfloat result;
        result.integer = integer / _comp.integer;
        result.fraction = (integer % _comp.integer) / _comp.fraction;

        return result;
    }

    // 取模运算符
    hpfloat operator%(const hpfloat& _comp) const {
        if (special == hpcalc::Nan || _comp.special == hpcalc::Nan) return hpfloat(hpcalc::Nan);
        if (_comp.isZero()) return hpfloat(hpcalc::Nan);
        if (special == hpcalc::Infinite) return hpfloat(hpcalc::Nan);
        if (_comp.special == hpcalc::Infinite) return *this;

        hpfloat result;
        result.integer = integer % _comp.integer;
        result.fraction = fraction % _comp.fraction;

        return result;
    }

    // 比较运算符
    bool operator==(const hpfloat& _comp) const {
        if (special != _comp.special) return false;
        if (special == hpcalc::Nan || _comp.special == hpcalc::Nan) return false;
        if (special == hpcalc::Infinite || _comp.special == hpcalc::Infinite) {
            return (special == _comp.special) && (negative == _comp.negative);
        }
        return (integer == _comp.integer) && (fraction == _comp.fraction) && (negative == _comp.negative);
    }

    bool operator!=(const hpfloat& _comp) const {
        return !(*this == _comp);
    }

    bool operator<(const hpfloat& _comp) const {
        if (special == hpcalc::Nan || _comp.special == hpcalc::Nan) return false;
        if (special == hpcalc::Infinite) return negative; // 负的 Infinite 小于任何数
        if (_comp.special == hpcalc::Infinite) return !_comp.negative; // 正的 Infinite 大于任何数

        if (negative != _comp.negative) return negative; // 负数小于正数

        if (integer != _comp.integer) return (negative ? integer > _comp.integer : integer < _comp.integer);
        return (negative ? fraction > _comp.fraction : fraction < _comp.fraction);
    }

    bool operator<=(const hpfloat& _comp) const {
        return (*this < _comp) || (*this == _comp);
    }

    bool operator>(const hpfloat& _comp) const {
        return !(*this <= _comp);
    }

    bool operator>=(const hpfloat& _comp) const {
        return !(*this < _comp);
    }

    // 复合赋值运算符
    hpfloat& operator+=(const hpfloat& _comp) { return *this = *this + _comp; }
    hpfloat& operator-=(const hpfloat& _comp) { return *this = *this - _comp; }
    hpfloat& operator*=(const hpfloat& _comp) { return *this = *this * _comp; }
    hpfloat& operator/=(const hpfloat& _comp) { return *this = *this / _comp; }
    hpfloat& operator%=(const hpfloat& _comp) { return *this = *this % _comp; }

    // 判断是否为零
    bool isZero() const {
        return integer.isZero() && fraction.isZero();
    }

    // 赋值运算符
    hpfloat& operator=(const hpfloat& _comp) {
        if (this != &_comp) {
            integer = _comp.integer;
            fraction = _comp.fraction;
            negative = _comp.negative;
            special = _comp.special;
        }
        return *this;
    }

    // 取反运算符
    hpfloat operator-() const {
        hpfloat result = *this;
        result.negative = !negative;
        return result;
    }

    bool isNegative() const { return negative; }

    // 设置符号
    hpfloat& setNegative(bool isNegative) {
        negative = isNegative;
        return *this;
    }

    bool isInfinite() const { return special == hpcalc::Infinite; }
    bool isNan() const { return special == hpcalc::Nan; }

    friend hpfloat floor(const hpfloat& _ref, const int32_t lev);
    friend hpfloat log(const hpfloat& _ref, const hpfloat& base);
    friend hpint round(const hpfloat& _ref);
    friend hpint ceil(const hpfloat& _ref);
    friend hpint truncate(const hpfloat& _ref);

protected:
    hpint integer;
    hpint fraction;

    bool negative;
    hpcalc::specialState special;
};

// 输入流运算符
std::istream& operator>>(std::istream& in, hpfloat& _comp) {
    std::string str;
    in >> str;
    _comp.make(str);
    return in;
}

// 输出流运算符
std::ostream& operator<<(std::ostream& out, const hpfloat& _comp) {
    out << _comp.to_string();
    return out;
}


class hpfrac {
public:

protected:
    hpint numerator;
    hpint denominator;

};