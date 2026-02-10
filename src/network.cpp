#include "network.hpp"

#include <stringlist.hpp>
#include <platform.hpp> // for platform specific includes and flags

int _n_sock = -1;

namespace network {


void init() noexcept
{
    if(_n_sock != -1) {
        return;
    }

    WSADATA wsaData;
	WSAStartup(0x202, &wsaData);

    _n_sock = socket(AF_INET, SOCK_DGRAM, 0); // udp
    _n_sock = socket(AF_INET, SOCK_STREAM, 0); // tcp
}

void cleanup() noexcept
{
    // currently, do nothing because the OS will clean up everything on exit
}

std::string ipv4::to_string() const
{
    return std::string();
}

ipv4 ipv4::from_string(const std::string &str)
{
    uint8_t cs[4];
    uint8_t csi = 0;

    std::stringlist cid = std::stringlist::split(str, ".");
    if(cid.size() != 4) throw network_error("Invalid ipv4 address");

    try {
        for(const auto& s : cid) {
            uint8_t c = static_cast<uint8_t>(std::stoi(s));
            cs[csi++] = c;
        }
    } catch(...) {
        throw network_error("Invalid ipv4 address");
    }

    return ipv4{ cs[0], cs[1], cs[2], cs[3] };
}

bool ipv4::valid() const
{
    // All values between 0 and 255 are valid
    return true;
}

bool network_address::valid() const
{
    return __ipv4.valid() || __ipv6.valid();
}

bool ping(const network_address &addr, std::chrono::milliseconds timeout)
{
#ifdef OS_WINDOWS
    // Windows implementation

    


#else
    // Unix implementation

#endif
}

network_address resolve(const std::string &hostname)
{
    return network_address();
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
