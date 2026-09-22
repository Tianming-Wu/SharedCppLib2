/*
    Compression layer for SharedCppLib2.

    compression_api.hpp describes what a compression provider looks like as a type. This
    header adds the parts needed to pick one while the program runs: a holder that erases
    the type, and an identifier that a file can record.

    classes: scl2::compression_provider, scl2::compression_id
    types:   scl2::compress_algo
    link target: SharedCppLib2::compression
*/

#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

#include "bytearray.hpp"
#include "compression_api.hpp"

namespace scl2 {

// ══ Identifiers ══════════════════════════════════════════════════════

/// The compression algorithms the library ships. The values are what a file records, so
/// they keep their meaning once assigned.
enum class compress_algo : uint8_t {
    none = 0,
    zlib = 1,
};

/// Identifiers from here up belong to applications. The range below is kept for the
/// library, so a later release can add an algorithm without colliding with one an
/// application picked.
inline constexpr uint8_t user_compression_base = 128;

/// Names a compression algorithm the way a file records it: one of the built-ins, or one an
/// application registered under an identifier of its own.
///
/// The identifier says nothing about what the algorithm does. Two programs agree on what a
/// file contains only if they agree on what each identifier means.
class compression_id {
public:
    constexpr compression_id() noexcept = default;
    constexpr compression_id(compress_algo algo) noexcept : m_id(static_cast<uint8_t>(algo)) {}
    constexpr explicit compression_id(uint8_t id) noexcept : m_id(id) {}

    constexpr uint8_t value() const noexcept { return m_id; }

    /// The identifier that means "nothing was applied".
    constexpr bool isNone() const noexcept { return m_id == 0; }

    /// In the library's range, where the meaning is fixed.
    constexpr bool isBuiltin() const noexcept { return m_id < user_compression_base; }

    /// In the application's range.
    constexpr bool isUser() const noexcept { return !isBuiltin(); }

    friend constexpr bool operator==(const compression_id&, const compression_id&) = default;

private:
    uint8_t m_id = 0;
};

// ══ Type-erased provider ═════════════════════════════════════════════

/// Holds a pair of compress / decompress functions without saying what type is behind them.
///
/// The library matches providers by type, which is fine until the choice has to be made
/// while the program runs. This is what to hold in that case.
class compression_provider {
public:
    using function_type = std::function<scl2::bytearray(const scl2::bytearray&)>;

    compression_provider() = default;

    /// Wraps a provider as compression_api.hpp describes them.
    template<typename T>
    requires has_compression_support<T> && has_decompression_support<T>
    static compression_provider from()
    {
        return from(
            [](const scl2::bytearray& data) { return scl2::compress<T>(data); },
            [](const scl2::bytearray& data) { return scl2::decompress<T>(data); });
    }

    /// Wraps a pair given directly. Either half may be left empty.
    static compression_provider from(function_type compress, function_type decompress)
    {
        compression_provider provider;
        provider.m_compress = std::move(compress);
        provider.m_decompress = std::move(decompress);
        return provider;
    }

    bool hasCompression() const noexcept { return static_cast<bool>(m_compress); }
    bool hasDecompression() const noexcept { return static_cast<bool>(m_decompress); }

    /// Both halves are there.
    bool isUsable() const noexcept { return hasCompression() && hasDecompression(); }

    /// @throws std::runtime_error when there is no compress function.
    scl2::bytearray compress(const scl2::bytearray& data) const
    {
        if (!m_compress) throw std::runtime_error("scl2::compression_provider: no compress function");
        return m_compress(data);
    }

    /// @throws std::runtime_error when there is no decompress function.
    scl2::bytearray decompress(const scl2::bytearray& data) const
    {
        if (!m_decompress) throw std::runtime_error("scl2::compression_provider: no decompress function");
        return m_decompress(data);
    }

    /// Runs a probe through both halves and reports whether what came back is what went in.
    ///
    /// Nothing in the library calls this on its own; it is here for a caller that assembled
    /// a pair itself and wants to check it before trusting data to it.
    ///
    /// @note It only says the two halves agree on this probe. A pair that disagrees on other
    ///       input, and a decompressor that expects a different algorithm, both pass. Pass a
    ///       probe of your own when the data has a shape worth testing.
    /// @note An exception from either half counts as a failure; it is not propagated.
    [[nodiscard]] bool verify() const { return verify(defaultProbe()); }
    [[nodiscard]] bool verify(const scl2::bytearray& probe) const;

    /// A small buffer that is half repetitive and half varying, for verify() to work with.
    static scl2::bytearray defaultProbe();

private:
    function_type m_compress;
    function_type m_decompress;
};

// ══ Inline definitions ═══════════════════════════════════════════════

inline bool compression_provider::verify(const scl2::bytearray& probe) const
{
    if (!isUsable()) return false;

    try {
        return m_decompress(m_compress(probe)) == probe;
    }
    catch (...) {
        return false;
    }
}

inline scl2::bytearray compression_provider::defaultProbe()
{
    scl2::bytearray probe;
    probe.reserve(256);

    // A repetitive half, so a codec that only handles plain copies is not enough.
    for (uint32_t i = 0; i < 128; ++i)
        probe.append(std::byte{static_cast<unsigned char>((i / 16) * 13)});

    // A varying half, so a codec that only handles runs is not enough either. The sequence
    // is a plain LCG: all it has to be is reproducible.
    uint32_t state = 0x12345678u;
    for (uint32_t i = 0; i < 128; ++i) {
        state = state * 1664525u + 1013904223u;
        probe.append(std::byte{static_cast<unsigned char>(state >> 24)});
    }

    return probe;
}

} // namespace scl2
