/*
    Data URI Scheme Module for SharedCppLib2

    RFC 2397 — The "data" URL scheme
    Format: data:[<media-type>][;base64],<data>

    Examples:
      data:;base64,SGVsbG8=
      data:image/png;base64,iVBORw0KGgo...
      data:text/html,<h1>Hello</h1>
*/

#pragma once

#include <string>
#include <stdexcept>

#include "bytearray.hpp"

namespace scl2 {

class data_uri {
public:
    data_uri() = default;
    explicit data_uri(std::string media_type, bool base64 = false)
        : media_type_(std::move(media_type)), is_base64_(base64) {}

    // ---- Encode: build a data: URI from raw data ----

    // Encode binary data (always uses base64).
    std::string encode(const std::bytearray& data) const;

    // Encode text data (uses percent-encoding when not base64).
    std::string encode(const std::string& data) const;

    // ---- Parse: extract info from a data: URI ----

    // Parse a full data: URI string into a data_uri object (metadata only).
    // Throws std::runtime_error on invalid input.
    static data_uri parse(const std::string& uri);

    // Decode only the <data> portion back to raw bytes.
    // Auto-detects base64 vs percent-encoding from the prefix.
    // Throws std::runtime_error on invalid input.
    static std::bytearray decode(const std::string& uri);

    // ---- Accessors ----

    const std::string& media_type() const { return media_type_; }
    bool is_base64() const { return is_base64_; }

    // The fixed URI scheme prefix.
    static constexpr const char* scheme = "data:";

protected:
    // Percent-encode a raw string for use in the data portion.
    static std::string percent_encode(const std::string& raw);

    // Percent-decode a string back to raw bytes.
    static std::string percent_decode(const std::string& encoded);

    bool is_base64_ = false;
    std::string media_type_; // e.g. "image/png", "text/html", or empty for default
};


// A data_uri with the decoded payload included.
// Use this when you need to hold both metadata and raw data together.
class inline_data_uri : public data_uri {
public:
    inline_data_uri() = default;

    inline_data_uri(std::string media_type, bool base64, std::bytearray data)
        : data_uri(std::move(media_type), base64), data_(std::move(data)) {}

    // Parse a full data: URI string and store both metadata and decoded payload.
    // Throws std::runtime_error on invalid input.
    static inline_data_uri from_string(const std::string& uri);

    // Re-encode back to a data: URI string.
    std::string to_string() const;

    // Access the raw decoded payload.
    const std::bytearray& data() const { return data_; }
    std::bytearray& data() { return data_; }

private:
    std::bytearray data_;
};

} // namespace scl2