/*
    Symbolic Math Algorithms (symalgo)

    namespace: none
    functions: pow, sqrt, log, exp, sin, cos, tan, asin, acos, atan
    
    namespace: hpconstants
    constants: pi, e, phi
*/

#pragma once
#include "symmath.hpp"

namespace scl2::sym {

// namespace hpconstants {

// const symfloat pi = symfloat::from_string("3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086513282306647093844609550582231725359408128481117450284102701938521105559644622948954930381964428810975665933446128475648233786783165271201909145648566923460348610454326648213393607260249141273724587006606315588174881520920962829254091715364367892590360011330530548820466521384146951941511609433057270365759591953092186117381932611793105118548074462379962749567351885752724891227938183011949128831426076896280457");
// const symfloat e = symfloat::from_string("2.718281828459045235360287471352662497757247093699959574966967627724076630353547594573993959431049128831426076896280457");
// const symfloat phi = symfloat::from_string("1.6180339887498948482045868343656381177203091807649921875");

// } // namespace hpconstants


// This global structure hold the settings of the symbolic math algorithms.
// Since it is not possible to maintain the full precision with certain functions
// (like iteration depth), we need to know when to stop.
struct __symalgo_trait {
    size_t max_iteration_depth = 1000;


} symalgo_trait;


/// @brief 幂函数
symint pow(const symint& _ref, unsigned int lev) {
    if(lev == 0) return 1;
    symint result(_ref);
    for(int c = 1; c <= lev; c++) result *= _ref;
    return result;
}

/// @brief 平方根函数
symint sqrt(const symint& _ref) {
    if(_ref.isZero()) return 0;
    if(_ref.isNegative()) throw std::runtime_error("symint::sqrt: negative number");
    symint result(0);
    symint low(0), high(_ref);
    while(low <= high) {
        symint mid = (low + high) / 2;
        symint square = mid * mid;
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
///TODO: 修改让它支持保留大于 10^0 的级别。
symfloat floor(const symfloat& _ref, const int32_t lev) {
    if (lev > 0) throw std::runtime_error("symfloat::floor: level must be non-positive");
    symint integer = _ref.integer;
    symint fraction = _ref.fraction;

    // 计算需要保留的小数位数
    int32_t keepDigits = -lev;
    if (fraction.length() > keepDigits) {
        fraction = fraction >> (fraction.length() - keepDigits); // 舍去多余位数
    } else {
        fraction = 0; // 小数部分不足时直接舍去
    }

    return symfloat(integer, fraction);
}

/// @brief 四舍五入函数
symint round(const symfloat& _ref) {
    symint result = _ref.integer;
    if (!_ref.fraction.isZero() && _ref.fraction >= symint(5)) {
        result += (_ref.negative ? -1 : 1); // 四舍五入
    }
    return result;
}

/// @brief 向上取整函数
symint ceil(const symfloat& _ref) {
    if (_ref.isNan()) return symint(Nan);
    if (_ref.isInfinite()) return symint(Infinite);
    if (_ref.isZero()) return symint(0);

    // 计算向上取整
    symint result = _ref.integer;
    if (!_ref.fraction.isZero()) {
        result += (_ref.negative ? -1 : 1); // 向上取整
    }
    return result;
}

/// @brief 向下取整函数
symint truncate(const symfloat& _ref) {
    if (_ref.isNan()) return symint(Nan);
    if (_ref.isInfinite()) return symint(Infinite);
    if (_ref.isZero()) return symint(0);

    // 计算截断
    symint result = _ref.integer;
    if (!_ref.fraction.isZero()) {
        result += (_ref.negative ? -1 : 1); // 截断
    }
    return result;
}

/// @brief 自然对数函数
symfloat ln(const symfloat& _ref) {
    if (_ref.isNan() || _ref <= 0) throw std::runtime_error("symfloat::ln: invalid argument");
    if (_ref.isInfinite()) return symfloat(Infinite);
    if (_ref == symfloat(1)) return symfloat(0); // ln(1) = 0

    // 使用泰勒级数展开计算自然对数
    // ln(x) = 2 * [ (x-1)/(x+1) + 1/3 * ((x-1)/(x+1))^3 + 1/5 * ((x-1)/(x+1))^5 + ... ]
    
    symfloat x = _ref;
    
    // 如果 x 不在 [0.5, 2] 范围内，先进行缩放
    int scale = 0;
    while (x > symfloat(2)) {
        x = x / symfloat(2);
        scale++;
    }
    while (x < symfloat(0.5)) {
        x = x * symfloat(2);
        scale--;
    }
    
    // 现在 x 在 [0.5, 2] 范围内，计算 ln(x)
    symfloat term = (x - symfloat(1)) / (x + symfloat(1));
    symfloat term_sq = term * term;
    symfloat current_term = term;
    symfloat result = current_term;
    
    // 泰勒级数展开
    const int iterations = 50; // 迭代次数，可根据精度需求调整
    for (int n = 1; n < iterations; n++) {
        current_term = current_term * term_sq;
        symfloat denominator = symfloat(2 * n + 1);
        result = result + current_term / denominator;
    }
    
    result = result * symfloat(2);
    
    // 加上缩放因子: ln(x * 2^scale) = ln(x) + scale * ln(2)
    symfloat ln2 = symfloat::from_string("0.6931471805599453094172321214581765680755001343602552541206800094933936219696947156058633269964186875");
    result = result + (symfloat(scale) * ln2);
    
    return result;
}

/// @brief 对数函数 
symfloat log(const symfloat& _ref, const symfloat& base) {
    if (_ref.isZero() || base.isZero()) throw std::runtime_error("symfloat::log: zero argument");
    if (_ref < 0 || base < 0) throw std::runtime_error("symfloat::log: negative argument");
    if (base == 1) throw std::runtime_error("symfloat::log: base 1");
    if (_ref == 1) return symfloat(0); // log_a(1) = 0

    // 使用换底公式 log_a(b) = ln(b) / ln(a)
    return ln(_ref) / ln(base);
}

/// @brief 指数函数
symfloat exp(const symfloat& _ref) {
    if (_ref.isNan()) return symfloat(Nan);
    if (_ref.isInfinite()) {
        if (_ref.isNegative()) return symfloat(0); // exp(-∞) = 0
        return symfloat(Infinite); // exp(+∞) = +∞
    }
    
    // 使用泰勒级数展开: exp(x) = 1 + x + x^2/2! + x^3/3! + ...
    symfloat result(1);
    symfloat term(1);
    symfloat x = _ref;
    
    const int iterations = 50; // 迭代次数
    for (int n = 1; n < iterations; n++) {
        term = term * x / symfloat(n);
        result = result + term;
        
        // 如果项变得很小，提前终止
        if (abs(term) < symfloat::from_string("0.00000000000000000000000000000000000000000000000001")) {
            break;
        }
    }
    
    return result;
}

symfloat abs(const symfloat& _ref) {
    if (_ref.isNan()) return symfloat(Nan);
    if (_ref.isInfinite()) return symfloat(Infinite);
    return _ref.isNegative()? -_ref : _ref;
}

symint abs(const symint& _ref) {
    if (_ref.isNan()) return symint(Nan);
    if (_ref.isInfinite()) return symint(Infinite);
    return _ref.isNegative()? -_ref : _ref;
}

symint min(const symint& a, const symint& b) {
    if (a < b) return a;
    return b;
}

symint max(const symint& a, const symint& b) {
    if (a > b) return a;
    return b;
}

symfloat min(const symfloat& a, const symfloat& b) {
    if (a < b) return a;
    return b;
}

symfloat max(const symfloat& a, const symfloat& b) {
    if (a > b) return a;
    return b;
}

} // namespace scl2::sym