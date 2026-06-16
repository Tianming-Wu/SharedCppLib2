/*
    TCP Server module as part of the network library.

    This is a standalone TCP server implementation that allows users to create
    a TCP server, accept client connections, and read/write data to/from clients.
*/

#pragma once

#include "network.hpp"
#include "tcp.hpp"

#include "basics.hpp"
#include "stream.hpp"

#include <map>
#include <vector>
#include <mutex>

namespace network::tcp {

class server;
typedef int client_id;

struct client_info {
    client_id id;
    socket_t socket;
    sockaddr_in addr;
    mutable std::mutex mutex; // to prevent concurrent access to the same client
};


class server_client_handler : basic_sclstream
{
public:
    server_client_handler(server& srv, client_info& info);
    ~server_client_handler() = default; // does not have ownership of anything

    enable_copy_move(server_client_handler)

    bool readyRead() override final;
    size_t available() override final;

    std::bytearray read(size_t bytes) override final;
    std::bytearray readAll() override final;

    size_t write(const std::bytearray& data);

    bool valid();

    auto lock() -> std::unique_lock<std::mutex>;

private:
    server& m_server;
    client_info& m_client_info;
};


class server {
    friend class server_client_handler;
public:
    server();
    server(uint16_t port);
    server(network_address address, uint16_t port);
    ~server();

    void start();
    void start(uint16_t port);
    void stop();

    uint16_t port() const;
    network_address address() const;

    static std::string hostname(size_t max_length = 255);
    
    std::vector<client_id> clients();

    server_client_handler selectClient(client_id& id);

    auto lock() -> std::unique_lock<std::mutex>;

    /// @brief Process one iteration of the server loop (accept new connections)
    /// @return Number of new clients accepted, or -1 if not running
    ///
    /// This function is not blocking and should be called in a loop by the user to keep the server running.
    /// If user don't call this frequently enough, the server may not accept new clients in time and cause connection timeouts.
    int tick();


private:
    network_address m_address;
    uint16_t m_port;
    mutable std::mutex m_mutex;

    std::map<client_id, client_info> m_clients;
    socket_t m_listen_socket = invalid_socket;
    client_id m_next_client_id = 1;
    bool m_running = false;

};

} // namespace network::tcp
