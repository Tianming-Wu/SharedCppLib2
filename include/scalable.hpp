/*
    Scalable function series for easy scaling of class/struct members.

    This module is made for high-dpi support, in which case you may
    need frequently calculate some coordinates or sizes scaled by a
    certain factor.

    Note:
        If you are making something with high dpi support, it's suggested
        to keep everything logical (unscaled, or to say they are the exact
        value when the scale is 100%) in your code, and only scale them when
        you are "talking" to the system, in which case physical instead of
        logical values are required.

        And, always make sure that any value is only scaled once. In a chain,
        you might only scale before using it, and only scale the copy of the
        struct in the smallest scope you're using it.

        However, for some cases if you want a better performance, you might
        want to cache the scaled value. In this case, remember that you should
        regenerate from the original value instead of unscaling and re-scaling
        the cached value.

        Also, if not necessary, avoid using unscale(). And in the logical side,
        consider using double to avoid precision loss, which occurs when a value
        is unscaled then scaled again, and you might lost one or two pixels.
*/


#pragma once

// ======================== SCALABLE 宏系列 ========================
// 用法: 在类/结构体中使用 SCALABLE(member1, member2, ...) 
// 会自动生成 scale() 和 unscale() 方法
// 示例: struct Rect {
//           int x, y, w, h;
//           SCALABLE(x, y, w, h)
//       };

// 私有辅助宏 - 处理单个成员缩放
#define _SCALE_ONE(member, factor) result.member *= factor;
#define _UNSCALE_ONE(member, factor) result.member /= factor;

// ===== 支持 1 到 8 个成员的具体宏 =====
#define SCALABLE_1(m1) \
    inline auto scale(double scalingFactor) const { \
        auto result = *this; \
        _SCALE_ONE(m1, scalingFactor) \
        return result; \
    } \
    inline auto unscale(double scalingFactor) const { \
        auto result = *this; \
        _UNSCALE_ONE(m1, scalingFactor) \
        return result; \
    }

#define SCALABLE_2(m1, m2) \
    inline auto scale(double scalingFactor) const { \
        auto result = *this; \
        _SCALE_ONE(m1, scalingFactor) \
        _SCALE_ONE(m2, scalingFactor) \
        return result; \
    } \
    inline auto unscale(double scalingFactor) const { \
        auto result = *this; \
        _UNSCALE_ONE(m1, scalingFactor) \
        _UNSCALE_ONE(m2, scalingFactor) \
        return result; \
    }

#define SCALABLE_3(m1, m2, m3) \
    inline auto scale(double scalingFactor) const { \
        auto result = *this; \
        _SCALE_ONE(m1, scalingFactor) \
        _SCALE_ONE(m2, scalingFactor) \
        _SCALE_ONE(m3, scalingFactor) \
        return result; \
    } \
    inline auto unscale(double scalingFactor) const { \
        auto result = *this; \
        _UNSCALE_ONE(m1, scalingFactor) \
        _UNSCALE_ONE(m2, scalingFactor) \
        _UNSCALE_ONE(m3, scalingFactor) \
        return result; \
    }

#define SCALABLE_4(m1, m2, m3, m4) \
    inline auto scale(double scalingFactor) const { \
        auto result = *this; \
        _SCALE_ONE(m1, scalingFactor) \
        _SCALE_ONE(m2, scalingFactor) \
        _SCALE_ONE(m3, scalingFactor) \
        _SCALE_ONE(m4, scalingFactor) \
        return result; \
    } \
    inline auto unscale(double scalingFactor) const { \
        auto result = *this; \
        _UNSCALE_ONE(m1, scalingFactor) \
        _UNSCALE_ONE(m2, scalingFactor) \
        _UNSCALE_ONE(m3, scalingFactor) \
        _UNSCALE_ONE(m4, scalingFactor) \
        return result; \
    }

#define SCALABLE_5(m1, m2, m3, m4, m5) \
    inline auto scale(double scalingFactor) const { \
        auto result = *this; \
        _SCALE_ONE(m1, scalingFactor) \
        _SCALE_ONE(m2, scalingFactor) \
        _SCALE_ONE(m3, scalingFactor) \
        _SCALE_ONE(m4, scalingFactor) \
        _SCALE_ONE(m5, scalingFactor) \
        return result; \
    } \
    inline auto unscale(double scalingFactor) const { \
        auto result = *this; \
        _UNSCALE_ONE(m1, scalingFactor) \
        _UNSCALE_ONE(m2, scalingFactor) \
        _UNSCALE_ONE(m3, scalingFactor) \
        _UNSCALE_ONE(m4, scalingFactor) \
        _UNSCALE_ONE(m5, scalingFactor) \
        return result; \
    }

#define SCALABLE_6(m1, m2, m3, m4, m5, m6) \
    inline auto scale(double scalingFactor) const { \
        auto result = *this; \
        _SCALE_ONE(m1, scalingFactor) \
        _SCALE_ONE(m2, scalingFactor) \
        _SCALE_ONE(m3, scalingFactor) \
        _SCALE_ONE(m4, scalingFactor) \
        _SCALE_ONE(m5, scalingFactor) \
        _SCALE_ONE(m6, scalingFactor) \
        return result; \
    } \
    inline auto unscale(double scalingFactor) const { \
        auto result = *this; \
        _UNSCALE_ONE(m1, scalingFactor) \
        _UNSCALE_ONE(m2, scalingFactor) \
        _UNSCALE_ONE(m3, scalingFactor) \
        _UNSCALE_ONE(m4, scalingFactor) \
        _UNSCALE_ONE(m5, scalingFactor) \
        _UNSCALE_ONE(m6, scalingFactor) \
        return result; \
    }

#define SCALABLE_7(m1, m2, m3, m4, m5, m6, m7) \
    inline auto scale(double scalingFactor) const { \
        auto result = *this; \
        _SCALE_ONE(m1, scalingFactor) \
        _SCALE_ONE(m2, scalingFactor) \
        _SCALE_ONE(m3, scalingFactor) \
        _SCALE_ONE(m4, scalingFactor) \
        _SCALE_ONE(m5, scalingFactor) \
        _SCALE_ONE(m6, scalingFactor) \
        _SCALE_ONE(m7, scalingFactor) \
        return result; \
    } \
    inline auto unscale(double scalingFactor) const { \
        auto result = *this; \
        _UNSCALE_ONE(m1, scalingFactor) \
        _UNSCALE_ONE(m2, scalingFactor) \
        _UNSCALE_ONE(m3, scalingFactor) \
        _UNSCALE_ONE(m4, scalingFactor) \
        _UNSCALE_ONE(m5, scalingFactor) \
        _UNSCALE_ONE(m6, scalingFactor) \
        _UNSCALE_ONE(m7, scalingFactor) \
        return result; \
    }

#define SCALABLE_8(m1, m2, m3, m4, m5, m6, m7, m8) \
    inline auto scale(double scalingFactor) const { \
        auto result = *this; \
        _SCALE_ONE(m1, scalingFactor) \
        _SCALE_ONE(m2, scalingFactor) \
        _SCALE_ONE(m3, scalingFactor) \
        _SCALE_ONE(m4, scalingFactor) \
        _SCALE_ONE(m5, scalingFactor) \
        _SCALE_ONE(m6, scalingFactor) \
        _SCALE_ONE(m7, scalingFactor) \
        _SCALE_ONE(m8, scalingFactor) \
        return result; \
    } \
    inline auto unscale(double scalingFactor) const { \
        auto result = *this; \
        _UNSCALE_ONE(m1, scalingFactor) \
        _UNSCALE_ONE(m2, scalingFactor) \
        _UNSCALE_ONE(m3, scalingFactor) \
        _UNSCALE_ONE(m4, scalingFactor) \
        _UNSCALE_ONE(m5, scalingFactor) \
        _UNSCALE_ONE(m6, scalingFactor) \
        _UNSCALE_ONE(m7, scalingFactor) \
        _UNSCALE_ONE(m8, scalingFactor) \
        return result; \
    }

// ======================== 参数计数宏（MSVC 兼容） ========================
// MSVC 的预处理器在处理 __VA_ARGS__ 时有特殊行为，需要额外的展开层

// 强制展开宏 - 这对 MSVC 很关键
#define _SCALABLE_EXPAND(x) x

// 参数计数宏 - 返回参数个数
#define _SCALABLE_NARGS_X(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define _SCALABLE_NARGS(...) _SCALABLE_EXPAND(_SCALABLE_NARGS_X(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1))

// 字符串拼接宏 - 拼接 SCALABLE_ 和参数个数
#define _SCALABLE_CONCAT_IMPL(NAME, N) NAME##N
#define _SCALABLE_CONCAT(NAME, N) _SCALABLE_CONCAT_IMPL(NAME, N)

// 应用宏 - 将计数结果和参数传递给对应的 SCALABLE_N
#define _SCALABLE_APPLY(N, ...) _SCALABLE_EXPAND(_SCALABLE_CONCAT(SCALABLE_, N)(__VA_ARGS__))

// 主宏 - 自动计数并调用对应版本
#define SCALABLE(...) _SCALABLE_APPLY(_SCALABLE_NARGS(__VA_ARGS__), __VA_ARGS__)