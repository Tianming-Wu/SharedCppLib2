/*
    This file is only specification of protocols.
*/

#pragma once
#include <string>
#include <chrono>
#include <stdexcept>

namespace network {

class network_error : public std::runtime_error
{
public:
    network_error(const std::string& ex)
        : runtime_error(ex)
    {}
};

void init() noexcept;
void cleanup() noexcept; // we can't really cleanup in destructor

struct ipv4 {
    uint8_t octet1, octet2, octet3, octet4;

    inline constexpr uint32_t to_uint32() const noexcept {
        return (static_cast<uint32_t>(octet1) << 24) | (static_cast<uint32_t>(octet2) << 16) | (static_cast<uint32_t>(octet3) << 8)  | (static_cast<uint32_t>(octet4));
    }
    inline static constexpr ipv4 from_uint32(uint32_t addr) noexcept {
        return ipv4 { static_cast<uint8_t>((addr >> 24) & 0xFF), static_cast<uint8_t>((addr >> 16) & 0xFF), static_cast<uint8_t>((addr >> 8) & 0xFF), static_cast<uint8_t>(addr & 0xFF) };
    }

    std::string to_string() const;
    static ipv4 from_string(const std::string& str);

    bool valid() const;
};

struct ipv6 {



    bool valid() const;
};

struct network_address {
    std::string address;
    
    ipv4 to_ipv4() const;
    ipv6 to_ipv6() const;

    bool valid() const;

    ipv4 __ipv4;
    ipv6 __ipv6;

    bool dummy = false; // If this flag was set, the address is just a placeholder and does not represent a real address
};

// functions
bool ping(const network_address& addr, std::chrono::milliseconds timeout = std::chrono::seconds(1));
network_address resolve(const std::string& hostname);


namespace http {

// client& operator>> (client& cli, std::string& response);
// client& operator<< (client& cli, const std::string& request);

} // namespace http


/// @brief DNS lookup functions
namespace dns {

} // namespace dns


} // namespace network

//  namespace

std::ostream& operator<<(std::ostream& os, const network::ipv4& addr);
std::istream& operator>>(std::istream& is, network::ipv4& addr);