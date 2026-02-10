/*
    HTTP Server module as part of the network library.
    Tianming Wu <https://github.com/Tianming-Wu> 2026.1.31

    Note that this is an individual link target and is not included in the main
    network library by default.
    You don't need to link network module separately when using http server module.

    This module provides a simple HTTP server implementation, which can be used for
    testing or lightweight applications.
*/

#pragma once

#include "network.hpp"
#include "network_platform.hpp"

#include "http.hpp"

#include "basics.hpp"
#include "stream.hpp"

#include <map>
#include <vector>
#include <mutex>

/// @brief HTTP protocol related classes and functions
namespace network::http {

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

protected:
    int tick(); // the single working loop iteration


private:
    network_address m_address;
    uint16_t m_port;
    mutable std::mutex m_mutex;

    std::map<client_id, client_info> m_clients;
    socket_t m_listen_socket = invalid_socket;
    client_id m_next_client_id = 1;
    bool m_running = false;

};

server& operator>> (server& srv, std::string& request);
server& operator<< (server& srv, const std::string& response);



} // namespace network::http