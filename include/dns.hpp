/*
    DNS module for SharedCppLib2, as part of the Network intergration
    module.

    DNS query does not support using DNS over HTTPS.
*/

#pragma once

#include "network.hpp"
#include "network_platform.hpp"

namespace network::dns {

struct dns_query_result {
    std::wstring hostname;
    std::vector<network_address> addresses;
};

struct dns_query_server {
    std::wstring ipv4;
    std::wstring ipv6;
};

dns_query_result dns_query(const std::wstring& hostname);

// Warning: This function changes the system DNS settings, not current process scope.
bool setDNSServers(const std::vector<network_address>& servers);

} // namespace network::dns