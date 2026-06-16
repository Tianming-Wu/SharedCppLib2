#include "http.hpp"

#include <sstream>
#include <algorithm>
#include <cctype>

namespace network::http {

static const std::map<http_method, std::string> method_to_str = {
    {http_method::GET, "GET"},
    {http_method::POST, "POST"},
    {http_method::PUT, "PUT"},
    {http_method::DELETE, "DELETE"},
    {http_method::HEAD, "HEAD"},
    {http_method::OPTIONS, "OPTIONS"},
    {http_method::PATCH, "PATCH"},
    {http_method::TRACE, "TRACE"},
    {http_method::CONNECT, "CONNECT"}
};

static const std::map<std::string, http_method> str_to_method = {
    {"GET", http_method::GET},
    {"POST", http_method::POST},
    {"PUT", http_method::PUT},
    {"DELETE", http_method::DELETE},
    {"HEAD", http_method::HEAD},
    {"OPTIONS", http_method::OPTIONS},
    {"PATCH", http_method::PATCH},
    {"TRACE", http_method::TRACE},
    {"CONNECT", http_method::CONNECT}
};

static const std::map<http_status, std::string> status_messages = {
    {http_status::OK, "OK"},
    {http_status::CREATED, "Created"},
    {http_status::NO_CONTENT, "No Content"},
    {http_status::BAD_REQUEST, "Bad Request"},
    {http_status::NOT_FOUND, "Not Found"},
    {http_status::METHOD_NOT_ALLOWED, "Method Not Allowed"},
    {http_status::INTERNAL_SERVER_ERROR, "Internal Server Error"},
    {http_status::NOT_IMPLEMENTED, "Not Implemented"}
};

std::string method_to_string(http_method method)
{
    auto it = method_to_str.find(method);
    return it != method_to_str.end() ? it->second : "GET";
}

http_method string_to_method(const std::string& str)
{
    auto it = str_to_method.find(str);
    if (it != str_to_method.end()) {
        return it->second;
    }
    throw network_error("Invalid HTTP method: " + str);
}

std::string status_to_string(http_status status)
{
    auto it = status_messages.find(status);
    return it != status_messages.end() ? it->second : "Unknown";
}

// ========== request implementation ==========

std::string request::serialize() const
{
    std::string result;
    result.reserve(512); // reasonable default size

    // Request line: METHOD /path HTTP/version
    result += method_to_string(method) + " " + path + " " + http_version + "\r\n";
    
    // Headers
    for (const auto& [key, value] : headers) {
        result += key + ": " + value + "\r\n";
    }
    
    // Add Content-Length if body exists and not already specified
    if (!body.empty() && headers.find("Content-Length") == headers.end()) {
        result += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    
    result += "\r\n"; // End of headers
    
    if (!body.empty()) {
        result += body;
    }

    return result;
}

request request::deserialize(const std::string& str)
{
    request req;
    
    if (str.empty()) {
        throw network_error("Empty HTTP request");
    }

    std::istringstream stream(str);
    std::string line;
    
    // Parse request line: METHOD /path HTTP/version
    if (!std::getline(stream, line)) {
        throw network_error("Invalid HTTP request: no request line");
    }
    
    // Remove trailing \r if present
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    std::istringstream line_stream(line);
    std::string method_str, version;
    
    line_stream >> method_str >> req.path >> version;
    
    if (method_str.empty() || req.path.empty()) {
        throw network_error("Invalid HTTP request line");
    }
    
    req.method = string_to_method(method_str);
    req.http_version = version.empty() ? "HTTP/1.1" : version;
    
    // Parse headers
    while (std::getline(stream, line)) {
        // Remove trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Empty line indicates end of headers
        if (line.empty()) {
            break;
        }
        
        // Parse header: Key: Value
        size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue; // Skip malformed headers
        }
        
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        
        // Trim whitespace from value
        size_t start = value.find_first_not_of(" \t");
        if (start != std::string::npos) {
            value = value.substr(start);
        }
        
        req.headers[key] = value;
    }
    
    // Read body (rest of stream)
    std::string body_content;
    std::getline(stream, body_content, '\0');
    req.body = body_content;
    
    return req;
}

bool request::has_complete_headers(const std::string& str)
{
    return str.find("\r\n\r\n") != std::string::npos;
}

size_t request::get_content_length() const
{
    auto it = headers.find("Content-Length");
    if (it != headers.end()) {
        try {
            return std::stoull(it->second);
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

// ========== response implementation ==========

std::string response::serialize() const
{
    std::string result;
    result.reserve(512);

    // Status line: HTTP/version STATUS_CODE STATUS_MESSAGE
    result += http_version + " " + std::to_string(static_cast<int>(status)) + " " + status_to_string(status) + "\r\n";
    
    // Headers
    for (const auto& [key, value] : headers) {
        result += key + ": " + value + "\r\n";
    }
    
    // Add Content-Length if body exists and not already specified
    if (!body.empty() && headers.find("Content-Length") == headers.end()) {
        result += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    
    result += "\r\n"; // End of headers
    
    if (!body.empty()) {
        result += body;
    }

    return result;
}

response response::deserialize(const std::string& str)
{
    response resp;
    
    if (str.empty()) {
        throw network_error("Empty HTTP response");
    }

    std::istringstream stream(str);
    std::string line;
    
    // Parse status line
    if (!std::getline(stream, line)) {
        throw network_error("Invalid HTTP response: no status line");
    }
    
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    std::istringstream line_stream(line);
    std::string version;
    int status_code;
    
    line_stream >> version >> status_code;
    
    resp.http_version = version;
    resp.status = static_cast<http_status>(status_code);
    
    // Parse headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.empty()) {
            break;
        }
        
        size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        
        size_t start = value.find_first_not_of(" \t");
        if (start != std::string::npos) {
            value = value.substr(start);
        }
        
        resp.headers[key] = value;
    }
    
    // Read body
    std::string body_content;
    std::getline(stream, body_content, '\0');
    resp.body = body_content;
    
    return resp;
}

bool response::has_complete_headers(const std::string& str)
{
    return str.find("\r\n\r\n") != std::string::npos;
}

response response::make_text(http_status status, const std::string& text)
{
    response resp;
    resp.status = status;
    resp.headers["Content-Type"] = "text/plain; charset=utf-8";
    resp.body = text;
    return resp;
}

response response::make_json(http_status status, const std::string& json)
{
    response resp;
    resp.status = status;
    resp.headers["Content-Type"] = "application/json; charset=utf-8";
    resp.body = json;
    return resp;
}

} // namespace network::http