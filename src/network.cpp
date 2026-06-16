#include "network.hpp"

#include "network_platform.hpp" // for platform specific includes and flags
#include "typemask.hpp"

// since we already linked to basics lib, we can use these for some simplicity.
#include "stringlist.hpp"

#ifdef OS_WINDOWS
    SOCKET _n_sock = INVALID_SOCKET;
#endif

namespace network {


void init() noexcept
{
    if(_n_sock != INVALID_SOCKET) {
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

ipv4 network_address::to_ipv4() const { return __ipv4; }

ipv6 network_address::to_ipv6() const { return __ipv6; }

bool network_address::valid() const
{
    return __ipv4.valid() || __ipv6.valid();
}

bool ping(const network_address &addr, std::chrono::milliseconds timeout)
{
#ifdef OS_WINDOWS
    // Windows implementation

    return false;

#else
    // Unix implementation

    return false;
#endif
}

network_address resolve(const std::string &hostname)
{
    return network_address();
}

std::string ipv6::to_string() const
{
    // In this function, we need to consider IPv6 address compression rules.
    // Find the longest sequence of zero blocks
    size_t max_start = 0;
    size_t max_length = 0;
    size_t current_start = 0;
    size_t current_length = 0;

    for (size_t i = 0; i < 8; ++i) {
        if (blocks[i] == 0) {
            if (current_length == 0) {
                current_start = i;
            }
            current_length++;
        } else {
            if (current_length > max_length) {
                max_start = current_start;
                max_length = current_length;
            }
            current_length = 0;
        }
    }

    if (current_length > max_length) {
        max_start = current_start;
        max_length = current_length;
    }

    // If we found a sequence of zeros that is at least two blocks long, compress it
    if (max_length >= 2) {
        std::string result;
        for (size_t i = 0; i < 8; ++i) {
            if (i == max_start && max_length >= 2) {
                result += "::";
                i += max_length - 1; // Skip the compressed blocks
            } else {
                if (i > 0) {
                    result += ":";
                }
                result += std::to_string(blocks[i]);
            }
        }
        return result;
    }

    // If no compression is needed, return the uncompressed string
    return to_string_nocompress();
}

std::string ipv6::to_string_nocompress() const
{
    // We don't need to compress here.

    std::string result;
    for (size_t i = 0; i < 8; ++i) {
        if (i > 0) {
            result += ":";
        }
        result += std::to_string(blocks[i]);
    }
    return result;
}

ipv6 ipv6::from_string(const std::string &str)
{
    ipv6 ip_addr;

    for(size_t i = 0; i < 8; ++i) {
        ip_addr.blocks[i] = 0;
    }

    // we use stringlist for simplicity.
    std::stringlist strl = std::stringlist::split(str, ':');
    if(strl.size() > 8) {
        throw network_error("Invalid IPv6 address: too many blocks");
    }

    size_t block_index = 0;
    bool compressed = false;

    for(size_t i = 0; i < strl.size(); ++i) {
        if(strl[i].empty()) {
            if(compressed) {
                throw network_error("Invalid IPv6 address: multiple '::'");
            }
            compressed = true;
            block_index += 8 - strl.size() + 1; // Skip the compressed blocks
        } else {
            // We checked "too many blocks" before, so we can safely parse the block here.
            ip_addr.blocks[block_index++] = static_cast<uint16_t>(std::stoi(strl[i], nullptr, 16));
        }
    }

    return ip_addr;
}

std::bytearray ipv6::to_bytearray() const noexcept
{
    std::bytearray ba(16);
    for (size_t i = 0; i < 8; ++i) {
        ba[i * 2] = static_cast<std::byte>((blocks[i] >> 8) & 0xFF);
        ba[i * 2 + 1] = static_cast<std::byte>(blocks[i] & 0xFF);
    }
    return ba;
}

ipv6 ipv6::from_bytearray(const std::bytearray &ba)
{
    ipv6 ip_addr;
    if (ba.rawSize() != 16) {
        throw network_error("Invalid bytearray size for IPv6 address");
    }

    for (size_t i = 0; i < 8; ++i) {
        ip_addr.blocks[i] = (static_cast<uint16_t>(ba.rawData()[i * 2]) << 8) | static_cast<uint16_t>(ba.rawData()[i * 2 + 1]);
    }
    return ip_addr;
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
