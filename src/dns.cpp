#include "dns.hpp"

#include "network_platform.hpp"
#include "platform.hpp"
#include "string.hpp"

#include <set>
#include <system_error>

#ifndef OS_WINDOWS
#  include <netdb.h>
#endif

namespace network::dns {

namespace {

/// @brief Map a resolver error code onto our status.
dns_status status_from(int code)
{
#ifdef OS_WINDOWS
    switch (code) {
        case WSAHOST_NOT_FOUND:
        case WSANO_DATA:      return dns_status::not_found;
        case WSATRY_AGAIN:    return dns_status::temporary_failure;
        case WSANO_RECOVERY:  return dns_status::server_failure;
        default:              return dns_status::failed;
    }
#else
    switch (code) {
        case EAI_NONAME:      return dns_status::not_found;
        case EAI_AGAIN:       return dns_status::temporary_failure;
        case EAI_FAIL:        return dns_status::server_failure;
        default:              return dns_status::failed;
    }
#endif
}

std::string message_from(int code)
{
#ifdef OS_WINDOWS
    std::string text = std::system_category().message(code);
#else
    std::string text = ::gai_strerror(code);
#endif
    if (text.empty()) {
        text = "resolver error " + std::to_string(code);
    }
    return text;
}

} // namespace


dns_query_result dns_query(const std::string& hostname)
{
    dns_query_result result;
    result.hostname = hostname;

    if (hostname.empty()) {
        result.status = dns_status::not_found;
        result.status_text = "no host name given";
        return result;
    }

#ifdef OS_WINDOWS
    // No hints beyond the family: asking for every socket type is fine, the
    // duplicates it produces are dropped below.
    ADDRINFOW hints{};
    hints.ai_family = AF_UNSPEC;

    ADDRINFOW* raw = nullptr;
    const std::wstring wide = scl2::str_to_wstr(hostname);
    const int code = ::GetAddrInfoW(wide.c_str(), nullptr, &hints, &raw);

    if (code != 0) {
        result.status = status_from(code);
        result.status_text = message_from(code);
        return result;
    }

    std::set<std::string> seen;
    for (ADDRINFOW* it = raw; it != nullptr; it = it->ai_next) {
        try {
            network_address addr;
            wchar_t text[INET6_ADDRSTRLEN] = {};

            if (it->ai_family == AF_INET) {
                ::InetNtopW(AF_INET, &reinterpret_cast<sockaddr_in*>(it->ai_addr)->sin_addr,
                            text, INET6_ADDRSTRLEN);
                addr.type = network_address::kind::ipv4;
                addr.ipv4_addr = ipv4::from_string(scl2::wstr_to_str(text));
            } else if (it->ai_family == AF_INET6) {
                auto* sin6 = reinterpret_cast<sockaddr_in6*>(it->ai_addr);
                ::InetNtopW(AF_INET6, &sin6->sin6_addr, text, INET6_ADDRSTRLEN);
                addr.type = network_address::kind::ipv6;
                addr.ipv6_addr = ipv6::from_string(scl2::wstr_to_str(text));
                addr.ipv6_addr.scope_id = sin6->sin6_scope_id;
            } else {
                continue;  // a family we have no address type for
            }

            addr.address = addr.to_string();
            if (addr.address.empty() || !seen.insert(addr.address).second) continue;
            result.addresses.push_back(addr);
        } catch (...) {
            continue;  // an address we cannot represent is skipped, not fatal
        }
    }
    ::FreeAddrInfoW(raw);
#else
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;

    addrinfo* raw = nullptr;
    const int code = ::getaddrinfo(hostname.c_str(), nullptr, &hints, &raw);

    if (code != 0) {
        result.status = status_from(code);
        result.status_text = message_from(code);
        return result;
    }

    std::set<std::string> seen;
    for (addrinfo* it = raw; it != nullptr; it = it->ai_next) {
        try {
            network_address addr;
            char text[INET6_ADDRSTRLEN] = {};

            if (it->ai_family == AF_INET) {
                ::inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(it->ai_addr)->sin_addr,
                            text, sizeof(text));
                addr.type = network_address::kind::ipv4;
                addr.ipv4_addr = ipv4::from_string(text);
            } else if (it->ai_family == AF_INET6) {
                auto* sin6 = reinterpret_cast<sockaddr_in6*>(it->ai_addr);
                ::inet_ntop(AF_INET6, &sin6->sin6_addr, text, sizeof(text));
                addr.type = network_address::kind::ipv6;
                addr.ipv6_addr = ipv6::from_string(text);
                addr.ipv6_addr.scope_id = sin6->sin6_scope_id;
            } else {
                continue;
            }

            addr.address = addr.to_string();
            if (addr.address.empty() || !seen.insert(addr.address).second) continue;
            result.addresses.push_back(addr);
        } catch (...) {
            continue;  // an address we cannot represent is skipped, not fatal
        }
    }
    ::freeaddrinfo(raw);
#endif

    if (result.addresses.empty()) {
        result.status = dns_status::not_found;
        result.status_text = "the resolver returned no usable address";
        return result;
    }

    result.status = dns_status::ok;
    return result;
}

dns_query_result dns_query(const std::wstring& hostname)
{
    return dns_query(scl2::wstr_to_str(hostname));
}

bool dns_query_result::ok() const
{
    return status == dns_status::ok && !addresses.empty();
}

std::vector<network_address> dns_query_result::by_family(network_address::kind kind) const
{
    std::vector<network_address> out;
    for (const network_address& addr : addresses) {
        if (addr.type == kind) out.push_back(addr);
    }
    return out;
}

network_address dns_query_result::preferred(ip_preference preference) const
{
    const network_address::kind first = (preference == ip_preference::ipv6_first)
        ? network_address::kind::ipv6 : network_address::kind::ipv4;
    const network_address::kind second = (first == network_address::kind::ipv4)
        ? network_address::kind::ipv6 : network_address::kind::ipv4;

    for (const network_address& addr : addresses) {
        if (addr.type == first) return addr;
    }
    for (const network_address& addr : addresses) {
        if (addr.type == second) return addr;
    }
    return network_address();
}

bool setDNSServers(const std::vector<network_address> &servers)
{
    (void)servers;
    return false;
}

} // namespace network::dns
