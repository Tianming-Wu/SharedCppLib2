/*
    HTTP Protocol shared classes and functions for both client and server.
    Tianming Wu <https://github.com/Tianming-Wu> 2026.2.9

    Supports HTTP/1.0 and HTTP/1.1, with basic request parsing and serialization.
    Future versions may add support for HTTP/2 and HTTP/3, as well as more
    advanced features like chunked transfer encoding, content negotiation, etc.

*/

#pragma once

#include "network.hpp"

#include <map>
#include <string>
#include <optional>


namespace network::http {

#ifdef OS_WINDOWS
using socket_t = SOCKET;
constexpr socket_t invalid_socket = INVALID_SOCKET;
#else
using socket_t = int;
constexpr socket_t invalid_socket = -1;
#endif

enum class http_method {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    PATCH,
    TRACE,
    CONNECT
};

/// @brief HTTP request type
struct request {
    http_method method; // e.g. http_method::GET, http_method::POST
    std::map<std::string, std::string> headers; // e.g. {"Host": "example.com", "User-Agent": "MyClient/1.0"}
    std::optional<std::string> path;   // e.g. "/index.html"
    std::optional<std::string> body;   // request body for POST/PUT requests

    std::string serialize() const; // serialize the request to a string for sending over the network
    static request deserialize(const std::string& str); // parse a raw HTTP request string into a request object
};




} // namespace network::http