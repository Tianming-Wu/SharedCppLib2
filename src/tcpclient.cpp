#include "tcpclient.hpp"
#include "dns.hpp"
#include "platform.hpp"
#include "string.hpp"

#include <cstring>
#include <vector>

#ifndef OS_WINDOWS
#  include <cerrno>
#  include <fcntl.h>
#endif

namespace network::tcp {

namespace {

int socket_error_code()
{
#ifdef OS_WINDOWS
    return ::WSAGetLastError();
#else
    return errno;
#endif
}

bool connect_in_progress(int error)
{
#ifdef OS_WINDOWS
    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEINVAL;
#else
    return error == EINPROGRESS;
#endif
}

std::string error_text(const std::string& what, int code)
{
    if (code == 0) return what;
    return what + " (error " + std::to_string(code) + ")";
}

void set_non_blocking(socket_t sock, bool on)
{
#ifdef OS_WINDOWS
    u_long value = on ? 1 : 0;
    ::ioctlsocket(sock, FIONBIO, &value);
#else
    const int flags = ::fcntl(sock, F_GETFL, 0);
    ::fcntl(sock, F_SETFL, on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
#endif
}

/// @return > 0 writable, 0 timed out, < 0 error.
int wait_writable(socket_t sock, std::chrono::milliseconds timeout)
{
    fd_set writeset;
    FD_ZERO(&writeset);
    FD_SET(sock, &writeset);

    timeval tv;
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

#ifdef OS_WINDOWS
    return ::select(0, nullptr, &writeset, nullptr, &tv);
#else
    return ::select(sock + 1, nullptr, &writeset, nullptr, &tv);
#endif
}

bool make_sockaddr(const network_address& addr, uint16_t port, sockaddr_storage& ss, socklen_t& len)
{
    std::memset(&ss, 0, sizeof(ss));

    if (addr.type == network_address::kind::ipv6) {
        auto* sin6 = reinterpret_cast<sockaddr_in6*>(&ss);
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = htons(port);
        sin6->sin6_scope_id = addr.ipv6_addr.scope_id;
        const auto bytes = addr.ipv6_addr.to_bytes();
        std::memcpy(&sin6->sin6_addr, bytes.data(), bytes.size());
        len = sizeof(sockaddr_in6);
        return true;
    }

    if (addr.type == network_address::kind::ipv4) {
        auto* sin = reinterpret_cast<sockaddr_in*>(&ss);
        sin->sin_family = AF_INET;
        sin->sin_port = htons(port);
        sin->sin_addr.s_addr = htonl(addr.ipv4_addr.to_uint32());
        len = sizeof(sockaddr_in);
        return true;
    }

    return false;
}

/// @brief Connect, waiting at most @p timeout when it is not zero.
bool connect_socket(socket_t sock, const sockaddr* addr, socklen_t len,
                    std::chrono::milliseconds timeout, std::string& error)
{
    if (timeout.count() <= 0) {
        if (::connect(sock, addr, len) == socket_error) {
            error = error_text("connect() failed", socket_error_code());
            return false;
        }
        return true;
    }

    set_non_blocking(sock, true);

    if (::connect(sock, addr, len) == socket_error) {
        const int code = socket_error_code();
        if (!connect_in_progress(code)) {
            set_non_blocking(sock, false);
            error = error_text("connect() failed", code);
            return false;
        }

        const int ready = wait_writable(sock, timeout);
        if (ready == 0) {
            set_non_blocking(sock, false);
            error = "connect() timed out after " + std::to_string(timeout.count()) + " ms";
            return false;
        }
        if (ready < 0) {
            set_non_blocking(sock, false);
            error = error_text("waiting for connect() failed", socket_error_code());
            return false;
        }

        int so_error = 0;
        socklen_t size = sizeof(so_error);
        ::getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &size);
        if (so_error != 0) {
            set_non_blocking(sock, false);
            error = error_text("connect() failed", so_error);
            return false;
        }
    }

    set_non_blocking(sock, false);
    return true;
}

} // namespace

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
    return connect(network_address::parse(address), port, m_connect_timeout);
}

bool client::connect(const std::string& address, uint16_t port, std::chrono::milliseconds timeout)
{
    return connect(network_address::parse(address), port, timeout);
}

bool client::connect(const network_address& address, uint16_t port)
{
    return connect(address, port, m_connect_timeout);
}

bool client::connect(const network_address& address, uint16_t port, std::chrono::milliseconds timeout)
{
    m_last_error.clear();

    // A name is resolved here, and a failure comes back as false, not as an
    // exception from inside the connection path.
    std::vector<network_address> candidates;
    if (address.needs_resolution()) {
        dns::dns_query_result result;
        try {
            result = dns::dns_query(address.address);
        } catch (...) {
            m_last_error = "the host name could not be resolved";
            return false;
        }
        if (!result.ok()) {
            m_last_error = result.status_text.empty()
                ? "the host name could not be resolved" : result.status_text;
            return false;
        }
        // IPv4 first: it is the form that works on the most machines, IPv6 is
        // the fallback rather than the other way round.
        for (const auto& addr : result.by_family(network_address::kind::ipv4)) candidates.push_back(addr);
        for (const auto& addr : result.by_family(network_address::kind::ipv6)) candidates.push_back(addr);
    } else if (address.is_literal()) {
        candidates.push_back(address);
    } else {
        m_last_error = "there is no address to connect to";
        return false;
    }

    // disconnect() takes the lock itself, so it has to run before we hold it.
    if (m_socket != invalid_socket) {
        disconnect();
    }

    for (const network_address& candidate : candidates) {
        const int family = (candidate.type == network_address::kind::ipv6) ? AF_INET6 : AF_INET;

        socket_t sock = ::socket(family, SOCK_STREAM, IPPROTO_TCP);
        if (sock == invalid_socket) {
            m_last_error = error_text("could not create a socket", socket_error_code());
            continue;
        }

        sockaddr_storage ss{};
        socklen_t len = 0;
        if (!make_sockaddr(candidate, port, ss, len)) {
            close_socket(sock);
            continue;
        }

        std::string error;
        if (!connect_socket(sock, reinterpret_cast<const sockaddr*>(&ss), len, timeout, error)) {
            close_socket(sock);
            if (m_last_error.empty()) m_last_error = error;
            continue;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_socket = sock;
        m_server_address = candidate;
        m_server_port = port;
        return true;
    }

    if (m_last_error.empty()) m_last_error = "no address could be reached";
    return false;
}

void client::set_connect_timeout(std::chrono::milliseconds timeout)
{
    m_connect_timeout = timeout;
}

std::chrono::milliseconds client::connect_timeout() const
{
    return m_connect_timeout;
}

std::string client::lastError() const
{
    return m_last_error;
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

scl2::bytearray client::read(size_t bytes)
{
    if (m_socket == invalid_socket || bytes == 0) {
        return scl2::bytearray();
    }
    
    std::vector<std::byte> buffer(bytes);
    int received = ::recv(
        m_socket,
        reinterpret_cast<char*>(buffer.data()),
        static_cast<int>(bytes),
        0
    );
    
    if (received <= 0) {
        return scl2::bytearray();
    }
    
    return scl2::bytearray(buffer.data(), static_cast<size_t>(received));
}

scl2::bytearray client::readAll()
{
    size_t bytes = available();
    if (bytes == 0) {
        return scl2::bytearray();
    }
    return read(bytes);
}

size_t client::write(const scl2::bytearray& data)
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
