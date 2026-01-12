#include "network.hpp"

#include <stringlist.hpp>
#include <platform.hpp> // for platform specific includes and flags

namespace network {

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

bool inet_addr::valid() const
{
    return __ipv4.valid() || __ipv6.valid();
}

bool ping(const inet_addr &addr, std::chrono::milliseconds timeout)
{
#ifdef OS_WINDOWS
    // Windows implementation

    


#else
    // Unix implementation

#endif
}

inet_addr resolve(const std::string &hostname)
{
    return inet_addr();
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
