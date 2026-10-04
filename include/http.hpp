/*
    HTTP Protocol shared classes and functions for both client and server.
    Tianming Wu <https://github.com/Tianming-Wu> 2026.2.9

    Supports HTTP/1.0 and HTTP/1.1, with basic request parsing and serialization.
    Response bodies may be framed by Content-Length, by Transfer-Encoding:
    chunked (decoded transparently in network::http::client), or read until
    the connection closes. Future versions may add support for HTTP/2 and HTTP/3.

*/

#pragma once

#include "network.hpp"

#include <map>
#include <string>
#include <optional>


namespace network::http {

// Another Windows macro
#undef DELETE

enum class http_method {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    PATCH,
    TRACE,
    CONNECT,
    CUSTOM_METHOD // Will be supported in the future, allowing user to send custom messages.
};

/// @brief HTTP status codes
/// @note This is the set of codes the library names; a response that carries
///       any other code keeps it in `status_code` and reports `UNKNOWN` here.
enum class http_status {
    UNKNOWN = 0,
    OK = 200,
    CREATED = 201,
    ACCEPTED = 202,
    NO_CONTENT = 204,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    NOT_MODIFIED = 304,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    REQUEST_TIMEOUT = 408,
    CONFLICT = 409,
    LENGTH_REQUIRED = 411,
    PAYLOAD_TOO_LARGE = 413,
    URI_TOO_LONG = 414,
    TOO_MANY_REQUESTS = 429,
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503,
    GATEWAY_TIMEOUT = 504,
};

/// @brief HTTP request type
struct request {
    http_method method;
    std::string path;   // e.g. "/index.html"
    std::string http_version; // e.g. "HTTP/1.1"
    std::map<std::string, std::string> headers;
    std::string body;

    std::string serialize() const;
    static request deserialize(const std::string& str);
    
    /// @brief Check if request has complete headers (ends with \r\n\r\n)
    static bool has_complete_headers(const std::string& str);
    
    /// @brief Get required body length from Content-Length header, returns 0 if not present
    size_t get_content_length() const;
};

/// @brief HTTP response type
struct response {
    http_status status = http_status::OK;

    /// @brief The code exactly as it came in, or 0 for a response that was built
    ///        rather than parsed. Serialization prefers this over `status`.
    int status_code = 0;

    std::string http_version = "HTTP/1.1";
    std::map<std::string, std::string> headers;
    std::string body;

    std::string serialize() const;
    static response deserialize(const std::string& str);
    
    /// @brief Check if response has complete headers (ends with \r\n\r\n)
    static bool has_complete_headers(const std::string& str);
    
    /// @brief Helper to create a simple text response
    static response make_text(http_status status, const std::string& text);
    
    /// @brief Helper to create a JSON response
    static response make_json(http_status status, const std::string& json);
};

/// @brief Convert HTTP method enum to string
std::string method_to_string(http_method method);

/// @brief Convert string to HTTP method enum
http_method string_to_method(const std::string& str);

/// @brief Get status code description
std::string status_to_string(http_status status);

/// @brief Get the description of a raw status code; empty when it is unknown.
std::string status_to_string(int status_code);




} // namespace network::http