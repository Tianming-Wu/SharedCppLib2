/*
    Named pipes: one server, many clients, addressed by a name instead of a port.

    classes:
        scl2::pipe::server, scl2::pipe::server_client, scl2::pipe::client,
        scl2::pipe::permissions
    link target:
        SharedCppLib2::pipe

    A pipe carries a byte stream, or messages with their boundaries preserved - the kernel
    keeps the framing, so readAll() returns exactly what one write() sent. Connections are
    accepted one at a time: queryNextConnection() hands out a connected client and listens
    for the next one.

    Windows only. On other platforms this header defines nothing and says so, and
    SCL2_PIPE_SUPPORTED is 0 there, so portable code can ask before using it.
*/

#pragma once

#if defined(OS_WINDOWS) || defined(_WIN32) || defined(_WIN64)
    #define SCL2_PIPE_SUPPORTED 1
#else
    // Unknown or non-Windows platform: stay out of the way instead of guessing.
    #define SCL2_PIPE_SUPPORTED 0
#endif


#if SCL2_PIPE_SUPPORTED

#include "basics.hpp"
#include "bytearray.hpp"
#include "engineering.hpp"
#include "stream.hpp"
#include "typemask.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace scl2::pipe {

// forward declaration
class permissions;
class server_client;
class server;
class client;

// Built-in message structures.
// The original implementation is using bytearray. You can use these to
// make sure the message format, sturcture, sizes and encoding are consistent
// between the server and client.
struct request;
struct response;

/// @brief What a pipe carries.
enum class mode {
    Byte,         // Byte mode: data written as a raw byte stream, no message boundaries
    Message,      // Message mode: each write is a discrete message, boundaries preserved on read.
                  //   read() is disabled (returns empty); use readAll() for a complete message.
    MessageChunk, // Same underlying kernel mode as Message, but read() returns partial chunks.
                  //   The caller is responsible for reassembling across multiple read() calls.
                  //   readAll() still reassembles automatically.
};

/// @brief Which security descriptor the server creates the pipe with.
enum class permission_preset {
    None,     // Do not use the preset
    Default,  // Default, which is usually Administrator.
    Everyone, // Allow everyone to access the pipe.
    SameSID,  // Allow current user to access the pipe.
};

inline constexpr std::byte acknowledge_token = std::byte{0xFF};


/// @brief The permissions a server creates its pipe with.
class permissions {
public:
    permissions();
    permissions(permission_preset preset);

protected:
    friend class server;
    void* getSecurityDescriptor() const;

private:
    permission_preset m_preset;
};


/* This is a single client. This is required since a pipe can have multiple clients. */
/* Different client handlers are thread-safe. */
class server_client : public scl2::basic_iostream {
    friend class server;
private:
    server_client(winhandle_t hpipe, size_t buffer_size, mode pipe_mode);

    disable_copy(server_client)

    // move should be implemented, since you need to set the old handle to an invalid value.
    server_client(server_client&& another);
    server_client& operator=(server_client&& another);

public:
    virtual ~server_client();

    virtual bool valid() override;
    bool broken() const;

    virtual bool readyRead() override;
    virtual bool waitForReadyRead(std::chrono::milliseconds timeout = std::chrono::seconds(5)) override;

    virtual size_t available() override;

    virtual scl2::bytearray read(size_t bytes) override;
    virtual scl2::bytearray readAll() override;

    virtual size_t write(const scl2::bytearray& data) override;

    bool acknowledge();
    bool waitForAcknowledged(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    bool close();
    bool cleanup();

    // This buffer size is not settable, it keeps the same as the server.
    size_t bufferSize() const;

private:
    winhandle_t m_pipe;
    scl2::bytearray m_read_buffer;  // Internal read buffer
    bool m_broken = false;
    mode m_mode;
    size_t buffer_size;
};



/// @brief The listening end of a named pipe.
class server {
public:
    server(const std::string& name, const permissions& perms = permissions(permission_preset::Default));
    ~server();

    bool start();
    bool stop();
    bool cleanup();

    bool active() const;
    bool stopped() const;

    int clientCount() const;

    server_client queryNextConnection();
    bool hasPendingConnection() const;

    bool waitForNextConnection(std::chrono::milliseconds timeout = std::chrono::seconds(5)) const;

    void setBufferSize(size_t size);
    size_t bufferSize() const;

    void setPermissions(const permissions& perms);
    permissions getPermissions() const;

    void setPipeMode(mode pipe_mode);
    mode getPipeMode() const;

    void setMaxClients(int maxClients);
    int getMaxClients() const;

    // Windows hard limit on named pipe instances is 255, but queryNextConnection()
    // momentarily holds two simultaneous instances during accept-and-relisten handoff,
    // so the user-visible maximum is capped at 254 (CreateNamedPipe receives max_clients+1).
    constexpr static int unlimitedClients = 254;

private:
    std::string m_name;
    permissions m_permissions;
    mode m_mode = mode::Byte;
    bool m_stopped = false;

    winhandle_t m_pipe;
    winhandle_t m_completion_port;
    winhandle_t m_connect_event;
    winhandle_t m_stop_event;   // Event to signal server stop
    void* m_overlapped_connect; // OVERLAPPED*, kept out of the header

    std::vector<server_client> m_clients;
    int max_clients = unlimitedClients; // CreateNamedPipe receives max_clients+1

    size_t buffer_size = 4_Ki;
};



/// @brief The client end of a named pipe.
class client : public scl2::basic_iostream {
public:
    client(const std::string& name);
    ~client();

    disable_copy(client)

    client(client&& another);
    client& operator=(client&& another);

    bool connect(std::chrono::milliseconds timeout = std::chrono::seconds(5));
    bool waitForConnection(std::chrono::milliseconds timeout = std::chrono::seconds(5));
    bool serverExists() const;

    virtual bool valid() override;

    virtual bool readyRead() override;
    virtual bool waitForReadyRead(std::chrono::milliseconds timeout = std::chrono::seconds(5)) override;

    virtual size_t available() override;

    virtual scl2::bytearray read(size_t bytes) override;
    virtual scl2::bytearray readAll() override;

    virtual size_t write(const scl2::bytearray& data) override;

    bool acknowledge();
    bool waitForAcknowledged(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    bool broken() const;
    bool close();
    bool cleanup();

    // Returns the mode detected from the server after connect().
    // Can be overridden with setPipeMode() if you need MessageChunk behaviour on a Message pipe.
    mode pipeMode() const;
    void setPipeMode(mode pipe_mode);

private:
    std::string m_name;
    winhandle_t m_pipe;
    scl2::bytearray m_read_buffer;  // Internal read buffer
    bool m_broken = false;
    mode m_mode = mode::Byte; // updated by connect() via GetNamedPipeInfo

    size_t buffer_size = 4_Ki;
};


} // namespace scl2::pipe

#else   // not Windows

    #if defined(_MSC_VER)
        #pragma message("pipe: named pipes are Windows only, so nothing is defined here.")
    #else
        #warning "pipe: named pipes are Windows only, so nothing is defined here."
    #endif

#endif  // SCL2_PIPE_SUPPORTED
