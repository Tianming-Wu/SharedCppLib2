#include "udp.hpp"

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

    // Prefer IPv4 if it has been set (even to 0.0.0.0; that means "any")
    // We detect this by checking if the __ipv4 octets differ from a
    // default-constructed ipv4.  A default ipv4 is all-zero (0.0.0.0),
    // which is valid for "bind to any".  We use a simple heuristic:
    // if address string is non-empty, trust that; otherwise default to IPv4 any.
    if (!addr.address.empty() && addr.address.find(':') != std::string::npos) {
        // looks like an IPv6 or hostname with port; just use IPv4 for now
    }

    // Default to IPv4
    auto* sin = reinterpret_cast<sockaddr_in*>(&ss);
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);

    uint32_t raw = addr.__ipv4.to_uint32();
    std::memcpy(&sin->sin_addr, &raw, sizeof(raw));
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

    socklen_t addr_len = sizeof(sockaddr_in);
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
    // Try IPv4 parse first, then resolve
    network_address addr;
    try {
        addr.__ipv4 = ipv4::from_string(address);
        addr.address = address;
    } catch (...) {
        // Fallback: treat as hostname (simple resolve via DNS)
        // For now, store address as dummy and let the first send fail
        // if resolution is not available.  The user should call
        // network::resolve() themselves if needed.
        addr.address = address;
        addr.dummy   = true;
    }
    return connect(addr, port);
}

bool socket::connect(const network_address& addr, uint16_t port)
{
    if (m_socket == invalid_socket) {
        // Auto-create socket if we haven't bound yet
        m_socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket == invalid_socket) {
            return false;
        }
    }

    sockaddr_storage ss{};
    fill_sockaddr(ss, addr, port);

    socklen_t len = sizeof(sockaddr_in);
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

size_t socket::sendTo(const std::bytearray& data,
                      const network_address& dest, uint16_t port)
{
    if (m_socket == invalid_socket || data.rawSize() == 0) {
        return 0;
    }

    sockaddr_storage ss{};
    fill_sockaddr(ss, dest, port);

    int sent = ::sendto(
        m_socket,
        reinterpret_cast<const char*>(data.rawData()),
        static_cast<int>(data.rawSize()),
        0, // flags
        reinterpret_cast<const sockaddr*>(&ss),
        sizeof(sockaddr_in)
    );

    return (sent == socket_error) ? 0 : static_cast<size_t>(sent);
}

size_t socket::send(const std::bytearray& data)
{
    if (m_socket == invalid_socket || data.rawSize() == 0) {
        return 0;
    }

#ifdef OS_WINDOWS
    int sent = ::send(
        m_socket,
        reinterpret_cast<const char*>(data.rawData()),
        static_cast<int>(data.rawSize()),
        0
    );
#else
    ssize_t sent = ::send(
        m_socket,
        data.rawData(),
        data.rawSize(),
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

    dg.data = std::bytearray(buf.data(), static_cast<size_t>(received));

    // Extract sender address
    auto* sin = reinterpret_cast<sockaddr_in*>(&from);
    dg.sender_addr.__ipv4 = ipv4::from_uint32(ntohl(sin->sin_addr.s_addr));
    dg.sender_addr.address = dg.sender_addr.__ipv4.to_string();
    dg.sender_port = ntohs(sin->sin_port);

    return dg;
}

// ---- basic_sclstream ----

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

std::bytearray socket::read(size_t bytes)
{
    if (m_socket == invalid_socket || bytes == 0) {
        return std::bytearray();
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
        return std::bytearray();
    }

    return std::bytearray(buf.data(), static_cast<size_t>(received));
}

std::bytearray socket::readAll()
{
    return read(MAX_UDP_PAYLOAD);
}

size_t socket::write(const std::bytearray& data)
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
