#include "httpserver.hpp"

namespace network::http {

server::server()
    : m_tcp_server()
{
}

server::server(uint16_t port)
    : m_tcp_server(port)
{
}

server::server(network_address address, uint16_t port)
    : m_tcp_server(address, port)
{
}

server::~server()
{
    stop();
}

void server::start()
{
    m_tcp_server.start();
}

void server::start(uint16_t port)
{
    m_tcp_server.start(port);
}

void server::stop()
{
    m_tcp_server.stop();
}

void server::route(http_method method, const std::string& path, route_handler handler)
{
    m_routes[{method, path}] = handler;
}

int server::tick()
{
    // First, accept new connections at TCP level
    int new_clients = m_tcp_server.tick();

    // Then, process data from existing clients
    for (auto client_id : m_tcp_server.clients()) {
        handleClient(client_id);
    }

    return new_clients;
}

uint16_t server::port() const
{
    return m_tcp_server.port();
}

network_address server::address() const
{
    return m_tcp_server.address();
}

void server::handleClient(tcp::client_id id)
{
    auto handler = m_tcp_server.selectClient(id);
    
    if (!handler.valid()) {
        return;
    }

    // Read available data
    if (handler.readable()) {
        auto data = handler.readAll();
        if (data.empty()) {
            // readAll() probes EOF and invalidates the TCP client on shutdown.
            m_tcp_server.closeClient(id);
            m_client_buffers.erase(id);
            return;
        }

        // Accumulate data in buffer
        m_client_buffers[id] += data.toStdString();

        std::string& buffer = m_client_buffers[id];

        const size_t header_end = buffer.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            if (buffer.size() > max_header_size) {
                response resp = response::make_text(http_status::PAYLOAD_TOO_LARGE,
                                                    "Request headers too large");
                handler.write(scl2::bytearray(resp.serialize()));
                m_tcp_server.closeClient(id);
                m_client_buffers.erase(id);
            }
            return; // Wait for more data
        }

        if (header_end + 4 > max_header_size ||
            buffer.size() > max_header_size + max_body_size) {
            response resp = response::make_text(http_status::PAYLOAD_TOO_LARGE,
                                                "Request too large");
            handler.write(scl2::bytearray(resp.serialize()));
            m_tcp_server.closeClient(id);
            m_client_buffers.erase(id);
            return;
        }
        
        // Try to parse the request to check Content-Length
        request req;
        try {
            req = request::deserialize(buffer);
        } catch (const std::exception&) {
            // Parsing failed, send 400 Bad Request
            response resp = response::make_text(http_status::BAD_REQUEST, "Bad Request");
            handler.write(scl2::bytearray(resp.serialize()));
            m_tcp_server.closeClient(id);
            m_client_buffers.erase(id);
            return;
        }

        for (const auto& [name, value] : req.headers) {
            if (name.size() == 17) {
                std::string lowered = name;
                for (char& c : lowered) {
                    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
                }
                if (lowered == "transfer-encoding") {
                    response resp = response::make_text(http_status::BAD_REQUEST,
                                                        "Transfer-Encoding is not supported");
                    handler.write(scl2::bytearray(resp.serialize()));
                    m_tcp_server.closeClient(id);
                    m_client_buffers.erase(id);
                    return;
                }
            }
        }
        
        // Check if we have complete body
        size_t content_length = 0;
        try {
            content_length = req.get_content_length();
        } catch (const std::exception&) {
            response resp = response::make_text(http_status::BAD_REQUEST, "Bad Request");
            handler.write(scl2::bytearray(resp.serialize()));
            m_tcp_server.closeClient(id);
            m_client_buffers.erase(id);
            return;
        }
        if (content_length > max_body_size) {
            response resp = response::make_text(http_status::PAYLOAD_TOO_LARGE,
                                                "Request body too large");
            handler.write(scl2::bytearray(resp.serialize()));
            m_tcp_server.closeClient(id);
            m_client_buffers.erase(id);
            return;
        }

        size_t body_start = header_end + 4;
        size_t current_body_length = buffer.size() - body_start;

        if (current_body_length > max_body_size) {
            response resp = response::make_text(http_status::PAYLOAD_TOO_LARGE,
                                                "Request body too large");
            handler.write(scl2::bytearray(resp.serialize()));
            m_tcp_server.closeClient(id);
            m_client_buffers.erase(id);
            return;
        }
        
        if (current_body_length < content_length) {
            return; // Wait for more body data
        }
        
        // We have a complete request, process it
        response resp;
        
        // Look up route handler
        auto route_key = std::make_pair(req.method, req.path);
        auto route_it = m_routes.find(route_key);
        
        if (route_it != m_routes.end()) {
            // Route found, call handler
            try {
                std::string response_body = route_it->second(req);
                resp = response::make_text(http_status::OK, response_body);
            } catch (const std::exception& e) {
                // Handler threw exception
                resp = response::make_text(http_status::INTERNAL_SERVER_ERROR, 
                                          "Internal Server Error: " + std::string(e.what()));
            }
        } else {
            // No route found
            resp = response::make_text(http_status::NOT_FOUND, 
                                      "Not Found: " + req.path);
        }
        
        // Send response
        handler.write(scl2::bytearray(resp.serialize()));
        
        // Clear the buffer
        m_tcp_server.closeClient(id);
        m_client_buffers.erase(id);
    }
}

} // namespace network::http
