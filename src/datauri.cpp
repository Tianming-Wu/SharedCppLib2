#include "datauri.hpp"

#include <sstream>
#include <iomanip>
#include <cctype>

#include "Base64.hpp"

namespace scl2 {

// ============================================================
//  Encoding
// ============================================================

std::string data_uri::encode(const std::bytearray& data) const
{
    // Binary data is always written as base64 — hex in data URIs is non-standard.
    std::string encoded = data.toBase64();

    std::ostringstream oss;
    oss << "data:";
    if (!media_type_.empty()) {
        oss << media_type_;
    }
    oss << ";base64," << encoded;
    return oss.str();
}

std::string data_uri::encode(const std::string& data) const
{
    std::ostringstream oss;
    oss << "data:";
    if (!media_type_.empty()) {
        oss << media_type_;
    }
    if (is_base64_) {
        oss << ";base64," << Base64::encode(data);
    } else {
        oss << "," << percent_encode(data);
    }
    return oss.str();
}

// ============================================================
//  Parsing / Decoding
// ============================================================

data_uri data_uri::parse(const std::string& uri)
{
    // Must start with "data:"
    if (!uri.starts_with("data:")) {
        throw std::runtime_error("data_uri::parse: URI does not start with 'data:'");
    }

    // Find the data separator (comma)
    size_t comma_pos = uri.find(',', 5); // skip "data:"
    if (comma_pos == std::string::npos) {
        throw std::runtime_error("data_uri::parse: missing comma separator in data URI");
    }

    // Extract the metadata portion: everything between "data:" and the comma
    std::string meta = uri.substr(5, comma_pos - 5); // after "data:"

    bool is_base64 = false;
    std::string media_type;

    if (meta.ends_with(";base64")) {
        is_base64 = true;
        // Strip ";base64" from the end of meta
        media_type = meta.substr(0, meta.size() - 7);
    } else {
        media_type = meta;
    }

    // media_type may be empty (which is valid)
    data_uri result(std::move(media_type), is_base64);
    return result;
}

std::bytearray data_uri::decode(const std::string& uri)
{
    // Must start with "data:"
    if (!uri.starts_with("data:")) {
        throw std::runtime_error("data_uri::decode: URI does not start with 'data:'");
    }

    // Find the comma that separates metadata from data
    size_t comma_pos = uri.find(',', 5);
    if (comma_pos == std::string::npos) {
        throw std::runtime_error("data_uri::decode: missing comma separator in data URI");
    }

    // Determine encoding from metadata
    std::string meta = uri.substr(5, comma_pos - 5);
    bool is_base64 = meta.ends_with(";base64");

    // Extract and decode the data portion
    std::string data = uri.substr(comma_pos + 1);

    if (is_base64) {
        return std::bytearray::fromBase64(data);
    } else {
        return std::bytearray(percent_decode(data));
    }
}

// ============================================================
//  Percent-encoding helpers
// ============================================================

std::string data_uri::percent_encode(const std::string& raw)
{
    std::ostringstream oss;
    for (unsigned char ch : raw) {
        // Unreserved characters (RFC 3986) are kept as-is.
        // Everything else is percent-encoded.
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            oss << ch;
        } else {
            oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(ch);
            oss << std::nouppercase << std::dec;
        }
    }
    return oss.str();
}

std::string data_uri::percent_decode(const std::string& encoded)
{
    std::string result;
    result.reserve(encoded.size());

    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            // Parse two hex digits
            auto hex_to_nibble = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                return 0;
            };
            int hi = hex_to_nibble(encoded[i + 1]);
            int lo = hex_to_nibble(encoded[i + 2]);
            result += static_cast<char>((hi << 4) | lo);
            i += 2;
        } else {
            result += encoded[i];
        }
    }
    return result;
}

// ============================================================
//  inline_data_uri
// ============================================================

inline_data_uri inline_data_uri::from_string(const std::string& uri)
{
    auto meta = data_uri::parse(uri);
    auto data = data_uri::decode(uri);
    return inline_data_uri(meta.media_type(), meta.is_base64(), std::move(data));
}

std::string inline_data_uri::to_string() const
{
    return encode(data_);
}

} // namespace scl2