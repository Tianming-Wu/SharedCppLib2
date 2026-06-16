/*
    TCP Client module as part of the network library.
*/

#pragma once

#include "network.hpp"
#include "tcp.hpp"

#include "basics.hpp"
#include "stream.hpp"

#include <string>
#include <mutex>

namespace network::tcp {

/// @brief TCP client for connecting and communicating with remote servers
class client : public basic_sclstream
{
public:
    client();
    ~client();

    enable_move_only(client)

    /// @brief Connect to a remote server
    /// @param address Target server address (IP or hostname)
    /// @param port Target port
    /// @return true if connection successful
    bool connect(const std::string& address, uint16_t port);
    
    /// @brief Connect to a remote server using network_address
    bool connect(const network_address& address, uint16_t port);
    
    /// @brief Disconnect from the server
    void disconnect();
    
    /// @brief Check if connected to a server
    bool is_connected() const;
    
    /// @brief Check if the connection is valid
    bool valid() override;
    
    /// @brief Check if data is ready to read
    bool readyRead() override;
    
    /// @brief Get number of bytes available to read
    size_t available() override;
    
    /// @brief Read specified number of bytes
    std::bytearray read(size_t bytes) override;
    
    /// @brief Read all available data
    std::bytearray readAll() override;
    
    /// @brief Write data to the server
    size_t write(const std::bytearray& data);
    
    /// @brief Get the connected server address
    network_address server_address() const;
    
    /// @brief Get the connected server port
    uint16_t server_port() const;

private:
    socket_t m_socket = invalid_socket;
    network_address m_server_address;
    uint16_t m_server_port = 0;
    mutable std::mutex m_mutex;
};

} // namespace network::tcp
