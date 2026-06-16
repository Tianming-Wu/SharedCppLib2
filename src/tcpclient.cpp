#include "tcpclient.hpp"
#include "dns.hpp"
#include "platform.hpp"

#include <cstring>

namespace network::tcp {

client::client()
{
    init();
}

client::~client()
{
    disconnect();
}

bool client::connect(const std::string& address, uint16_t port)
{
    // Resolve hostname/IP to network_address
    network_address addr;
    
    // Try to parse as IPv4 first
    try {
        addr.__ipv4 = ipv4::from_string(address);
        addr.address = address;
    } catch (...) {
        // Not a direct IP, try DNS lookup
        auto dns_result = dns::dns_query(platform::stringToWstring(address));
        if (dns_result.addresses.empty()) {
            return false;
        }
        addr = dns_result.addresses[0];
    }
    
    return connect(addr, port);
}

bool client::connect(const network_address& address, uint16_t port)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Close existing connection if any
    if (m_socket != invalid_socket) {
        disconnect();
    }
    
    // Create socket
    socket_t sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == invalid_socket) {
        return false;
    }
    
    // Setup server address structure
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // Convert IP address
    if (::inet_pton(AF_INET, address.address.c_str(), &server_addr.sin_addr) != 1) {
#ifdef OS_WINDOWS
        ::closesocket(sock);
#else
        ::close(sock);
#endif
        return false;
    }
    
    // Connect to server
    int result = ::connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
    if (result == socket_error) {
#ifdef OS_WINDOWS
        ::closesocket(sock);
#else
        ::close(sock);
#endif
        return false;
    }
    
    m_socket = sock;
    m_server_address = address;
    m_server_port = port;
    
    return true;
}

void client::disconnect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_socket != invalid_socket) {
#ifdef OS_WINDOWS
        ::closesocket(m_socket);
#else
        ::close(m_socket);
#endif
        m_socket = invalid_socket;
    }
    
    m_server_port = 0;
}

bool client::is_connected() const
{
    return m_socket != invalid_socket;
}

bool client::valid()
{
    return is_connected();
}

bool client::readyRead()
{
    if (m_socket == invalid_socket) {
        return false;
    }
    
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(m_socket, &readset);
    
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    
#ifdef OS_WINDOWS
    int ret = ::select(0, &readset, nullptr, nullptr, &tv);
#else
    int ret = ::select(m_socket + 1, &readset, nullptr, nullptr, &tv);
#endif
    
    return ret > 0 && FD_ISSET(m_socket, &readset);
}

size_t client::available()
{
    if (m_socket == invalid_socket) {
        return 0;
    }
    
#ifdef OS_WINDOWS
    u_long bytes = 0;
    if (::ioctlsocket(m_socket, FIONREAD, &bytes) != 0) {
        return 0;
    }
    return static_cast<size_t>(bytes);
#else
    int bytes = 0;
    if (::ioctl(m_socket, FIONREAD, &bytes) != 0) {
        return 0;
    }
    return static_cast<size_t>(bytes);
#endif
}

std::bytearray client::read(size_t bytes)
{
    if (m_socket == invalid_socket || bytes == 0) {
        return std::bytearray();
    }
    
    std::vector<std::byte> buffer(bytes);
    int received = ::recv(
        m_socket,
        reinterpret_cast<char*>(buffer.data()),
        static_cast<int>(bytes),
        0
    );
    
    if (received <= 0) {
        return std::bytearray();
    }
    
    return std::bytearray(buffer.data(), static_cast<size_t>(received));
}

std::bytearray client::readAll()
{
    size_t bytes = available();
    if (bytes == 0) {
        return std::bytearray();
    }
    return read(bytes);
}

size_t client::write(const std::bytearray& data)
{
    if (m_socket == invalid_socket || data.empty()) {
        return 0;
    }
    
    int sent = ::send(
        m_socket,
        reinterpret_cast<const char*>(data.data()),
        static_cast<int>(data.size()),
        0
    );
    
    if (sent <= 0) {
        return 0;
    }
    
    return static_cast<size_t>(sent);
}

network_address client::server_address() const
{
    return m_server_address;
}

uint16_t client::server_port() const
{
    return m_server_port;
}

} // namespace network::tcp
