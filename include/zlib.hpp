/*
    zlib codec module for SharedCppLib2 — a self-contained, dependency-free
    DEFLATE / zlib implementation (RFC 1950 + RFC 1951).

    It is deliberately lightweight: compression uses fixed-Huffman blocks with
    a bounded LZ77 match finder, and automatically falls back to stored
    (uncompressed) blocks when compression would not help. Decompression is a
    full DEFLATE inflate (stored / fixed / dynamic blocks), so any standard
    zlib / gzip / PNG-produced stream can be decoded.

    The class `scl2::zlib` is a provider for compression_api.hpp:

      - one-shot:  zlib::compress(data) / zlib::decompress(data)
      - streaming: zlib::stream_type(codec_dir) + update() + end()

    Output format is a zlib stream (2-byte header + raw DEFLATE + Adler-32),
    the same container PNG uses.
*/

#pragma once

#include "compression_api.hpp"
#include "bytearray.hpp"

namespace scl2 {

/// @brief zlib (RFC1950) compression provider.
class zlib {
public:
    /// Streaming block size hint used by generic_buffer_size().
    static constexpr size_t block_size = 32768;

    /// @brief Compress into a zlib stream.
    static scl2::bytearray compress(const scl2::bytearray& data);

    /// @brief Decompress a zlib stream back to the original bytes.
    /// @throws std::runtime_error if the data is not a valid zlib stream.
    static scl2::bytearray decompress(const scl2::bytearray& data);

    /// @brief Streaming codec. Compress direction emits DEFLATE blocks as
    ///        input is fed (constant memory); end() flushes the tail block.
    ///        Decompress direction buffers until a full zlib stream is
    ///        present, then emits the decoded bytes.
    class stream_type {
    public:
        stream_type(codec_dir dir);
        ~stream_type();

        stream_type(const stream_type&) = delete;
        stream_type& operator=(const stream_type&) = delete;

        /// @brief Feed a chunk; returns bytes produced (possibly empty).
        scl2::bytearray update(const scl2::bytearray& data);
        /// @brief Finish the stream; returns the flushed tail (if any).
        scl2::bytearray end();

    private:
        struct Impl;
        Impl* m_;
    };
};

} // namespace scl2
