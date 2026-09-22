/*
    Network I/O module — file upload/download over HTTP.

    High-level file transfer helpers built on the network_http client,
    exposing simple bytearray-based APIs:

        scl2::bytearray download(const std::string& url);                 // GET -> bytes
        void download_file(const std::string& url, const fs::path& local); // GET -> file
        network::http::response upload(const std::string& url, const bytearray& data, ...); // POST
        network::http::response upload_file(const std::string& url, const fs::path& local, ...);

    http:// is supported natively (plain TCP on port 80). https:// requires a
    TLS transport — provided by the SCL2Ext openssl integration — and is
    intentionally rejected here to keep SharedCppLib2 dependency-free.

    Note on the "active/passive" question: HTTP uses a plain request/response
    model — the client always initiates the connection and the server merely
    replies. There is no separate data channel, so no active/passive handling
    is needed (that concept belongs to FTP, which is not part of this module).
*/

#pragma once

#include "http.hpp"      // network::http::response
#include "bytearray.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace network::io {

/// @brief Parsed URL components for an http(s) URL.
struct url_info {
    std::string scheme;   ///< "http" or "https" (lowercase)
    std::string host;     ///< host name or IP
    uint16_t port = 0;    ///< resolved default if the URL has no explicit port
    std::string path;     ///< includes query string, if any
};

/// @brief Parse an http(s) URL.
/// @throw std::invalid_argument if the URL is malformed or the scheme is unsupported.
url_info parse_url(const std::string& url);

/// @brief Download the whole resource at @p url as a bytearray (HTTP GET).
/// @throw network::network_error / std::runtime_error on failure.
scl2::bytearray download(const std::string& url);

/// @brief Download @p url and write it to @p local_path.
/// @throw network::network_error / std::runtime_error on failure.
void download_file(const std::string& url, const std::filesystem::path& local_path);

/// @brief Upload raw bytes to @p url (HTTP POST).
/// @return The server response (caller checks .status).
network::http::response upload(const std::string& url,
                               const scl2::bytearray& data,
                               const std::string& content_type = "application/octet-stream");

/// @brief Upload the file at @p local_path to @p url (HTTP POST).
/// @return The server response (caller checks .status).
network::http::response upload_file(const std::string& url,
                                    const std::filesystem::path& local_path,
                                    const std::string& content_type = "application/octet-stream");

} // namespace network::io
