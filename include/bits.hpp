/*
    Bits module for SharedCppLib2.

    This module provides a series of utilities for bit manipulation.

    With optimizations, this module should be as efficient as direct
    bit manipulation on the underlying type, while providing a more
    convenient interface for working with bits.

    This module is header-only.

*/

#pragma once
#include <cstddef>
#include <type_traits>

namespace bits {

struct bits {
    std::byte b;

    inline constexpr void put(size_t pos, bool value = true) {
        if (value) {
            b |= (std::byte{1} << pos);
        } else {
            b &= ~(std::byte{1} << pos);
        }
    }

    inline constexpr bool get(size_t pos) {
        return (b & (std::byte{1} << pos)) != std::byte{0};
    }

    inline constexpr bits rotate(int off) const {
        while(off < 0) {
            off += 8;
        }
        off %= 8;
        return bits{std::byte((static_cast<unsigned char>(b) << off) | (static_cast<unsigned char>(b) >> (8 - off)))};
    }

    inline constexpr bits shift(int off) const {
        // shifted out of range, so all bits are lost.
        if (off < -8 || off > 8) return { std::byte{0} };
        if (off < 0) {
            return bits{std::byte(static_cast<unsigned char>(b) >> (-off))};
        } else {
            return bits{std::byte(static_cast<unsigned char>(b) << off)};
        }
    }
};


template <typename T>
    requires std::is_trivially_copyable_v<T>
struct ebits {
    const size_t bitSize = sizeof(T) * 8;
    const size_t size = sizeof(T);
    std::byte bs[sizeof(T)];

    ebits() = default;
    ebits(const T& value) {
        std::memcpy(bs, &value, size);
    }

    inline T value() const {
        T result;
        std::memcpy(&result, bs, size);
        return result;
    }

    inline constexpr void put(size_t pos, bool value = true) {
        size_t byI = pos / 8, biI = pos % 8;
        if (byI >= size) return;

        if(value) {
            bs[byI] |= (std::byte{1} << biI);
        } else {
            bs[byI] &= ~(std::byte{1} << biI);
        }
    }

    inline constexpr bool get(size_t pos) {
        size_t byI = pos / 8, biI = pos % 8;
        if (byI >= size) return false;

        return (bs[byI] & (std::byte{1} << biI)) != std::byte{0};
    }

    inline constexpr ebits rotate(int off) const {
        ebits result;
        for (size_t i = 0; i < bitSize; i++) {
            if (get(i)) {
                result.put((i + off) % bitSize);
            }
        }
        return result;
    }

    inline constexpr ebits shift(int off) const {
        ebits result;
        for (size_t i = 0; i < bitSize; i++) {
            if (get(i)) {
                size_t newPos = static_cast<int>(i) + off;
                if (newPos < bitSize) {
                    result.put(newPos);
                }
            }
        }
        return result;
    }

};


} // namespace bits