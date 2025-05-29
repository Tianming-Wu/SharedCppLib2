/*
    High Precision Algorithms (hpalgo)

    namespace: none
    functions: pow, sqrt, log, exp, sin, cos, tan, asin, acos, atan
    
    namespace: hpconstants
    constants: pi, e, phi
*/

#pragma once
#include "hpint.hpp"
#include "hparray.hpp"

namespace hpconstants {

const hpfloat pi = hpfloat::from_string("3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086513282306647093844609550582231725359408128481117450284102701938521105559644622948954930381964428810975665933446128475648233786783165271201909145648566923460348610454326648213393607260249141273724587006606315588174881520920962829254091715364367892590360011330530548820466521384146951941511609433057270365759591953092186117381932611793105118548074462379962749567351885752724891227938183011949128831426076896280457");
const hpfloat e = hpfloat::from_string("2.718281828459045235360287471352662497757247093699959574966967627724076630353547594573993959431049128831426076896280457");
const hpfloat phi = hpfloat::from_string("1.6180339887498948482045868343656381177203091807649921875");

} // namespace hpconstants


/// @brief 幂函数
hpint pow(const hpint& _ref, unsigned int lev) {
    if(lev == 0) return 1;
    hpint result(_ref);
    for(int c = 1; c <= lev; c++) result *= _ref;
    return result;
}

/// @brief 平方根函数
hpint sqrt(const hpint& _ref) {
    if(_ref.isZero()) return 0;
    if(_ref.isNegative()) throw std::runtime_error("hpint::sqrt: negative number");
    hpint result(0);
    hpint low(0), high(_ref);
    while(low <= high) {
        hpint mid = (low + high) / 2;
        hpint square = mid * mid;
        if(square == _ref) return mid;
        if(square < _ref) {
            result = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return result;
}

/// @brief 保留特定位数，小数点坐标为0 
hpfloat floor(const hpfloat& _ref, const int32_t lev) {
    if (lev > 0) throw std::runtime_error("hpfloat::floor: level must be non-positive");
    hpint integer = _ref.integer;
    hpint fraction = _ref.fraction;

    // 计算需要保留的小数位数
    int32_t keepDigits = -lev;
    if (fraction.length() > keepDigits) {
        fraction = fraction >> (fraction.length() - keepDigits); // 舍去多余位数
    } else {
        fraction = 0; // 小数部分不足时直接舍去
    }

    return hpfloat(integer, fraction);
}

/// @brief 四舍五入函数
hpint round(const hpfloat& _ref) {
    hpint result = _ref.integer;
    if (!_ref.fraction.isZero() && _ref.fraction >= hpint(5)) {
        result += (_ref.negative ? -1 : 1); // 四舍五入
    }
    return result;
}

/// @brief 向上取整函数
hpint ceil(const hpfloat& _ref) {
    if (_ref.isNan()) return hpint(hpcalc::Nan);
    if (_ref.isInfinite()) return hpint(hpcalc::Infinite);
    if (_ref.isZero()) return hpint(0);

    // 计算向上取整
    hpint result = _ref.integer;
    if (!_ref.fraction.isZero()) {
        result += (_ref.negative ? -1 : 1); // 向上取整
    }
    return result;
}

/// @brief 向下取整函数
hpint truncate(const hpfloat& _ref) {
    if (_ref.isNan()) return hpint(hpcalc::Nan);
    if (_ref.isInfinite()) return hpint(hpcalc::Infinite);
    if (_ref.isZero()) return hpint(0);

    // 计算截断
    hpint result = _ref.integer;
    if (!_ref.fraction.isZero()) {
        result += (_ref.negative ? -1 : 1); // 截断
    }
    return result;
}

/// @brief 自然对数函数
hpfloat ln(const hpfloat& _ref) {
    if (_ref.isNan() || _ref<= 0) throw std::runtime_error("hpfloat::ln: invalid argument");
    if (_ref.isInfinite()) return hpfloat(hpcalc::Infinite);

    // 简单占位实现，实际需要数值方法
    hpfloat result(0);
    // TODO: 实现自然对数的数值计算
    return result;
}

/// @brief 对数函数 
hpfloat log(const hpfloat& _ref, const hpfloat& base) {
    if (_ref.isZero() || base.isZero()) throw std::runtime_error("hpfloat::log: zero argument");
    if (_ref < 0 || base < 0) throw std::runtime_error("hpfloat::log: negative argument");
    if (base == 1) throw std::runtime_error("hpfloat::log: base 1");
    if (_ref == 1) return hpfloat(0); // log_a(1) = 0

    // 使用换底公式 log_a(b) = ln(b) / ln(a)
    return ln(_ref) / ln(base);
}

/// @brief 指数函数
hpfloat exp(const hpfloat& _ref) {
    if (_ref.isNan()) return hpfloat(hpcalc::Nan);
    if (_ref.isInfinite()) return hpfloat(hpcalc::Infinite);

    // 简单占位实现，实际需要数值方法
    hpfloat result(0);
}

hpfloat abs(const hpfloat& _ref) {
    if (_ref.isNan()) return hpfloat(hpcalc::Nan);
    if (_ref.isInfinite()) return hpfloat(hpcalc::Infinite);
    return _ref.isNegative()? -_ref : _ref;
}

hpint abs(const hpint& _ref) {
    if (_ref.isNan()) return hpint(hpcalc::Nan);
    if (_ref.isInfinite()) return hpint(hpcalc::Infinite);
    return _ref.isNegative()? -_ref : _ref;
}

hpint min(const hpint& a, const hpint& b) {
    if (a < b) return a;
    return b;
}

hpint max(const hpint& a, const hpint& b) {
    if (a > b) return a;
    return b;
}

hpfloat min(const hpfloat& a, const hpfloat& b) {
    if (a < b) return a;
    return b;
}

hpfloat max(const hpfloat& a, const hpfloat& b) {
    if (a > b) return a;
    return b;
}