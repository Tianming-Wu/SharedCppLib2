#include "udp.hpp"

#include "dns.hpp"
#include "network_platform.hpp"

#include <cstring>

namespace network::udp {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Maximum theoretical UDP payload (IPv4): 65535 - 20 (IP) - 8 (UDP) = 65507
/// For IPv6 jumbograms this can be larger, but 65535 is the practical limit.
static constexpr size_t MAX_UDP_PAYLOAD = 65535;

/// Fill sockaddr_storage from network_address + port. Returns address family.
static int fill_sockaddr(sockaddr_storage& ss, const network_address& addr, uint16_t port)
{
    std::memset(&ss, 0, sizeof(ss));

    if (addr.type == network_address::kind::ipv6) {
        auto* sin6 = reinterpret_cast<sockaddr_in6*>(&ss);
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = htons(port);
        sin6->sin6_scope_id = addr.ipv6_addr.scope_id;
        const auto bytes = addr.ipv6_addr.to_bytes();
        std::memcpy(&sin6->sin6_addr, bytes.data(), bytes.size());
        return AF_INET6;
    }

    // IPv4, and "any" for an unspecified address (what a plain bind() means).
    auto* sin = reinterpret_cast<sockaddr_in*>(&ss);
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    const uint32_t raw = (addr.type == network_address::kind::ipv4) ? addr.ipv4_addr.to_uint32() : 0;
    sin->sin_addr.s_addr = htonl(raw);
    return AF_INET;
}

/// Call network::init() lazily to ensure WSAStartup has been called on Windows.
static void ensure_init()
{
    network::init();
}

// ---------------------------------------------------------------------------
// socket
// ---------------------------------------------------------------------------

socket::socket()
{
    ensure_init();
}

socket::~socket()
{
    close();
}

bool socket::bind(uint16_t port)
{
    return bind(network_address{}, port);
}

bool socket::bind(const network_address& addr, uint16_t port)
{
    if (m_socket != invalid_socket) {
        close();
    }

    int family = AF_INET;
    sockaddr_storage ss{};
    family = fill_sockaddr(ss, addr, port);

    m_socket = ::socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == invalid_socket) {
        return false;
    }

    // Allow reusing the address (useful for restarting)
    int reuse = 1;
    ::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    const socklen_t addr_len = (family == AF_INET6) ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
    if (::bind(m_socket, reinterpret_cast<const sockaddr*>(&ss), addr_len) == socket_error) {
        close();
        return false;
    }

    m_local_address = addr;
    m_local_port    = port;

    return true;
}

bool socket::connect(const std::string& address, uint16_t port)
{
    network_address addr = network_address::parse(address);

    if (addr.needs_resolution()) {
        try {
            const dns::dns_query_result result = dns::dns_query(addr.address);
            if (!result.ok()) return false;
            addr = result.preferred();
        } catch (...) {
            return false;
        }
    }

    if (!addr.is_literal()) return false;
    return connect(addr, port);
}

bool socket::connect(const network_address& addr, uint16_t port)
{
    const int family = (addr.type == network_address::kind::ipv6) ? AF_INET6 : AF_INET;

    if (m_socket == invalid_socket) {
        // Auto-create socket if we haven't bound yet
        m_socket = ::socket(family, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket == invalid_socket) {
            return false;
        }
    }

    sockaddr_storage ss{};
    fill_sockaddr(ss, addr, port);

    socklen_t len = (family == AF_INET6) ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
    if (::connect(m_socket, reinterpret_cast<const sockaddr*>(&ss), len) == socket_error) {
        return false;
    }

    m_connected           = true;
    m_default_address     = addr;
    m_default_port        = port;
    m_default_sockaddr    = ss;
    m_default_sockaddr_len = static_cast<int>(len);

    return true;
}

void socket::close()
{
    if (m_socket != invalid_socket) {
#ifdef OS_WINDOWS
        ::closesocket(m_socket);
#else
        ::close(m_socket);
#endif
        m_socket = invalid_socket;
    }
    m_connected   = false;
    m_local_port  = 0;
}

// ---- Send ----

size_t socket::sendTo(const scl2::bytearray& data,
                      const network_address& dest, uint16_t port)
{
    if (m_socket == invalid_socket || data.size() == 0) {
        return 0;
    }

    sockaddr_storage ss{};
    const int family = fill_sockaddr(ss, dest, port);
    const socklen_t len = (family == AF_INET6) ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);

    int sent = ::sendto(
        m_socket,
        reinterpret_cast<const char*>(data.data()),
        static_cast<int>(data.size()),
        0, // flags
        reinterpret_cast<const sockaddr*>(&ss),
        len
    );

    return (sent == socket_error) ? 0 : static_cast<size_t>(sent);
}

size_t socket::send(const scl2::bytearray& data)
{
    if (m_socket == invalid_socket || data.size() == 0) {
        return 0;
    }

#ifdef OS_WINDOWS
    int sent = ::send(
        m_socket,
        reinterpret_cast<const char*>(data.data()),
        static_cast<int>(data.size()),
        0
    );
#else
    ssize_t sent = ::send(
        m_socket,
        data.data(),
        data.size(),
        0
    );
#endif

    return (sent == socket_error) ? 0 : static_cast<size_t>(sent);
}

// ---- Receive ----

datagram socket::receiveFrom()
{
    return receiveFrom(std::chrono::milliseconds(0));
}

datagram socket::receiveFrom(std::chrono::milliseconds timeout)
{
    datagram dg;
    if (m_socket == invalid_socket) {
        return dg;
    }

    // Wait with select()
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(m_socket, &readset);

    timeval tv{};
    tv.tv_sec  = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

#ifdef OS_WINDOWS
    int ret = ::select(0, &readset, nullptr, nullptr, (timeout.count() == 0) ? nullptr : &tv);
#else
    int ret = ::select(m_socket + 1, &readset, nullptr, nullptr,
                       (timeout.count() == 0) ? nullptr : &tv);
#endif

    if (ret <= 0 || !FD_ISSET(m_socket, &readset)) {
        return dg;
    }

    // Read one datagram
    std::vector<std::byte> buf(MAX_UDP_PAYLOAD);
    sockaddr_storage from{};
    socklen_t from_len = sizeof(from);

#ifdef OS_WINDOWS
    int received = ::recvfrom(
        m_socket,
        reinterpret_cast<char*>(buf.data()),
        static_cast<int>(buf.size()),
        0,
        reinterpret_cast<sockaddr*>(&from),
        &from_len
    );
#else
    ssize_t received = ::recvfrom(
        m_socket,
        buf.data(),
        buf.size(),
        0,
        reinterpret_cast<sockaddr*>(&from),
        &from_len
    );
#endif

    if (received <= 0) {
        return dg;
    }

    dg.data = scl2::bytearray(buf.data(), static_cast<size_t>(received));

    // Extract sender address
    auto* sin = reinterpret_cast<sockaddr_in*>(&from);
    dg.sender_addr = network_address(ipv4::from_uint32(ntohl(sin->sin_addr.s_addr)));
    dg.sender_port = ntohs(sin->sin_port);

    return dg;
}

// ---- basic_iostream ----

bool socket::valid()
{
    return m_socket != invalid_socket;
}

bool socket::readyRead()
{
    if (m_socket == invalid_socket) {
        return false;
    }

    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(m_socket, &readset);

    timeval tv{0, 0};

#ifdef OS_WINDOWS
    int ret = ::select(0, &readset, nullptr, nullptr, &tv);
#else
    int ret = ::select(m_socket + 1, &readset, nullptr, nullptr, &tv);
#endif

    return ret > 0 && FD_ISSET(m_socket, &readset);
}

size_t socket::available()
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

scl2::bytearray socket::read(size_t bytes)
{
    if (m_socket == invalid_socket || bytes == 0) {
        return scl2::bytearray();
    }

    std::vector<std::byte> buf(bytes);

#ifdef OS_WINDOWS
    int received = ::recvfrom(
        m_socket,
        reinterpret_cast<char*>(buf.data()),
        static_cast<int>(bytes),
        0,
        nullptr,
        nullptr
    );
#else
    ssize_t received = ::recvfrom(
        m_socket,
        buf.data(),
        bytes,
        0,
        nullptr,
        nullptr
    );
#endif

    if (received <= 0) {
        return scl2::bytearray();
    }

    return scl2::bytearray(buf.data(), static_cast<size_t>(received));
}

scl2::bytearray socket::readAll()
{
    return read(MAX_UDP_PAYLOAD);
}

size_t socket::write(const scl2::bytearray& data)
{
    return send(data);
}

// ---- Info ----

network_address socket::localAddress() const
{
    return m_local_address;
}

uint16_t socket::localPort() const
{
    return m_local_port;
}

bool socket::is_connected() const
{
    return m_connected;
}

} // namespace network::udp
