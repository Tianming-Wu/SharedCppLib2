/*
    TCP protocol shared types and utilities for both client and server.
*/

#pragma once

#include "network_platform.hpp"

namespace network::tcp {

#ifdef OS_WINDOWS
using socket_t = SOCKET;
constexpr socket_t invalid_socket = INVALID_SOCKET;
constexpr int socket_error = SOCKET_ERROR;
#else
using socket_t = int;
constexpr socket_t invalid_socket = -1;
constexpr int socket_error = -1;
#endif

} // namespace network::tcp
