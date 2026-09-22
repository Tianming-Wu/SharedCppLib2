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
#include <stdexcept>
#include <string>

namespace scl2::pipe {

// forward declaration
class permissions;
class server_client;
class server;
class client;

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

/// @brief The token acknowledge() sends.
/// @deprecated The kernel already reports when the peer has read what you wrote, so a token
/// in the data stream is not needed. Use waitForFinished() instead.
[[deprecated("use waitForFinished() instead")]]
inline constexpr std::byte acknowledge_token = std::byte{0xFF};


/// @brief Thrown when an operation finds the connection gone, on a connection that was told to
/// throw instead of only recording it.
///
/// Nothing here is a programming error: the peer may crash, or close its end, at any moment,
/// which is why this is off by default - a program that does not catch it dies exactly where it
/// meant to handle the failure. Turn it on per connection with setThrowOnBroken() when a
/// try/catch around the handler is the shape you want.
class connection_broken : public std::runtime_error {
public:
    explicit connection_broken(const std::string& what) : std::runtime_error(what) {}
};


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
/* It owns its handle and its buffer, and shares nothing with another one, so a handler can be
   moved to its own thread. The object itself is not thread-safe: one connection, one thread. */
class server_client : public scl2::basic_iostream {
    friend class server;
private:
    server_client(winhandle_t hpipe, size_t buffer_size, mode pipe_mode, winhandle_t cancel_event);

    disable_copy(server_client)

    // move should be implemented, since you need to set the old handle to an invalid value.
    server_client(server_client&& another);
    server_client& operator=(server_client&& another);

public:
    virtual ~server_client();

    virtual bool valid() override;

    /// Whether an operation has found this connection gone. It is recorded by every operation
    /// that can find it, including the ones that only look (available(), readyRead()), so a
    /// failed call is enough to tell a timeout from a dead peer.
    bool broken() const;

    /// Whether a failing operation should throw connection_broken instead of only recording it.
    /// Off by default.
    void setThrowOnBroken(bool enabled);
    bool throwOnBroken() const;

    /// Wake up any wait on this connection without closing it: the wait answers false, as if it
    /// had timed out. Sticky until reset(). This is the one call that is meant to be made from
    /// another thread, and it never throws. server::stop() does the same for the connections
    /// that server handed out.
    void cancel();
    bool cancelled() const;

    virtual bool readyRead() override;
    virtual bool waitForReadyRead(std::chrono::milliseconds timeout = std::chrono::seconds(5)) override;

    virtual size_t available() override;

    virtual scl2::bytearray read(size_t bytes) override;
    virtual scl2::bytearray readAll() override;

    virtual size_t write(const scl2::bytearray& data) override;

    /// Wait until the peer has read everything written so far, so that closing the pipe
    /// cannot discard anything. A negative timeout waits for as long as it takes.
    bool waitForFinished(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    [[deprecated("use waitForFinished() instead")]]
    bool acknowledge();
    [[deprecated("use waitForFinished() instead")]]
    bool waitForAcknowledged(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    bool close();

    /// The stream interface name for dropping the connection: release the handle and go back to
    /// an invalid state. Idempotent, so this answers true even when there is nothing to release.
    virtual bool reset() override;

    // This buffer size is not settable, it keeps the same as the server.
    size_t bufferSize() const;

private:
    // Adds this connection's cancel events to a wait array, for the waits in the implementation.
    // void**, because the header stays free of Windows types: a HANDLE is a void*.
    size_t appendCancelEvents(void** waits, size_t count) const;

    winhandle_t m_pipe;
    winhandle_t m_cancel_event;         // set by cancel()
    winhandle_t m_shared_cancel_event;  // copy of the server's stop event, set by server::stop()
    scl2::bytearray m_read_buffer;      // Internal read buffer
    bool m_broken = false;
    bool m_throw_on_broken = false;
    bool m_message_incomplete = false;  // the buffer holds only the start of a message
    mode m_mode;
    size_t m_buffer_size;
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

    server_client queryNextConnection();
    bool hasPendingConnection() const;

    bool waitForNextConnection(std::chrono::milliseconds timeout = std::chrono::seconds(5)) const;

    void setBufferSize(size_t size);
    size_t bufferSize() const;

    void setPermissions(const permissions& perms);
    permissions getPermissions() const;

    void setPipeMode(mode pipe_mode);
    mode getPipeMode() const;

    void setClientLimit(int limit);
    int clientLimit() const;

    // Windows caps the instances of one pipe name at 255, and queryNextConnection()
    // momentarily holds two of them during the accept-and-relisten handoff, so the usable
    // maximum is one less (CreateNamedPipe receives the limit + 1).
    constexpr static int maximumClientLimit = 254;

private:
    std::string m_name;
    permissions m_permissions;
    mode m_mode = mode::Byte;
    bool m_stopped = false;

    winhandle_t m_pipe;
    winhandle_t m_connect_event;
    winhandle_t m_stop_event;   // Event to signal server stop
    void* m_overlapped_connect; // OVERLAPPED*, kept out of the header

    int m_client_limit = maximumClientLimit; // CreateNamedPipe receives the limit + 1

    size_t m_buffer_size = 4_Ki;
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

    /// Wait until the peer has read everything written so far, so that closing the pipe
    /// cannot discard anything. A negative timeout waits for as long as it takes.
    bool waitForFinished(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    [[deprecated("use waitForFinished() instead")]]
    bool acknowledge();
    [[deprecated("use waitForFinished() instead")]]
    bool waitForAcknowledged(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    bool broken() const;
    /// Whether a failing operation should throw connection_broken instead of only recording it.
    /// Off by default.
    void setThrowOnBroken(bool enabled);
    bool throwOnBroken() const;

    /// Wake up any wait on this connection without closing it: the wait answers false, as if it
    /// had timed out. Sticky until reset(). This is the one call that is meant to be made from
    /// another thread, and it never throws.
    void cancel();
    bool cancelled() const;
    bool close();

    /// The stream interface name for dropping the connection: release the handle and go back to
    /// an invalid state. Idempotent, so this answers true even when there is nothing to release.
    virtual bool reset() override;

    // Returns the mode detected from the server after connect().
    // Can be overridden with setPipeMode() if you need MessageChunk behaviour on a Message pipe.
    mode pipeMode() const;
    void setPipeMode(mode pipe_mode);

private:
    // Adds this connection's cancel events to a wait array, for the waits in the implementation.
    // void**, because the header stays free of Windows types: a HANDLE is a void*.
    size_t appendCancelEvents(void** waits, size_t count) const;

    std::string m_name;
    winhandle_t m_pipe;
    winhandle_t m_cancel_event;         // set by cancel()
    scl2::bytearray m_read_buffer;      // Internal read buffer
    bool m_broken = false;
    bool m_throw_on_broken = false;
    bool m_message_incomplete = false;  // the buffer holds only the start of a message
    mode m_mode = mode::Byte; // updated by connect() via GetNamedPipeInfo

    size_t m_buffer_size = 4_Ki;
};


} // namespace scl2::pipe

#else   // not Windows

    #if defined(_MSC_VER)
        #pragma message("pipe: named pipes are Windows only, so nothing is defined here.")
    #else
        #warning "pipe: named pipes are Windows only, so nothing is defined here."
    #endif

#endif  // SCL2_PIPE_SUPPORTED
