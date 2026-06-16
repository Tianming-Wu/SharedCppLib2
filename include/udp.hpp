/*
    UDP protocol shared types and socket class.
    
    UDP is a connectionless, datagram-oriented protocol.
    - No handshake, no connection state
    - Each read/write operates on independent datagrams
    - A single socket can communicate with multiple peers
    - Datagrams may be lost, duplicated, or arrive out of order

    The UDP socket implements basic_sclstream so it can be used as a
    generic stream, but with UDP-specific semantics:
    - read(n)  reads at most n bytes from one datagram (rest is discarded)
    - readAll() reads one complete datagram
    - write()   sends to the default destination (requires prior connect())
*/

#pragma once

#include "network_platform.hpp"
#include "network.hpp"

#include "basics.hpp"
#include "stream.hpp"

#include <cstdint>

namespace network::udp {

#ifdef OS_WINDOWS
using socket_t = SOCKET;
constexpr socket_t invalid_socket = INVALID_SOCKET;
constexpr int socket_error = SOCKET_ERROR;
#else
using socket_t = int;
constexpr socket_t invalid_socket = -1;
constexpr int socket_error = -1;
#endif

/// @brief A received UDP datagram with sender information
struct datagram {
    std::bytearray data;          ///< The received data
    network_address sender_addr;  ///< Sender's IP address
    uint16_t sender_port = 0;     ///< Sender's port
};

/// @brief UDP socket for sending and receiving datagrams
///
/// Supports both connected and unconnected modes:
/// - Unconnected: use sendTo() / receiveFrom() for per-packet addressing
/// - Connected:   use write() / read() with a preset default destination
///
/// Usage (server-like, unconnected):
/// @code
///   udp::socket sock;
///   sock.bind(12345);
///   auto [data, addr, port] = sock.receiveFrom();
///   sock.sendTo(std::bytearray("reply"), addr, port);
/// @endcode
///
/// Usage (client-like, connected):
/// @code
///   udp::socket sock;
///   sock.connect(ipv4{127,0,0,1}, 12345);
///   sock.write(std::bytearray("hello"));
///   auto reply = sock.readAll();
/// @endcode
class socket : public basic_sclstream
{
public:
    socket();
    ~socket();

    enable_move_only(socket)

    // ---- Bind / Connect ----

    /// @brief Bind the socket to a local port on all interfaces
    /// @return true on success
    bool bind(uint16_t port);

    /// @brief Bind the socket to a specific local address and port
    bool bind(const network_address& addr, uint16_t port);

    /// @brief Connect to a default remote destination
    ///
    /// In UDP, connect() does NOT perform a handshake; it merely sets the
    /// default destination for subsequent write() / read() calls.
    /// Packets from other peers are still received via receiveFrom().
    bool connect(const std::string& address, uint16_t port);
    bool connect(const network_address& addr, uint16_t port);

    /// @brief Close the socket and release resources
    void close();

    // ---- Send ----

    /// @brief Send data to a specific destination
    /// @return Number of bytes sent, or 0 on failure
    size_t sendTo(const std::bytearray& data,
                  const network_address& dest, uint16_t port);

    /// @brief Send data to the default destination (requires connect() first)
    /// @return Number of bytes sent, or 0 on failure
    size_t send(const std::bytearray& data);

    // ---- Receive (with peer info) ----

    /// @brief Receive a datagram and its sender address (non-blocking)
    /// @return The received datagram. data.empty() if nothing available.
    datagram receiveFrom();

    /// @brief Receive a datagram and its sender address (blocking, with timeout)
    /// @param timeout Maximum time to wait
    /// @return The received datagram. data.empty() on timeout.
    datagram receiveFrom(std::chrono::milliseconds timeout);

    // ---- basic_sclstream interface ----

    /// @brief Check if the socket is valid (bound or connected)
    bool valid() override;

    /// @brief Check if data is available to read (non-blocking)
    bool readyRead() override;

    /// @brief Get number of bytes available in the next pending datagram
    ///
    /// Returns the size of the first queued datagram, or 0 if none.
    /// Note: unlike TCP, this returns the size of ONE datagram, not the
    /// total bytes in the receive buffer.
    size_t available() override;

    /// @brief Read at most @p bytes from one datagram
    ///
    /// UDP semantics: if the datagram is larger than @p bytes, the
    /// remaining data is discarded (truncation).
    std::bytearray read(size_t bytes) override;

    /// @brief Read one complete datagram
    std::bytearray readAll() override;

    /// @brief Write data to the default destination (requires connect())
    size_t write(const std::bytearray& data) override;

    // ---- Info ----

    /// @brief Get the bound local address (only valid after bind())
    network_address localAddress() const;

    /// @brief Get the bound local port (0 if not bound)
    uint16_t localPort() const;

    /// @brief Check if a default destination has been set via connect()
    bool is_connected() const;

private:
    socket_t m_socket = invalid_socket;
    network_address m_local_address;
    uint16_t m_local_port = 0;

    bool m_connected = false;
    network_address m_default_address;
    uint16_t m_default_port = 0;
    sockaddr_storage m_default_sockaddr{};
    int m_default_sockaddr_len = 0;
};

} // namespace network::udp
