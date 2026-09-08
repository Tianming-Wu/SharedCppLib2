#pragma once
#include "apibase.hpp"
#include <istream>
#include <ostream>

/// Compression / decompression API.
///
/// A protocol layer letting codec providers (zlib / raw deflate, etc.) plug
/// into a common interface, mirroring hash_api.hpp and encryption_api.hpp.
///
/// A provider T may expose:
///   - one-shot, static:    T::compress(data) / T::decompress(data)
///   - one-shot, instance:  default-constructible T with member
///                          compress(data) / decompress(data)
///                          (lets a provider carry level / dictionary state)
///   - streaming:           typename T::stream_type constructible from
///                          codec_dir, with
///                            update(const bytearray&) -> bytearray  // emitted
///                            end()                 -> bytearray    // flushed tail
///
/// The one-shot helper functions are named `compress` / `decompress`
/// (no `generic_` prefix): they already live inside namespace scl2, so the
/// prefix would only add noise. The whole-stream helpers `streamed_compress`
/// / `streamed_decompress` pull an entire istream through the codec into an
/// ostream. Note these are one-shot actions (not stream objects); a true
/// composable filter-stream object may come later.

namespace scl2 {

/// Direction for a streamed codec instance.
enum class codec_dir : bool { Compress = true, Decompress = false };

// ─── Static one-shot: T::compress(data) / T::decompress(data) ────────

template<typename T>
concept has_static_compression = requires {
    requires std::is_class_v<T>
    && requires {
        { T::compress(std::declval<const scl2::bytearray&>()) } -> std::same_as<scl2::bytearray>;
    };
};

template<typename T>
concept has_static_decompression = requires {
    requires std::is_class_v<T>
    && requires {
        { T::decompress(std::declval<const scl2::bytearray&>()) } -> std::same_as<scl2::bytearray>;
    };
};

// ─── Instance one-shot: T().compress(data) / T().decompress(data) ────

template<typename T>
concept has_instance_compression = requires {
    requires std::is_class_v<T>
    && requires {
        { T().compress(std::declval<const scl2::bytearray&>()) } -> std::same_as<scl2::bytearray>;
    };
};

template<typename T>
concept has_instance_decompression = requires {
    requires std::is_class_v<T>
    && requires {
        { T().decompress(std::declval<const scl2::bytearray&>()) } -> std::same_as<scl2::bytearray>;
    };
};

template<typename T>
concept has_compression_support = has_static_compression<T> || has_instance_compression<T>;

template<typename T>
concept has_decompression_support = has_static_decompression<T> || has_instance_decompression<T>;

// ─── One-shot helpers ─────────────────────────────────────────────────

template<typename T>
requires has_compression_support<T>
scl2::bytearray compress(const scl2::bytearray& data) {
    if constexpr (has_static_compression<T>) {
        return T::compress(data);
    } else {
        return T().compress(data);
    }
}

template<typename T>
requires has_decompression_support<T>
scl2::bytearray decompress(const scl2::bytearray& data) {
    if constexpr (has_static_decompression<T>) {
        return T::decompress(data);
    } else {
        return T().decompress(data);
    }
}

#define scl2_check_compression_support(T) static_assert(::scl2::has_compression_support<T>, "Type " #T " does not support compression");
#define scl2_check_decompression_support(T) static_assert(::scl2::has_decompression_support<T>, "Type " #T " does not support decompression");

// ─── Streaming concepts ───────────────────────────────────────────────

template<typename T>
concept has_streamed_compression = requires {
    requires std::is_class_v<T>
    && requires {
        typename T::stream_type;
        requires std::is_class_v<typename T::stream_type>
        && requires(typename T::stream_type h) {
            { typename T::stream_type(codec_dir::Compress) };
            { h.update(std::declval<const scl2::bytearray&>()) } -> std::same_as<scl2::bytearray>;
            { h.end() } -> std::same_as<scl2::bytearray>;
        };
    };
};

template<typename T>
concept has_streamed_decompression = requires {
    requires std::is_class_v<T>
    && requires {
        typename T::stream_type;
        requires std::is_class_v<typename T::stream_type>
        && requires(typename T::stream_type h) {
            { typename T::stream_type(codec_dir::Decompress) };
            { h.update(std::declval<const scl2::bytearray&>()) } -> std::same_as<scl2::bytearray>;
            { h.end() } -> std::same_as<scl2::bytearray>;
        };
    };
};

#define scl2_check_streamed_compression(T) static_assert(::scl2::has_streamed_compression<T>, "Type " #T " does not support streamed compression");
#define scl2_check_streamed_decompression(T) static_assert(::scl2::has_streamed_decompression<T>, "Type " #T " does not support streamed decompression");

// ─── Whole-stream helpers (one-shot actions over istream → ostream) ────

template<typename T>
requires has_streamed_compression<T>
void streamed_compress(std::istream& input, std::ostream& output) {
    constexpr size_t bufsz = generic_buffer_size<T>();
    typename T::stream_type codec(codec_dir::Compress);
    scl2::bytearray buffer(bufsz);

    while (input) {
        buffer.resize(bufsz);
        input.read(reinterpret_cast<char*>(buffer.data()),
                   static_cast<std::streamsize>(bufsz));
        buffer.resize(static_cast<size_t>(input.gcount()));
        auto out = codec.update(buffer);
        if (!out.empty())
            output.write(reinterpret_cast<const char*>(out.data()),
                         static_cast<std::streamsize>(out.size()));
    }
    auto last = codec.end();
    if (!last.empty())
        output.write(reinterpret_cast<const char*>(last.data()),
                     static_cast<std::streamsize>(last.size()));
}

template<typename T>
requires has_streamed_decompression<T>
void streamed_decompress(std::istream& input, std::ostream& output) {
    constexpr size_t bufsz = generic_buffer_size<T>();
    typename T::stream_type codec(codec_dir::Decompress);
    scl2::bytearray buffer(bufsz);

    while (input) {
        buffer.resize(bufsz);
        input.read(reinterpret_cast<char*>(buffer.data()),
                   static_cast<std::streamsize>(bufsz));
        buffer.resize(static_cast<size_t>(input.gcount()));
        auto out = codec.update(buffer);
        if (!out.empty())
            output.write(reinterpret_cast<const char*>(out.data()),
                         static_cast<std::streamsize>(out.size()));
    }
    auto last = codec.end();
    if (!last.empty())
        output.write(reinterpret_cast<const char*>(last.data()),
                     static_cast<std::streamsize>(last.size()));
}

} // namespace scl2
