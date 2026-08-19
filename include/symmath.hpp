/*
    Symbolic math (symmath)

    namespace: scl2::sym
    classes:        

*/

#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <variant>
#include <limits>
#include <concepts>
#include <type_traits>
#include <cmath>

#include "math.hpp"

// currently for building arrays. May be excluded in the future
// if we want to make this module standalone.
#include "stringlist.hpp"

namespace scl2::sym {

// special values for symint and symfloat.
// not symobject system's. use symspecial for that.
enum special {
    Null, Infinite, Nan
};

// type of symbolic object
enum class type {
    none,
    integer, floatpoint, fraction, complex,
    matrix, vector, special,
    add, subtract, multiply, divide,
    power,
};

class symobject {
public:

    type symtype() const { return m_type; }


    
protected:
    symobject() : m_type(type::none) {}

    template<typename T>
    requires std::is_integral_v<T>
    symobject(T value) : m_type(type::integer), m_integer(static_cast<int64_t>(value)) {}

    template<typename T>
    requires std::is_floating_point_v<T>
    symobject(T value) : m_type(type::floatpoint), m_float(static_cast<double>(value)) {}

    symobject operator+(const symobject& other) const;
    symobject operator-(const symobject& other) const;
    symobject operator*(const symobject& other) const;
    symobject operator/(const symobject& other) const;


private:
    type m_type;
    std::variant<
        std::monostate, // none
        int64_t, double, // integer and floating point
        std::unique_ptr<std::pair<symobject, symobject>>, // fraction, complex and power
        std::vector<symobject> // vector, and add / substract / multiply / devide operands
        /*, ::scl2::matrix<symobject> */ // matrix and
    > m_value;

    // note: matrix is currently disabled, since it requires the included type to support
    // all required mathematical operations. This is not currently true for symobject,
    // so until symobject is almost complete it will stay commented out.

};

// Auto-spread invalid / infinite
#define sreturn_if_both_infinite if (this->special == special::Infinite || other.special == special::Infinite) return
#define sreturn_if_both_nan if (this->special == special::Nan || other.special == special::Nan) return
#define sreturn_if_any_infinite if (this->special == special::Infinite || other.special == special::Infinite) return
#define sreturn_if_any_nan if (this->special == special::Nan || other.special == special::Nan) return

#define sreturn_if_both(spec) if (this->special == spec && other.special == spec) return
#define sreturn_if_any(spec) if (this->special == spec || other.special == spec) return

class intslot
{
public:
    intslot() : v(0) {}
    intslot(uint8_t value) : v(std::clamp(value, (uint8_t)0, (uint8_t)9)) {}
    inline operator uint8_t() const { return v; }
private:
    uint8_t v;
};


// we cannot construct from symobject here since symobject will
// be based on this. We allow this to convert into a symobject though.
class symint 
{
public:
    symint() {}
    symint(scl2::sym::special state)
        : special(state)
    {
        // does not allow user to construct special value using null.
        // in that case, the default constructor should be used instead.
        if (state == special::Null)
            throw std::runtime_error("symint: manual construction of Null type is not allowed.");
    }

    template<typename T>
    requires std::is_integral_v<T>
    symint(T value) {
        // this is much more simple, and not that much slower than the manual implementation.
        // since the compiler will probably optimize this to move, with almost no overhead.
        *this = from_string(std::to_string(value));
    }

    // factory method to create a symint from a string
    // this is possibly the only way to create a symint that is
    // larger than that normal integer types can hold.
    static symint from_string(std::string str) {
        symint result;
        if(str.empty()) return result;

        if(str == "inf" || str == "infinity") {
            result.special = special::Infinite;
            return result;
        } else if (str == "nan") {
            result.special = special::Nan;
            return result;
        }

        if(str.at(0) == '-') {
            result.negative = true;
            str.erase(0, 1);
        }

        if(str.at(0) == '+') str.erase(0, 1);

        result.slots.reserve(str.length());
        for(size_t i = str.length() - 1; i >= 0; i--) {
            intslot slot = std::clamp(str[i]-'0', 0, 9); // what could possibly go wrong here
            if(slot != str[i]-'0') throw std::runtime_error("symint::from_string: invalid character");
            result.slots.push_back(slot);
        }

        return result;
    }

    // expo version of the symint from_string is available through symfloat, since
    // throwing away so many things here doesn't make sense.
    // you can clamp the constructed symfloat if you need.

    inline size_t length() const { return slots.size(); }
    inline auto at(size_t i) const { return slots.at(i); }
    inline auto& operator[](size_t i) { return slots[i]; }

    bool operator==(const symint& other) const {
        return (length() == other.length()) && [&] {
            for(size_t i = 0; i < length(); i++)
                if(slots[i] != other.at(i)) return false;
            return true;
        }();
    }

    // Compare without time information, to avoid side channel attacks.
    // Added for absolutely no reason.
    bool constant_time_compare(const symint& other) const {
        bool diff = true;
        for (size_t i = 0; i < std::max(length(), other.length()); i++) {
            auto a = (i < length()) ? at(i) : intslot(0);
            auto b = (i < other.length()) ? other.at(i) : intslot(0);
            if (a != b) diff = true;    
        }
        return diff;
    }

    symint operator+(const symint& other) const {
        sreturn_if_both_infinite symint(Infinite);
        sreturn_if_both_nan symint(Nan);
    
        if (negative == other.negative) {
            symint result;
            size_t maxLength = std::max(length(), other.length());
            result.slots.resize(maxLength);
    
            for (size_t i = 0; i < maxLength; i++) {
                intslot a = (i < length()) ? at(i) : intslot(0);
                intslot b = (i < other.length()) ? other.at(i) : intslot(0);
                result.slots[i] = a + b;
            }

            // result shares the same sign as the operands
            result.negative = negative;
            return result.simplify();
        } else {
            return *this - (-other);
        }
    }

    symint operator-(const symint& other) const {
        sreturn_if_both_nan symint(Nan);
        sreturn_if_both(Infinite) symint(); // 0
        if (special == Infinite) return symint(Infinite);
        if (other.special == Infinite) return - symint(Infinite);
    
        if (negative != other.negative) {
            return *this + (-other);

        } else if (*this < other) {
            return -(other - *this);

        } else {
            symint result(0);
            size_t maxLength = std::max(length(), other.length());
            result.slots.resize(maxLength);
    
            for (size_t i = 0; i < maxLength; i++) {
                intslot a = (i < length()) ? at(i) : intslot(0);
                intslot b = (i < other.length()) ? other.at(i) : intslot(0);
                result.slots[i] = a - b;
            }
            result.negative = negative;
            return result.simplify();
        }
    }
    
    symint operator*(const symint& other) const {
        sreturn_if_both_nan symint(Nan);
        sreturn_if_both(Infinite) symint(Infinite);
        if (special == Infinite || other.special == Infinite) {
            if (isZero() || other.isZero()) return symint(Nan);
            return symint(Infinite).setNegative(negative != other.negative);
        }
    
        symint result;
        size_t maxLength = length() + other.length();
        result.slots.resize(maxLength);
    
        for (size_t i = 0; i < length(); i++) {
            for (size_t j = 0; j < other.length(); j++) {
                result.slots[i + j] = result.slots[i + j] + at(i) * other.at(j);
            }
        }
        result.negative = (negative != other.negative);
        return result.simplify();
    }
    
    symint operator/(const symint& other) const {
        sreturn_if_both_nan symint(Nan);
        if (other.isZero()) return symint(Nan);
        if (special == Infinite) return symint(Infinite).setNegative(negative != other.negative);
        if (other.special == Infinite) return symint(); // 0
    
        // Simplified division logic (integer division)
        symint result;
        symint remainder = *this;
        result.negative = (negative != other.negative);
    
        while (remainder >= other) {
            remainder = remainder - other;
            result = result + symint(1);
        }
        return result.simplify();
    }
    
    symint operator%(const symint& other) const {
        sreturn_if_both_nan symint(Nan);
        if (other.isZero()) return symint(Nan);
        if (special == Infinite) return symint(Nan);
        if (other.special == Infinite) return *this;
    
        symint remainder = *this;
        while (remainder >= other) {
            remainder = remainder - other;
        }
        return remainder;
    }

    bool operator>(const symint& other) const {
        if(length() > other.length()) return true;
        if(length() < other.length()) return false;

        for(size_t i = length(); i > 0; i--) {
            if(at(i-1) > other.at(i-1)) return true;
            if(at(i-1) < other.at(i-1)) return false;
        }
        return false;
    }

    bool operator<(const symint& other) const {
        if(length() < other.length()) return true;
        if(length() > other.length()) return false;

        for(size_t i = length(); i > 0; i--) {
            if(at(i-1) < other.at(i-1)) return true;
            if(at(i-1) > other.at(i-1)) return false;
        }
        return false;
    }

    bool operator>=(const symint& other) const { return !(*this < other); }
    bool operator<=(const symint& other) const { return !(*this > other); }
    bool operator!=(const symint& other) const { return !(*this == other); }

    symint operator+=(const symint& other) { return (*this = *this + other); }
    symint operator-=(const symint& other) { return (*this = *this - other); }
    symint operator*=(const symint& other) { return (*this = *this * other); }
    symint operator/=(const symint& other) { return (*this = *this / other); }
    symint operator%=(const symint& other) { return (*this = *this % other); }

    symint operator++() { return (*this += symint(1)); }
    symint operator--() { return (*this -= symint(1)); }

    /// @brief Really fast multiplication by 10.
    /// @warning Not bitwise, instead based on 10.
    symint operator<<(int shift) const {
        symint result(0);
        result.slots.resize(length() + shift);
        std::copy(slots.begin(), slots.end(), result.slots.begin() + shift);
        return result;
    }

    symint operator<<(const symint& shift) const {
        if (shift.isNegative()) throw std::runtime_error("symint::operator<<: negative shift not allowed");
        if (shift.isInfinite()) throw std::runtime_error("symint::operator<<: infinite shift not allowed");
        if (shift.isNan()) throw std::runtime_error("symint::operator<<: NaN shift not allowed");


        
    }

    /// @brief Really fast division by 10.
    /// @warning Not bitwise, instead based on 10. Will discard digits that goes below 0. 
    symint operator>>(int shift) const {
        if(shift >= length()) return symint(0);
        symint result(0);
        result.slots.resize(length() - shift);
        std::copy(slots.begin() + shift, slots.end(), result.slots.begin());
        return result;
    }

    // remove leading zeros
    symint& simplify() {
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

    symint operator-() const {
        if (special == Nan) return symint(Nan); // Negative Nan is still Nan
        // infinite is also reverse, merge into normal process
        symint result = *this;
        result.negative = !negative; // reverse symbol
        return result;
    }
    
    bool isNegative() const { return (special != Nan) && negative; }
    
    symint& setNegative(bool isNegative) {
        negative = isNegative;
        return *this;
    }

    /// @brief Check if this symint fits in a standard integral type.
    template<typename T>
    requires std::is_integral_v<T>
    bool fits_in() const {
        if (special == Nan)
            return std::numeric_limits<T>::has_quiet_NaN;
        if (special == Infinite)
            return std::numeric_limits<T>::has_infinity;
        if (isZero())
            return true;

        symint simplified = *this;
        simplified.simplify();

        if constexpr (std::is_unsigned_v<T>) {
            if (simplified.negative) return false;
        }

        symint max_val = symint::from_string(std::to_string(std::numeric_limits<T>::max()));

        if constexpr (std::is_signed_v<T>) {
            if (simplified.negative) {
                symint min_val = symint::from_string(std::to_string(std::numeric_limits<T>::min()));
                return simplified >= min_val;
            }
        }
        return simplified <= max_val;
    }

    /// @brief Convert this symint to a standard integral type (must fit).
    template<typename T>
    requires std::is_integral_v<T>
    T as() const {
        if (special == Nan) {
            if constexpr (std::numeric_limits<T>::has_quiet_NaN)
                return std::numeric_limits<T>::quiet_NaN();
            else
                throw std::runtime_error("symint::as: NaN cannot be represented");
        }
        if (special == Infinite) {
            if constexpr (std::numeric_limits<T>::has_infinity)
                return negative ? -std::numeric_limits<T>::infinity()
                                : std::numeric_limits<T>::infinity();
            else
                throw std::runtime_error("symint::as: Infinity cannot be represented");
        }
        if (!fits_in<T>())
            throw std::runtime_error("symint::as: value does not fit in target type");

        T result = 0;
        for (size_t i = length(); i > 0; --i)
            result = static_cast<T>(result * 10 + static_cast<uint8_t>(at(i - 1)));
        return negative ? static_cast<T>(-result) : result;
    }

    bool isInfinite() const { return special == Infinite; }
    bool isNan() const { return special == Nan; }

    std::string to_string() const {
        std::string result;
        if(negative) result += '-';
        if(length() == 0) return "0";
        if(special == Infinite) return "inf";
        if(special == Nan) return "nan";
        for(size_t i = length(); i > 0; i--) {
            result += (slots.at(i-1) + '0');
        }
        return result;
    }

protected:
    std::vector<intslot> slots;
    bool negative;
    special special = Null;
};

std::istream& operator>>(std::istream& in, symint& other) {
    std::string str;
    in >> str;
    other = symint::from_string(str);
    return in;
}

std::ostream& operator<<(std::ostream& out, const symint& other) {
    out << other.to_string();
    return out;
}


class symfloat {

public:
    symfloat() {}
    symfloat(int32_t integer, int32_t fraction) : integer(integer), fraction(fraction) {}
    symfloat(int32_t integer) : integer(integer), fraction(0) {}
    symfloat(const symint& integer, const symint& fraction) : integer(integer), fraction(fraction) {}
    symfloat(const symint& integer) : integer(integer), fraction(0) {}

    template<typename T>
    requires std::is_floating_point_v<T>
    symfloat(T value) {
        *this = from_string(std::to_string(T));
    }

    symfloat(special state) : integer(state), fraction(state), negative(false) {}

    static symfloat from_string(const std::string& str) {
        symfloat result;

        if(size_t dotpos = str.find('.'); dotpos != std::string::npos) {
            result.integer = symint::from_string(str.substr(0, dotpos));
            result.fraction = symint::from_string(str.substr(dotpos + 1));
        } else {
            result.integer = symint::from_string(str);
        }

        return result;
    }

    static symfloat from_string_expo(const std::string& str) {
        if (str.empty()) return symint();

        symint result;
        // extract the number part and the exponent part
        if (size_t pos = str.find_first_of("eE"); pos != std::string::npos) {
            std::string num_part = str.substr(0, pos);
            std::string exp_part = str.substr(pos + 1);

            symfloat base = from_string(num_part);
            
            int exponent;
            try {
                exponent = std::stoi(exp_part);
            } catch (const std::invalid_argument&) {
                throw std::runtime_error("symint::from_string_expo: invalid exponent");
            } catch (const std::out_of_range&) {
                throw std::runtime_error("symint::from_string_expo: exponent out of range");
            }

            // calculate base * 10^exponent
            symfloat result = base;
            return (exponent > 0) ? result << exponent : result >> -exponent;
        } else {
            throw std::runtime_error("symint::from_string_expo: no exponent found");
        }
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
    symfloat operator+(const symfloat& other) const {
        sreturn_if_both_nan symfloat(Nan);
        if (special == Infinite || other.special == Infinite) {
            if (special == Infinite && other.special == Infinite) {
                return (negative == other.negative) ? *this : symfloat(Nan);
            }
            return (special == Infinite) ? *this : other;
        }

        symfloat result;
        result.integer = integer + other.integer;
        result.fraction = fraction + other.fraction;

        // 如果小数部分溢出，进位到整数部分
        if (result.fraction.length() > fraction.length()) {
            result.integer += symint(1);
            result.fraction.simplify();
        }

        return result;
    }

    // 减法运算符
    symfloat operator-(const symfloat& other) const {
        sreturn_if_both_nan symfloat(Nan);
        if (special == Infinite || other.special == Infinite) {
            if (special == Infinite && other.special == Infinite) {
                return (negative == other.negative) ? symfloat(0) : symfloat(Nan);
            }
            return (special == Infinite) ? *this : -other;
        }

        symfloat result;
        result.integer = integer - other.integer;
        result.fraction = fraction - other.fraction;

        // 如果小数部分借位，调整整数部分
        if (result.fraction.isNegative()) {
            result.integer -= symint(1);
            result.fraction += symint(10); // 假设小数部分是以 10 为基数
        }

        return result;
    }

    // 乘法运算符
    symfloat operator*(const symfloat& other) const {
        sreturn_if_both_nan symfloat(Nan);
        if (special == Infinite || other.special == Infinite) {
            if (isZero() || other.isZero()) return symfloat(Nan);
            return symfloat(Infinite).setNegative(negative != other.negative);
        }

        symfloat result;
        result.integer = integer * other.integer;
        result.fraction = (integer * other.fraction) + (fraction * other.integer);

        return result;
    }

    // 除法运算符
    symfloat operator/(const symfloat& other) const {
        sreturn_if_both_nan symfloat(Nan);
        if (other.isZero()) return symfloat(Nan);
        if (special == Infinite) return symfloat(Infinite).setNegative(negative != other.negative);
        if (other.special == Infinite) return symfloat(0);

        symfloat result;
        result.integer = integer / other.integer;
        result.fraction = (integer % other.integer) / other.fraction;

        return result;
    }

    // 取模运算符
    symfloat operator%(const symfloat& other) const {
        sreturn_if_both_nan symfloat(Nan);
        if (other.isZero()) return symfloat(Nan);
        if (special == Infinite) return symfloat(Nan);
        if (other.special == Infinite) return *this;

        symfloat result;
        result.integer = integer % other.integer;
        result.fraction = fraction % other.fraction;

        return result;
    }

    // 比较运算符
    bool operator==(const symfloat& other) const {
        if (special != other.special) return false;
        sreturn_if_both_nan false;
        sreturn_if_both_infinite (negative == other.negative);
        return (integer == other.integer) && (fraction == other.fraction) && (negative == other.negative);
    }

    bool operator!=(const symfloat& other) const {
        return !(*this == other);
    }

    bool operator<(const symfloat& other) const {
        if (special == Nan || other.special == Nan) return false;
        sreturn_if_both_infinite [&] {
            if (negative == other.negative) return false; // the same sign, equal is not less than
            if (negative && !other.negative) return true; // -inf < +inf
            if (!negative && other.negative) return false; // +inf > -inf
        }();
        if (negative != other.negative) return negative; // negative number is less than positive number

        if (integer != other.integer) return (negative ? integer > other.integer : integer < other.integer);
        return (negative ? fraction > other.fraction : fraction < other.fraction);
    }

    bool operator<=(const symfloat& other) const {
        return (*this < other) || (*this == other);
    }

    bool operator>(const symfloat& other) const {
        return !(*this <= other);
    }

    bool operator>=(const symfloat& other) const {
        return !(*this < other);
    }

    // 复合赋值运算符
    symfloat& operator+=(const symfloat& other) { return *this = *this + other; }
    symfloat& operator-=(const symfloat& other) { return *this = *this - other; }
    symfloat& operator*=(const symfloat& other) { return *this = *this * other; }
    symfloat& operator/=(const symfloat& other) { return *this = *this / other; }
    symfloat& operator%=(const symfloat& other) { return *this = *this % other; }

    // 判断是否为零
    bool isZero() const {
        return integer.isZero() && fraction.isZero();
    }

    // 赋值运算符
    symfloat& operator=(const symfloat& other) {
        if (this != &other) {
            integer = other.integer;
            fraction = other.fraction;
            negative = other.negative;
            special = other.special;
        }
        return *this;
    }

    // 取反运算符
    symfloat operator-() const {
        symfloat result = *this;
        result.negative = !negative;
        return result;
    }

    bool isNegative() const { return negative; }

    // 设置符号
    symfloat& setNegative(bool isNegative) {
        negative = isNegative;
        return *this;
    }

    bool isInfinite() const { return special == Infinite; }
    bool isNan() const { return special == Nan; }

    /// @brief Fast multiplication by 10^shift. Moves digits from fraction to integer.
    /// @warning Leading zeros in fraction are not preserved (pre-existing limitation).
    symfloat operator<<(int shift) const {
        if (shift < 0) return *this >> (-shift);
        if (shift == 0) return *this;
        if (special == Nan || special == Infinite) return *this;

        symfloat result = *this;
        int flen = result.fraction.length();

        if (flen == 0) {
            result.integer = result.integer << shift;
            return result;
        }

        if (shift >= flen) {
            // All fraction digits move to integer, pad with zeros
            result.integer = (result.integer << shift) + (result.fraction << (shift - flen));
            result.fraction = symint(0);
        } else {
            // Move top 'shift' (most significant) digits from fraction to integer
            symint top = result.fraction >> (flen - shift);
            result.integer = (result.integer << shift) + top;
            // Remove those digits from fraction
            result.fraction = result.fraction - (top << (flen - shift));
        }

        return result;
    }

    /// @brief Fast division by 10^shift. Moves digits from integer to fraction.
    /// @warning Leading zeros in fraction are not preserved (pre-existing limitation).
    symfloat operator>>(int shift) const {
        if (shift < 0) return *this << (-shift);
        if (shift == 0) return *this;
        if (special == Nan || special == Infinite) return *this;

        symfloat result = *this;
        int ilen = result.integer.length();

        if (ilen == 0) return result;

        if (shift >= ilen) {
            // All integer digits become part of fraction; integer becomes 0.
            // fraction = old_integer * 10^old_flen + old_fraction
            int flen = result.fraction.length();
            result.fraction = (result.integer << flen) + result.fraction;
            result.integer = symint(0);
        } else {
            // Move bottom 'shift' (least significant) digits from integer to fraction
            // Extract least significant 'shift' digits: integer - (integer >> shift) << shift
            symint bottom = result.integer - ((result.integer >> shift) << shift);
            result.integer = result.integer >> shift;

            // Prepend bottom digits to fraction:
            // new_fraction = bottom * 10^flen + old_fraction
            int flen = result.fraction.length();
            result.fraction = (bottom << flen) + result.fraction;
        }

        return result;
    }

    friend symfloat floor(const symfloat& _ref, const int32_t lev);
    friend symfloat log(const symfloat& _ref, const symfloat& base);
    friend symint round(const symfloat& _ref);
    friend symint ceil(const symfloat& _ref);
    friend symint truncate(const symfloat& _ref);

protected:
    symint integer;
    symint fraction;

    bool negative;
    special special;
};

// 输入流运算符
std::istream& operator>>(std::istream& in, symfloat& other) {
    std::string str;
    in >> str;
    other = symfloat::from_string(str);
    return in;
}

// 输出流运算符
std::ostream& operator<<(std::ostream& out, const symfloat& other) {
    out << other.to_string();
    return out;
}

template<typename T>
concept symarray_supported = 
    requires(T a, T b) {
        { a + b } -> std::convertible_to<T>;
        { a - b } -> std::convertible_to<T>;
        { a * b } -> std::convertible_to<T>;
        { a / b } -> std::convertible_to<T>;
        { a > b } -> std::convertible_to<bool>;
        { a < b } -> std::convertible_to<bool>;
        { a == b } -> std::convertible_to<bool>;
    };

template<typename T>
concept has_from_string = requires(const std::string& str) {
    { T::from_string(str) } -> std::convertible_to<T>;
};

template<typename T>
concept has_to_string = 
std::is_class_v<T> && requires(const T& obj) { { obj.to_string() } -> std::convertible_to<std::string>; }
|| requires(const T& val) { { std::to_string(val) } -> std::convertible_to<std::string>; };

// currently only support symint, but it would be easy to cast any container
// into this array and allow math operations on it.
// This is not really part of the symbolic math library. Should be moved to maths
// module in the future.
template<typename T>
requires symarray_supported<T>
class symarray : public std::vector<T>
{
public:
    symarray() {}
    symarray(int size, const T& value) : std::vector<T>(size, value) {}
    symarray(const std::initializer_list<T>& init) : std::vector<T>(init) {}
    symarray(const std::vector<T>& vec) : std::vector<T>(vec) {}

    T max() const {
        if (empty()) throw std::runtime_error("symarray::max: empty array");
        T maxValue = at(0);
        for (const T& value : *this) {
            if (value > maxValue) {
                maxValue = value;
            }
        }
        return maxValue;
    }

    T min() const {
        if (empty()) throw std::runtime_error("symarray::min: empty array");
        T minValue = at(0);
        for (const T& value : *this) {
            if (value < minValue) {
                minValue = value;
            }
        }
        return minValue;
    }
    
    T sum() const {
        T total(0);
        for (const T& value : *this) {
            total += value;
        }
        return total;
    }

    T average() const {
        if (empty()) throw std::runtime_error("symarray::average: empty array");
        T total = sum();
        return total / static_cast<int>(size());
    }

    T median() const {
        if (empty()) throw std::runtime_error("symarray::median: empty array");
        symarray<T> sortedArray = *this;
        std::sort(sortedArray.begin(), sortedArray.end());
        size_t mid = size() / 2;
        if (size() % 2 == 0) {
            return (sortedArray[mid - 1] + sortedArray[mid]) / 2;
        } else {
            return sortedArray[mid];
        }
    }

    T product() const {
        T total(1);
        for (const T& value : *this) {
            total *= value;
        }
        return total;
    }

    symarray sort() const {
        symarray<T> sortedArray = *this;
        std::sort(sortedArray.begin(), sortedArray.end());
        return sortedArray;
    }

    static symarray from_stringlist(const scl2::stringlist& strlist)
        requires has_from_string<T>
    {
        symarray<T> result;
        for (const std::string& str : strlist) {
            T value = T::from_string(str);
            result.push_back(value);
        }
        return result;
    }

    /// @brief Create a symarray from a string, splitting by common delimiters.
    /// @warning supported splits are: ",", " ", "\t", "\n" 
    static symarray from_string(const std::string& str)
        requires has_from_string<T>
    {
        symarray<T> result;
        scl2::stringlist strlist(str, scl2::stringlist({","," ","\t","\n"}));
        strlist.remove_empty();
        for (const std::string& s : strlist) {
            T value = T::from_string(s);
            result.push_back(value);
        }
        return result;
    }

    template<typename U = T>
    requires has_to_string<U>
    std::string T_to_string(const U& value) {
        if (constexpr requires(const U& obj) {{obj.to_string()}}) {
            return value.to_string();
        } else if (constexpr requires(const U& val) {{std::to_string(val)}}) {
            return std::to_string(value);
        } else {
            static_assert(false, "Type T does not have a to_string() method or std::to_string() overload.");
        }
    }

    scl2::stringlist to_stringlist() const
        requires has_to_string<T>
    {
        scl2::stringlist result;
        for (const T& value : *this) {
            result.push_back(value.to_string());
        }
        return result;
    }

    scl2::to_string() const
        requires has_to_string<T>
    {
        return to_stringlist().join();
    }

};

} // namespace scl2::sym