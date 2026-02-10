#include "httpserver.hpp"

#include <cstring>

extern int _n_sock;

namespace network::http {



server_client_handler::server_client_handler(server &srv, client_info &info)
    : m_server(srv), m_client_info(info)
{
}

bool server_client_handler::readyRead()
{
    if (m_client_info.socket == invalid_socket) {
        return false;
    }

    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(m_client_info.socket, &readset);

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

#ifdef OS_WINDOWS
    int ret = ::select(0, &readset, nullptr, nullptr, &tv);
#else
    int ret = ::select(m_client_info.socket + 1, &readset, nullptr, nullptr, &tv);
#endif
    return ret > 0 && FD_ISSET(m_client_info.socket, &readset);
}

size_t server_client_handler::available()
{
    if (m_client_info.socket == invalid_socket) {
        return 0;
    }

#ifdef OS_WINDOWS
    u_long bytes = 0;
    if (::ioctlsocket(m_client_info.socket, FIONREAD, &bytes) != 0) {
        return 0;
    }
    return static_cast<size_t>(bytes);
#else
    int bytes = 0;
    if (::ioctl(m_client_info.socket, FIONREAD, &bytes) != 0) {
        return 0;
    }
    return static_cast<size_t>(bytes);
#endif
}

std::bytearray server_client_handler::read(size_t bytes)
{
    if (m_client_info.socket == invalid_socket || bytes == 0) {
        return std::bytearray();
    }

    std::vector<std::byte> buffer(bytes);
    int received = ::recv(
        m_client_info.socket,
        reinterpret_cast<char*>(buffer.data()),
        static_cast<int>(bytes),
        0
    );
    if (received <= 0) {
        return std::bytearray();
    }
    return std::bytearray(buffer.data(), static_cast<size_t>(received));
}

std::bytearray server_client_handler::readAll()
{
    size_t bytes = available();
    if (bytes == 0) {
        return std::bytearray();
    }
    return read(bytes);
}

size_t server_client_handler::write(const std::bytearray &data)
{
    if (m_client_info.socket == invalid_socket || data.empty()) {
        return 0;
    }

    int sent = ::send(
        m_client_info.socket,
        reinterpret_cast<const char*>(data.data()),
        static_cast<int>(data.size()),
        0
    );
    if (sent <= 0) {
        return 0;
    }
    return static_cast<size_t>(sent);
}

bool server_client_handler::valid()
{
    return m_client_info.socket != invalid_socket;
}

auto server_client_handler::lock() -> std::unique_lock<std::mutex>
{
    return std::unique_lock<std::mutex>(m_client_info.mutex);
}



server::server()
    : m_address(), m_port(0)
{
    init();
    m_address.dummy = true;
}

server::server(uint16_t port)
    : m_address(), m_port(port)
{
    init();
    m_address.dummy = true;
}

server::server(network_address address, uint16_t port)
    : m_address(address), m_port(port)
{
    init();
    m_address.dummy = false;
}

server::~server()
{
    stop();
}

void server::start()
{
    start(m_port);
}

void server::start(uint16_t port)
{
    if(port != m_port) {
        m_port = port;
    }

    if (m_port == 0) {
        throw network_error("Invalid port");
    }

    if (m_listen_socket != invalid_socket) {
        stop();
    }

    socket_t listen_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == invalid_socket) {
        throw network_error("Failed to create socket");
    }

    int opt = 1;
    ::setsockopt(
        listen_socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&opt),
        sizeof(opt)
    );

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);

    if (m_address.dummy || m_address.address.empty()) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        m_address.address = "0.0.0.0";
        m_address.__ipv4 = ipv4::from_string(m_address.address);
        m_address.dummy = false;
    } else {
        in_addr in_addr_val;
        if (::inet_pton(AF_INET, m_address.address.c_str(), &in_addr_val) != 1) {
#ifdef OS_WINDOWS
            ::closesocket(listen_socket);
#else
            ::close(listen_socket);
#endif
            throw network_error("Invalid IPv4 address");
        }
        addr.sin_addr = in_addr_val;
        m_address.__ipv4 = ipv4::from_string(m_address.address);
    }

        if (
    #ifdef OS_WINDOWS
        ::bind(listen_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR
    #else
        ::bind(listen_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1
    #endif
        ) {
#ifdef OS_WINDOWS
        ::closesocket(listen_socket);
#else
        ::close(listen_socket);
#endif
        throw network_error("Failed to bind socket");
    }

        if (
    #ifdef OS_WINDOWS
        ::listen(listen_socket, SOMAXCONN) == SOCKET_ERROR
    #else
        ::listen(listen_socket, SOMAXCONN) == -1
    #endif
        ) {
#ifdef OS_WINDOWS
        ::closesocket(listen_socket);
#else
        ::close(listen_socket);
#endif
        throw network_error("Failed to listen on socket");
    }

#ifdef OS_WINDOWS
    u_long non_blocking = 1;
    ::ioctlsocket(listen_socket, FIONBIO, &non_blocking);
#endif

    m_listen_socket = listen_socket;
    m_running = true;
}

void server::stop()
{
    auto guard = lock();

    for (auto& [id, info] : m_clients) {
        if (info.socket != invalid_socket) {
#ifdef OS_WINDOWS
            ::closesocket(info.socket);
#else
            ::close(info.socket);
#endif
            info.socket = invalid_socket;
        }
    }
    m_clients.clear();

    if (m_listen_socket != invalid_socket) {
#ifdef OS_WINDOWS
        ::closesocket(m_listen_socket);
#else
        ::close(m_listen_socket);
#endif
        m_listen_socket = invalid_socket;
    }

    m_running = false;
}

uint16_t server::port() const { return m_port; }
network_address server::address() const { return m_address; }

std::string server::hostname(size_t max_length)
{
    std::string name;
    name.resize(max_length);
    if (gethostname(name.data(), static_cast<int>(max_length)) == -1) {
        throw network_error("Failed to get hostname");
    }
    name.resize(std::char_traits<char>::length(name.c_str()));
    return name;
}

std::vector<client_id> server::clients()
{
    // collect client IDs
    std::vector<client_id> ids;
    for (const auto& [id, info] : m_clients) {
        ids.push_back(id);
    }
    return ids;
}

server_client_handler server::selectClient(client_id &id)
{
    return server_client_handler(*this, m_clients[id]);
}

auto server::lock() -> std::unique_lock<std::mutex>
{
    return std::unique_lock<std::mutex>(m_mutex);
}

int server::tick()
{
    if (!m_running || m_listen_socket == invalid_socket) {
        return -1;
    }

    sockaddr_in client_addr;
    std::memset(&client_addr, 0, sizeof(client_addr));
#ifdef OS_WINDOWS
    int addr_len = sizeof(client_addr);
    socket_t client_socket = ::accept(m_listen_socket, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
#else
    socklen_t addr_len = sizeof(client_addr);
    socket_t client_socket = ::accept(m_listen_socket, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
#endif

    if (client_socket == invalid_socket) {
        return 0; // no pending connections
    }

    client_id new_id = m_next_client_id++;
    auto [it, inserted] = m_clients.try_emplace(new_id);
    it->second.id = new_id;
    it->second.socket = client_socket;
    it->second.addr = client_addr;
    return 1;
}

} // namespace network::http