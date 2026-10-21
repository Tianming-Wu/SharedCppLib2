#include "network.hpp"

#include "network_platform.hpp" // for platform specific includes and flags
#include "typemask.hpp"

#include <vector>

#ifdef OS_WINDOWS
    #include <iphlpapi.h>  // IPAddr / ICMP_ECHO_REPLY / IP_SUCCESS
    #include <icmpapi.h>   // IcmpCreateFile / IcmpSendEcho
    #pragma comment(lib, "iphlpapi.lib")
#else
    #include <poll.h>
#endif

// since we already linked to basics lib, we can use these for some simplicity.
#include "stringlist.hpp"

#ifdef OS_WINDOWS
    static bool _n_started = false;
#endif

namespace network {


#ifndef OS_WINDOWS
namespace {

/// @brief Internet checksum (RFC 1071) for an ICMP message.
uint16_t icmp_checksum(const void* data, size_t length)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t sum = 0;
    while (length > 1) {
        sum += static_cast<uint32_t>((bytes[0] << 8) | bytes[1]);
        bytes += 2;
        length -= 2;
    }
    if (length == 1) sum += static_cast<uint32_t>(bytes[0] << 8);
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return static_cast<uint16_t>(~sum);
}

} // namespace
#endif


void init() noexcept
{
#ifdef OS_WINDOWS
    if (_n_started) {
        return;
    }

    WSADATA wsaData;
    WSAStartup(0x202, &wsaData);
    _n_started = true;
#else
    // No initialization needed on Unix-like systems
#endif
}

void cleanup() noexcept
{
#ifdef OS_WINDOWS
    if (!_n_started) {
        return;
    }
    WSACleanup();
    _n_started = false;
#endif
}

std::string ipv4::to_string() const
{
    return std::to_string(octet1) + "." + std::to_string(octet2) + "."
         + std::to_string(octet3) + "." + std::to_string(octet4);
}

ipv4 ipv4::from_string(const std::string &str)
{
    // The platform parser is the strict one: four parts, each 0..255.
    in_addr parsed{};
    if (::inet_pton(AF_INET, str.c_str(), &parsed) != 1) {
        throw network_error("Invalid ipv4 address");
    }
    return ipv4::from_uint32(ntohl(parsed.s_addr));
}

bool ipv4::valid() const
{
    // All values between 0 and 255 are valid
    return true;
}

ipv4 network_address::to_ipv4() const
{
    if (type != kind::ipv4) throw network_error("network_address does not hold an IPv4 address");
    return ipv4_addr;
}

ipv6 network_address::to_ipv6() const
{
    if (type != kind::ipv6) throw network_error("network_address does not hold an IPv6 address");
    return ipv6_addr;
}

network_address network_address::parse(const std::string& text)
{
    network_address result;
    result.address = text;

    try {
        result.ipv4_addr = ipv4::from_string(text);
        result.type = kind::ipv4;
        return result;
    } catch (...) {}

    try {
        result.ipv6_addr = ipv6::from_string(text);
        result.type = kind::ipv6;
        return result;
    } catch (...) {}

    result.type = kind::hostname;
    return result;
}

std::string network_address::to_string() const
{
    switch (type) {
        case kind::ipv4: return ipv4_addr.to_string();
        case kind::ipv6: return ipv6_addr.to_string();
        case kind::hostname:
        case kind::unspecified: break;
    }
    return address;
}

bool ping(const network_address &addr, std::chrono::milliseconds timeout)
{
    return ping_rtt(addr, timeout).has_value();
}

std::optional<std::chrono::milliseconds> ping_rtt(const network_address &addr, std::chrono::milliseconds timeout)
{
    // A host name has to be resolved by the caller: this module knows nothing of dns.
    if (addr.type != network_address::kind::ipv4) {
        return std::nullopt;  // ICMPv6 is not built yet, and neither is anything else
    }

    const DWORD budget = (timeout.count() > 0) ? static_cast<DWORD>(timeout.count()) : 1;
    const uint32_t target = addr.ipv4_addr.to_uint32();

#ifdef OS_WINDOWS
    HANDLE handle = ::IcmpCreateFile();
    if (handle == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }

    std::array<uint8_t, 32> payload{};
    for (size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<uint8_t>(i);

    std::vector<uint8_t> reply(sizeof(ICMP_ECHO_REPLY) + payload.size() + 8);

    const DWORD replies = ::IcmpSendEcho(handle, htonl(target), payload.data(),
                                         static_cast<WORD>(payload.size()), nullptr,
                                         reply.data(), static_cast<DWORD>(reply.size()), budget);
    ::IcmpCloseHandle(handle);

    if (replies == 0) {
        return std::nullopt;
    }

    const auto* echo = reinterpret_cast<const ICMP_ECHO_REPLY*>(reply.data());
    if (echo->Status != IP_SUCCESS) {
        return std::nullopt;
    }
    return std::chrono::milliseconds(echo->RoundTripTime);
#else
    // Unprivileged ICMP: a DGRAM socket with IPPROTO_ICMP, which Linux allows.
    // Falls back to a raw socket, which needs privileges.
    int sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    bool raw_socket = false;
    if (sock < 0) {
        sock = ::socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        raw_socket = true;
    }
    if (sock < 0) {
        return std::nullopt;
    }

    struct icmp_echo {
        uint8_t type;
        uint8_t code;
        uint16_t checksum;
        uint16_t id;
        uint16_t sequence;
        uint8_t payload[32];
    } request{};

    request.type = 8;  // echo request
    request.id = static_cast<uint16_t>(::getpid() & 0xFFFF);
    request.sequence = 1;
    for (size_t i = 0; i < sizeof(request.payload); ++i) request.payload[i] = static_cast<uint8_t>(i);
    request.checksum = icmp_checksum(&request, sizeof(request));

    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = htonl(target);

    const auto started = std::chrono::steady_clock::now();

    if (::sendto(sock, &request, sizeof(request), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to)) < 0) {
        ::close(sock);
        return std::nullopt;
    }

    pollfd waiting{};
    waiting.fd = sock;
    waiting.events = POLLIN;
    if (::poll(&waiting, 1, static_cast<int>(budget)) <= 0) {
        ::close(sock);
        return std::nullopt;
    }

    uint8_t buffer[512] = {};
    const ssize_t received = ::recv(sock, buffer, sizeof(buffer), 0);
    ::close(sock);
    if (received <= 0) {
        return std::nullopt;
    }

    // A raw socket keeps the IP header in front of the ICMP message.
    const size_t offset = raw_socket ? static_cast<size_t>((buffer[0] & 0x0F) * 4) : 0;
    if (static_cast<size_t>(received) < offset + 8) {
        return std::nullopt;
    }

    const uint8_t* answer = buffer + offset;
    if (answer[0] != 0) {
        return std::nullopt;  // not an echo reply
    }
    if (static_cast<uint16_t>((answer[4] << 8) | answer[5]) != request.id) {
        return std::nullopt;
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
#endif
}

std::string ipv6::to_string() const
{
    // Let the platform print it, so that whatever it accepted comes back in one
    // canonical form.
    const std::array<uint8_t, 16> bytes = to_bytes();
    char text[INET6_ADDRSTRLEN] = {};
    if (::inet_ntop(AF_INET6, bytes.data(), text, sizeof(text)) == nullptr) {
        throw network_error("Invalid IPv6 address");
    }

    std::string result = text;
    if (scope_id != 0) result += "%" + std::to_string(scope_id);
    return result;
}

std::string ipv6::to_string_nocompress() const
{
    // We don't need to compress here.

    std::string result;
    for (size_t i = 0; i < 8; ++i) {
        if (i > 0) {
            result += ":";
        }
        result += std::to_string(blocks[i]);
    }
    if (scope_id != 0) result += "%" + std::to_string(scope_id);
    return result;
}

ipv6 ipv6::from_string(const std::string &str)
{
    ipv6 ip_addr;

    // A trailing %<zone> is the scope index of a link-local address, not part of it.
    std::string text = str;
    const size_t percent = text.find('%');
    if (percent != std::string::npos) {
        const std::string zone = text.substr(percent + 1);
        text = text.substr(0, percent);
        try {
            ip_addr.scope_id = static_cast<uint32_t>(std::stoul(zone));
        } catch (...) {
            throw network_error("Invalid IPv6 zone index");
        }
    }

    // The platform parser accepts every form we care about: "::" compression,
    // a trailing dotted quad, mixed case.
    std::array<uint8_t, 16> bytes{};
    if (::inet_pton(AF_INET6, text.c_str(), bytes.data()) != 1) {
        throw network_error("Invalid IPv6 address");
    }

    for (size_t i = 0; i < 8; ++i) {
        ip_addr.blocks[i] = static_cast<uint16_t>((bytes[i * 2] << 8) | bytes[i * 2 + 1]);
    }

    return ip_addr;
}

scl2::bytearray ipv6::to_bytearray() const noexcept
{
    scl2::bytearray ba(16);
    for (size_t i = 0; i < 8; ++i) {
        ba[i * 2] = static_cast<std::byte>((blocks[i] >> 8) & 0xFF);
        ba[i * 2 + 1] = static_cast<std::byte>(blocks[i] & 0xFF);
    }
    return ba;
}

ipv6 ipv6::from_bytearray(const scl2::bytearray &ba)
{
    ipv6 ip_addr;
    if (ba.size() != 16) {
        throw network_error("Invalid bytearray size for IPv6 address");
    }

    for (size_t i = 0; i < 8; ++i) {
        ip_addr.blocks[i] = (static_cast<uint16_t>(ba.data()[i * 2]) << 8) | static_cast<uint16_t>(ba.data()[i * 2 + 1]);
    }
    return ip_addr;
}

bool ipv6::valid() const
{
    for (size_t i = 0; i < 8; ++i) {
        if (blocks[i] != 0) return true;
    }
    return false;
}

} // namespace network

std::ostream &operator<<(std::ostream &os, const network::ipv4 &addr)
{
    std::string str = addr.to_string();
    os << str;
    return os;
}

std::istream &operator>>(std::istream &is, network::ipv4 &addr)
{
    std::string str;
    is >> str;
    addr.from_string(str);
    return is;
}
