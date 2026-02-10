#include "dns.hpp"

#include "network_platform.hpp"

namespace network::dns {

dns_query_result dns_query(const std::string &hostname)
{
    dns_query_result result;
    result.hostname = hostname;

    ::hostent *host = gethostbyname(hostname.c_str());

    for(size_t i = 0; host->h_addr_list[i] != nullptr; ++i) {
        char* ip = inet_ntoa(*(struct in_addr *)host->h_addr_list[i]);
        network_address addr;

        addr.address = std::string(ip);
        addr.__ipv4 = ipv4::from_string(addr.address);

        result.addresses.push_back(addr);
    }
    
    return result;
}



} // namespace network::dns