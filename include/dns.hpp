/*
    DNS module for SharedCppLib2, as part of the Network intergration
    module.

*/

#pragma once

#include "network.hpp"
#include "network_platform.hpp"

namespace network::dns {

struct dns_query_result {
    std::string hostname;
    std::vector<network_address> addresses;
};

dns_query_result dns_query(const std::string& hostname);

bool setDNSServers(const std::vector<network_address>& servers);

} // namespace network::dns