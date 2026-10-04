/*
    TCP Client module as part of the network library.
*/

#pragma once

#include "network.hpp"
#include "tcp.hpp"

#include "basics.hpp"
#include "stream.hpp"

#include <string>
#include <chrono>
#include <mutex>

namespace network::tcp {

/// @brief TCP client for connecting and communicating with remote servers
class client : public scl2::transport_interface
{
public:
    client();
    ~client();

    enable_move_only(client)

    /// @brief Connect to a remote server
    /// @param address Target server address (IP or hostname)
    /// @param port Target port
    /// @return true if connection successful
    bool connect(const std::string& address, uint16_t port) override;

    /// @brief Connect with a time limit for the connection itself.
    bool connect(const std::string& address, uint16_t port, std::chrono::milliseconds timeout) override;

    /// @brief Connect to a remote server using network_address
    /// @note A host name is resolved first (IPv4 addresses preferred), and the
    ///       IPv6 candidates are tried when no IPv4 address can be reached.
    bool connect(const network_address& address, uint16_t port);

    /// @brief Connect to a remote server, giving up after @p timeout.
    /// @note Also resolves a host name, within the same time budget.
    bool connect(const network_address& address, uint16_t port, std::chrono::milliseconds timeout);

    /// @brief Time limit for the next connect(); 0 means wait as long as it takes.
    void set_connect_timeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds connect_timeout() const;

    /// @brief Why the last connect() returned false; empty when there is nothing to say.
    std::string lastError() const;
    
    /// @brief Disconnect from the server
    void disconnect() override;
    
    /// @brief Check if connected to a server
    bool is_connected() const override;
    
    /// @brief Check if the connection is valid
    bool valid() override;
    
    /// @brief Check if data is ready to read
    bool readyRead() override;
    
    /// @brief Get number of bytes available to read
    size_t available() override;
    
    /// @brief Read specified number of bytes
    scl2::bytearray read(size_t bytes) override;
    
    /// @brief Read all available data
    scl2::bytearray readAll() override;
    
    /// @brief Write data to the server
    size_t write(const scl2::bytearray& data);
    
    /// @brief Get the connected server address
    network_address server_address() const;
    
    /// @brief Get the connected server port
    uint16_t server_port() const;

private:
    socket_t m_socket = invalid_socket;
    network_address m_server_address;
    uint16_t m_server_port = 0;
    std::chrono::milliseconds m_connect_timeout{ 0 };
    std::string m_last_error;
    mutable std::mutex m_mutex;
};

} // namespace network::tcp
