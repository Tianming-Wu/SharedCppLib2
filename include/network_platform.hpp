/*
    Platform module for network-related functionalities.

    We decided not to use current platform.hpp here because it causes problems.
*/

#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS
#else
    #define OS_UNIX
#endif

#ifdef OS_WINDOWS
    // Winsock2.h MUST be included before Windows.h
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib") // Link with the Winsock library

    #include <windows.h>
    #include <direct.h>
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>

    #include <unistd.h>
    #include <sys/wait.h>
#endif

#ifdef OS_WINDOWS
    using socket_t = SOCKET;
    constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
    inline int close_socket(socket_t s) { return closesocket(s); }
#else
    using socket_t = int;
    constexpr socket_t INVALID_SOCK = -1;
    inline int close_socket(socket_t s) { return close(s); }
#endif