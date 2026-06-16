/*
    HTTP Server module as part of the network library.
    Tianming Wu <https://github.com/Tianming-Wu> 2026.2.15

    This module provides an HTTP protocol layer on top of TCP server,
    similar to QHttpServer, with request parsing, routing, and response building.

    Note that this is an individual link target and is not included in the main
    network library by default.
    You don't need to link network module separately when using http server module.
*/

#pragma once

#include "tcpserver.hpp"
#include "http.hpp"

#include <functional>
#include <map>
#include <string>
#include <memory>

namespace network::http {

class server;

/// @brief HTTP route handler function type
/// Takes a request and returns a response body (status and headers can be set separately)
using route_handler = std::function<std::string(const request&)>;

/// @brief HTTP server that handles HTTP protocol on top of TCP
class server {
public:
    server();
    server(uint16_t port);
    server(network_address address, uint16_t port);
    ~server();

    /// @brief Start the HTTP server
    void start();
    void start(uint16_t port);
    
    /// @brief Stop the HTTP server
    void stop();

    /// @brief Register a route handler for a specific path and method
    /// @param method HTTP method (GET, POST, etc.)
    /// @param path URL path (e.g., "/api/users")
    /// @param handler Function to handle the request
    void route(http_method method, const std::string& path, route_handler handler);

    /// @brief Process one iteration of the server loop (accept connections, handle requests)
    /// @return Number of new clients accepted, or -1 if not running
    int tick();

    uint16_t port() const;
    network_address address() const;

private:
    /// @brief Handle incoming data from a TCP client
    void handleClient(tcp::client_id id);

    tcp::server m_tcp_server;
    std::map<std::pair<http_method, std::string>, route_handler> m_routes;
    std::map<tcp::client_id, std::string> m_client_buffers; // Partial HTTP requests
};

} // namespace network::http
