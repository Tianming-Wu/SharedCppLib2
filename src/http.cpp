#include "http.hpp"

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

std::string request::serialize() const
{
    std::string result;

    result += method_to_str.at(method) + " " + path.value_or("/") + " HTTP/1.1\r\n";
    result += "Host: " + headers.at("Host") + "\r\n";
    for (const auto& [key, value] : headers) {
        if (key != "Host") {
            result += key + ": " + value + "\r\n";
        }
    }
    result += "\r\n"; // End of headers

    if (body.has_value()) {
        result += body.value();
    }

    return result;
}

request request::deserialize(const std::string &str)
{
    request req;

    ///TODO: implement this function to parse the raw HTTP request string and populate the request object.
    // We want full compatibility with the HTTP standard.
}

} // namespace network::http