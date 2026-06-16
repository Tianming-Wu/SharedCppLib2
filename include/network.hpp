/*
    This file is only specification of protocols.
*/

#pragma once
#include <string>
#include <chrono>
#include <stdexcept>

#include <array>
#include <cstdint>

#include "bytearray.hpp"

namespace network {

class network_error : public std::runtime_error
{
public:
    network_error(const std::string& ex)
        : runtime_error(ex)
    {}
};


// network platform initialization
// Note: You don't really need to call this function.
// The library will automatically initialize the network platform when needed.
void init() noexcept;

// we can't really cleanup in destructor, because we might have multiple instances
// of clients and servers. And we can't cleanup if any of them is still alive.
// We CAN ofcourse use reference counting, but that would be more complex and is not
// really useful. So just cleanup manually when the program is about to exit.
void cleanup() noexcept;

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
    uint16_t blocks[8];

    inline constexpr std::array<uint8_t, 16> to_bytes() const noexcept {
        std::array<uint8_t, 16> bytes{};
        for (size_t i = 0; i < 8; ++i) {
            bytes[i * 2] = static_cast<uint8_t>((blocks[i] >> 8) & 0xFF);
            bytes[i * 2 + 1] = static_cast<uint8_t>(blocks[i] & 0xFF);
        }
        return bytes;
    }

    // Return the string representation of IPv6 address, with zero compression if possible.
    std::string to_string() const;
    std::string to_string_nocompress() const;

    static ipv6 from_string(const std::string& str);

    std::bytearray to_bytearray() const noexcept;
    static ipv6 from_bytearray(const std::bytearray& ba);

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


// The definitions of these classes are in their respective headers.
namespace http {}
namespace dns {}


} // namespace network

std::ostream& operator<<(std::ostream& os, const network::ipv4& addr);
std::istream& operator>>(std::istream& is, network::ipv4& addr);

/*
    Protocol specifications

    ipv4:
        - 4 octets, each 8 bits, total 32 bits
        - usually represented in dot-decimal notation (e.g. "192.168.1.1")
        - can be converted to/from uint32_t for easier storage and comparison

    ipv6:
        - 8 groups of 4 hexadecimal digits, each group representing 16 bits, total 128 bits
        - usually represented in colon-hexadecimal notation (e.g. "2001:0db8:85a3:0000:0000:8a2e:0370:7334")
        - can be compressed by omitting leading zeros and consecutive groups of zeros
            For example, "2001:0db8:85a3::8a2e:0370:7334" is a valid compressed form of the above address
        - can be converted to/from a 16-byte array for easier storage and comparison


    network_address:



*/