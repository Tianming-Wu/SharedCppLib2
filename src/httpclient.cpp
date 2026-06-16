#include "httpclient.hpp"

#include <chrono>
#include <thread>

namespace network::http {

client::client()
{
}

client::~client()
{
    disconnect();
}

bool client::connect(const std::string& host, uint16_t port)
{
    m_host = host;
    m_port = port;
    return m_tcp_client.connect(host, port);
}

void client::disconnect()
{
    m_tcp_client.disconnect();
}

bool client::is_connected() const
{
    return m_tcp_client.is_connected();
}

response client::get(const std::string& path, 
                    const std::map<std::string, std::string>& headers)
{
    request req = build_request(http_method::GET, path, "", headers);
    return send_request(req);
}

response client::post(const std::string& path, 
                     const std::string& body,
                     const std::string& content_type,
                     const std::map<std::string, std::string>& headers)
{
    auto full_headers = headers;
    full_headers["Content-Type"] = content_type;
    
    request req = build_request(http_method::POST, path, body, full_headers);
    return send_request(req);
}

response client::send_request(const request& req)
{
    if (!is_connected()) {
        throw network_error("Not connected to server");
    }
    
    // Serialize and send request
    std::string req_str = req.serialize();
    std::bytearray req_data(req_str);
    
    size_t sent = m_tcp_client.write(req_data);
    if (sent == 0) {
        throw network_error("Failed to send request");
    }
    
    // Receive response
    return receive_response();
}

void client::set_timeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
}

std::chrono::milliseconds client::get_timeout() const
{
    return m_timeout;
}

std::string client::server_host() const
{
    return m_host;
}

uint16_t client::server_port() const
{
    return m_port;
}

request client::build_request(http_method method, const std::string& path,
                             const std::string& body,
                             const std::map<std::string, std::string>& headers)
{
    request req;
    req.method = method;
    req.path = path;
    req.http_version = "HTTP/1.1";
    req.body = body;
    
    // Set Host header (required for HTTP/1.1)
    req.headers["Host"] = m_host;
    if (m_port != 80 && m_port != 443) {
        req.headers["Host"] += ":" + std::to_string(m_port);
    }
    
    // Set Connection header
    req.headers["Connection"] = "close";
    
    // Copy additional headers
    for (const auto& [key, value] : headers) {
        req.headers[key] = value;
    }
    
    return req;
}

response client::receive_response()
{
    std::string buffer;
    auto start_time = std::chrono::steady_clock::now();
    
    // Read until we have complete headers
    while (!response::has_complete_headers(buffer)) {
        if (m_tcp_client.readyRead()) {
            auto data = m_tcp_client.readAll();
            if (!data.empty()) {
                buffer += data.toStdString();
            }
        }
        
        // Check timeout
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > m_timeout) {
            throw network_error("Response timeout");
        }
        
        // Small delay to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Parse partial response to get Content-Length
    response resp;
    try {
        resp = response::deserialize(buffer);
    } catch (...) {
        throw network_error("Failed to parse response headers");
    }
    
    // Calculate expected body length
    size_t content_length = 0;
    auto cl_it = resp.headers.find("Content-Length");
    if (cl_it != resp.headers.end()) {
        try {
            content_length = std::stoull(cl_it->second);
        } catch (...) {
            content_length = 0;
        }
    }
    
    // Read body if needed
    size_t header_end = buffer.find("\r\n\r\n");
    size_t body_start = header_end + 4;
    size_t current_body_length = buffer.size() - body_start;
    
    while (current_body_length < content_length) {
        if (m_tcp_client.readyRead()) {
            auto data = m_tcp_client.readAll();
            if (!data.empty()) {
                buffer += data.toStdString();
                current_body_length = buffer.size() - body_start;
            }
        }
        
        // Check timeout
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > m_timeout) {
            throw network_error("Response body timeout");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Parse complete response
    return response::deserialize(buffer);
}

} // namespace network::http
