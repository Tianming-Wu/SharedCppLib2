/*
    DNS module for SharedCppLib2, as part of the Network intergration
    module.

    DNS query does not support using DNS over HTTPS.
*/

#pragma once

#include "network.hpp"
#include "network_platform.hpp"

namespace network::dns {

/// @brief How a lookup ended. `ok` only means the resolver answered.
enum class dns_status : std::uint8_t {
    ok = 0,
    not_found,          ///< the name does not exist
    temporary_failure,  ///< the resolver could not answer now; try again later
    server_failure,     ///< the resolver answered with a failure
    failed,             ///< anything else
};

/// @brief The result of one lookup: what was found, and why nothing was.
/// @note A name can resolve to several addresses, of both families, and the
///       order they arrive in is the resolver's.
struct dns_query_result {
    std::string hostname;
    std::vector<network_address> addresses;
    dns_status status = dns_status::ok;
    std::string status_text;  ///< platform message, empty on success

    /// @brief True when the lookup answered and found at least one address.
    bool ok() const;

    /// @brief The addresses of one family, in their original order.
    std::vector<network_address> by_family(network_address::kind kind) const;

    /// @brief The first address of the preferred family, falling back to the
    ///        other one; an invalid address when there is nothing to return.
    network_address preferred(ip_preference preference = ip_preference::ipv4_first) const;
};

struct dns_query_server {
    std::string ipv4;
    std::string ipv6;
};

dns_query_result dns_query(const std::string& hostname);
dns_query_result dns_query(const std::wstring& hostname);

// Warning: This function changes the system DNS settings, not current process scope.
bool setDNSServers(const std::vector<network_address>& servers);

} // namespace network::dns