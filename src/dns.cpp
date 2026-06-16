#include "dns.hpp"

#include "network_platform.hpp"
#include "platform.hpp"

namespace network::dns {

dns_query_result dns_query(const std::wstring &hostname)
{
    dns_query_result result;
    result.hostname = hostname;

    ADDRINFOW *addr_info = nullptr;
    GetAddrInfoW(hostname.c_str(), nullptr, nullptr, &addr_info);
    
    for(ADDRINFOW* ptr = addr_info; ptr != nullptr; ptr = ptr->ai_next) {
        wchar_t ip[INET_ADDRSTRLEN];
        if(ptr->ai_family == AF_INET) {
            InetNtopW(AF_INET, &((struct sockaddr_in*)ptr->ai_addr)->sin_addr, ip, sizeof(ip));
        } else if(ptr->ai_family == AF_INET6) {
            InetNtopW(AF_INET6, &((struct sockaddr_in6*)ptr->ai_addr)->sin6_addr, ip, sizeof(ip));
        }
        network_address addr;

        addr.address = platform::wstringToString(std::wstring(ip));
        addr.__ipv4 = ipv4::from_string(addr.address);

        result.addresses.push_back(addr);
    }
    
    return result;
}

bool setDNSServers(const std::vector<network_address> &servers)
{
    return false;
}

} // namespace network::dns