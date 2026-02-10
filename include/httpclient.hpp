/*
    HTTP Client module as part of the network library.
    Tianming Wu <https://github.com/Tianming-Wu> 2026.2.9

    Note that this is an individual link target and is not included in the main
    network library by default.
    You don't need to link network module separately when using http client module.
*/

#pragma once

#include "network.hpp"
#include "basics.hpp"

#include "http.hpp"

#include "http.h"

namespace network::http {

class client
{
public:
    client();
    ~client();

    enable_move_only(client)

    bool connect(const std::string& url, std::optional<uint16_t> port = std::nullopt);

    std::string get(const std::string& path);
    std::string post(const std::string& path, const std::string& data);

    bool disconnect();

private:
    network_address m_serverAddress;

};


} // namespace network::http



/*

    Usage Example (designed):

        network::http::client cli;
        cli.connect("http://example.com");

        std::string response;

        if(cli.waitForConnected(30000)) { // wait for 30 seconds
            response = cli.get("/index.html");
        } else {
            // handle connection timeout
        }

        cli.disconnect();

*/