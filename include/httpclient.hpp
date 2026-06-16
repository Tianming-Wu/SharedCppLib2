/*
    HTTP Client module as part of the network library.
    Tianming Wu <https://github.com/Tianming-Wu> 2026.2.9

    This module provides an HTTP protocol layer on top of TCP client,
    with request building, response parsing, and common HTTP operations.
*/

#pragma once

#include "tcpclient.hpp"
#include "http.hpp"

#include <string>
#include <chrono>
#include <optional>

namespace network::http {

/// @brief HTTP client for making HTTP requests to remote servers
class client
{
public:
    client();
    ~client();

    enable_move_only(client)

    /// @brief Connect to a server (hostname or IP)
    /// @param host Server hostname or IP address
    /// @param port Server port (default: 80 for HTTP)
    /// @return true if connection successful
    bool connect(const std::string& host, uint16_t port = 80);
    
    /// @brief Disconnect from the server
    void disconnect();
    
    /// @brief Check if connected to a server
    bool is_connected() const;
    
    /// @brief Send a GET request
    /// @param path Request path (e.g., "/index.html")
    /// @param headers Optional additional headers
    /// @return Response object
    response get(const std::string& path, 
                 const std::map<std::string, std::string>& headers = {});
    
    /// @brief Send a POST request
    /// @param path Request path
    /// @param body Request body content
    /// @param content_type Content-Type header (default: "application/x-www-form-urlencoded")
    /// @param headers Optional additional headers
    /// @return Response object
    response post(const std::string& path, 
                  const std::string& body,
                  const std::string& content_type = "application/x-www-form-urlencoded",
                  const std::map<std::string, std::string>& headers = {});
    
    /// @brief Send a custom HTTP request
    /// @param req Request object
    /// @return Response object
    response send_request(const request& req);
    
    /// @brief Set timeout for receiving responses
    /// @param timeout Timeout duration (default: 30 seconds)
    void set_timeout(std::chrono::milliseconds timeout);
    
    /// @brief Get current timeout setting
    std::chrono::milliseconds get_timeout() const;
    
    /// @brief Get the connected server hostname
    std::string server_host() const;
    
    /// @brief Get the connected server port
    uint16_t server_port() const;

private:
    /// @brief Build request with Host header
    request build_request(http_method method, const std::string& path,
                         const std::string& body = "",
                         const std::map<std::string, std::string>& headers = {});
    
    /// @brief Receive response from server (blocks until complete or timeout)
    response receive_response();

    tcp::client m_tcp_client;
    std::string m_host;
    uint16_t m_port = 80;
    std::chrono::milliseconds m_timeout = std::chrono::seconds(30);
};

} // namespace network::http
