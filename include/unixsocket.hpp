/*
    AF_UNIX (local domain) sockets for POSIX systems.

    classes:
        scl2::posix::unix_stream, scl2::posix::unix_server
    link target:
        SharedCppLib2::unixsocket

    A local socket is addressed by a name instead of an address and a port, so there is none
    of the address handling the IP modules have: a path is enough. A name that starts with
    '@' uses the abstract namespace (Linux and Android): it has no file behind it, so it needs
    no unlink, no permissions, and it disappears when the process does.

    `unix_stream` implements scl2::transport_interface, so the protocol layers that take one
    (the HTTP client, for example) work over a local socket unchanged.

    POSIX only. On Windows, and on anything the check below does not recognise, this header
    defines nothing and warns; SCL2_UNIXSOCKET_SUPPORTED is 0 there, so portable code can
    ask before using it.
*/

#pragma once

#if defined(OS_WINDOWS) || defined(_WIN32) || defined(_WIN64)
    #define SCL2_UNIXSOCKET_SUPPORTED 0
#elif defined(OS_UNIX) || defined(__unix__) || defined(__unix) || defined(__linux__) || \
      defined(__ANDROID__) || defined(__APPLE__) || defined(__FreeBSD__) || \
      defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    #define SCL2_UNIXSOCKET_SUPPORTED 1
#else
    // Unknown platform: stay out of the way instead of guessing.
    #define SCL2_UNIXSOCKET_SUPPORTED 0
#endif


#if SCL2_UNIXSOCKET_SUPPORTED

#include "basics.hpp"
#include "bytearray.hpp"
#include "stream.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

namespace scl2::posix {

namespace detail {

/// @brief `strerror` text for a system error code (0 means "the last call succeeded").
inline std::string error_text(int err)
{
    if (err == 0) return "no error";
    return std::strerror(err);
}

/// @brief Whether the name uses the abstract namespace, which has no file behind it.
inline bool is_abstract(const std::string& name) noexcept
{
    return !name.empty() && (name[0] == '@' || name[0] == '\0');
}

/// @brief Fill a sockaddr_un from a path, or from "@name" for the abstract namespace.
/// @return false when the name does not fit in sun_path.
/// @note The name is copied without the '@', since the leading '\0' in sun_path is what
///       marks the abstract namespace. A name that already starts with '\0' means the same.
inline bool make_address(const std::string& name, sockaddr_un& addr, socklen_t& addr_len)
{
    const size_t marker = (!name.empty() && (name[0] == '@' || name[0] == '\0')) ? 1 : 0;
    const char* text = name.c_str() + marker;
    const size_t text_len = name.size() - marker;

    if (text_len + 1 > sizeof(addr.sun_path)) return false;

    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    if (marker == 1) {
        // sun_path[0] stays '\0', and the name follows it.
        std::memcpy(addr.sun_path + 1, text, text_len);
        addr_len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1 + text_len);
    }
    else {
        std::memcpy(addr.sun_path, text, text_len);
        addr_len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + text_len + 1);
    }
    return true;
}

inline void set_close_on_exec(int fd) noexcept
{
    const int flags = ::fcntl(fd, F_GETFD);
    if (flags >= 0) ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

inline void set_nonblocking(int fd) noexcept
{
    const int flags = ::fcntl(fd, F_GETFL);
    if (flags >= 0) ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/// @brief Keep a write to a closed peer from killing the process with SIGPIPE.
inline void set_no_sigpipe(int fd) noexcept
{
#ifdef SO_NOSIGPIPE
    const int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#else
    (void)fd;   // Linux and Android: send() carries MSG_NOSIGNAL instead
#endif
}

#ifdef MSG_NOSIGNAL
constexpr int send_flags = MSG_NOSIGNAL;
#else
constexpr int send_flags = 0;
#endif

/// @brief Whether something is listening at this address right now.
/// @note Used before removing a socket file that is already at the path: a file left behind
///       by a dead process can go, a live server's must not.
inline bool socket_is_live(const sockaddr_un& addr, socklen_t addr_len) noexcept
{
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;

    set_nonblocking(fd);
    const bool live = ::connect(fd, reinterpret_cast<const sockaddr*>(&addr), addr_len) == 0;
    ::close(fd);
    return live;
}

} // namespace detail


/// @brief A connected AF_UNIX stream socket.
///
/// It is a scl2::transport_interface, so it can be handed to anything that takes a transport.
///
/// @code
///   scl2::posix::unix_stream s;
///   if (!s.connect("/run/app.sock")) return;   // "@app" for the abstract namespace
///   s.write(scl2::bytearray("ping"));
///   if (s.waitForReadyRead(std::chrono::seconds(1))) auto reply = s.readAll();
/// @endcode
class unix_stream : public scl2::transport_interface
{
public:
    unix_stream() = default;

    /// @brief Take over an already connected descriptor. The stream closes it in its destructor.
    explicit unix_stream(int fd) noexcept : m_fd(fd) {}

    ~unix_stream() override { disconnect(); }

    // The descriptor moves, so the source has to end up without it - the defaulted move
    // would copy the number and leave both objects closing the same socket.
    unix_stream(unix_stream&& other) noexcept : m_fd(other.m_fd), m_error(other.m_error)
    {
        other.m_fd = -1;
    }

    unix_stream& operator=(unix_stream&& other) noexcept
    {
        if (this != &other) {
            disconnect();
            m_fd = other.m_fd;
            m_error = other.m_error;
            other.m_fd = -1;
        }
        return *this;
    }

    unix_stream(const unix_stream&) = delete;
    unix_stream& operator=(const unix_stream&) = delete;

    // ---- Connecting ------------------------------------------------

    /// @brief Connect to a local socket.
    /// @param name A path, or "@name" for the abstract namespace (Linux / Android).
    /// @return true on success; `lastError()` holds the system error code when it is false.
    bool connect(const std::string& name)
    {
        disconnect();

        sockaddr_un addr{};
        socklen_t addr_len = 0;
        if (!detail::make_address(name, addr, addr_len)) {
            m_error = ENAMETOOLONG;
            return false;
        }

        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            m_error = errno;
            return false;
        }

        detail::set_close_on_exec(fd);
        detail::set_no_sigpipe(fd);

        if (::connect(fd, reinterpret_cast<const sockaddr*>(&addr), addr_len) != 0) {
            m_error = errno;
            ::close(fd);
            return false;
        }

        m_fd = fd;
        m_error = 0;
        return true;
    }

    /// @note transport_interface takes a host and a port. A local socket has no port, so the
    ///       name goes in `address` and `port` is not used; the single-argument connect()
    ///       says the same thing without the extra argument.
    bool connect(const std::string& address, uint16_t) override { return connect(address); }

    /// @brief Shut the connection down and close the descriptor. Does nothing when there is none.
    void disconnect() override
    {
        if (m_fd >= 0) {
            ::shutdown(m_fd, SHUT_RDWR);
            ::close(m_fd);
            m_fd = -1;
        }
        m_error = 0;
    }

    /// @brief Whether a descriptor is held. A closed peer is not detected.
    bool is_connected() const override { return m_fd >= 0; }

    // ---- Stream interface ------------------------------------------

    bool valid() override { return m_fd >= 0; }

    /// @brief Whether there is something to read now: data, or a closed peer.
    bool readyRead() override
    {
        if (m_fd < 0) return false;

        pollfd p{ m_fd, POLLIN, 0 };
        if (::poll(&p, 1, 0) <= 0) return false;
        return (p.revents & (POLLIN | POLLHUP | POLLERR)) != 0;
    }

    /// @brief Wait until there is something to read.
    /// @return false when the timeout runs out, or the stream is not connected.
    bool waitForReadyRead(std::chrono::milliseconds timeout = std::chrono::seconds(5)) override
    {
        if (m_fd < 0) return false;

        pollfd p{ m_fd, POLLIN, 0 };
        if (::poll(&p, 1, static_cast<int>(timeout.count())) <= 0) return false;
        return (p.revents & (POLLIN | POLLHUP | POLLERR)) != 0;
    }

    /// @brief How many bytes are waiting to be read.
    size_t available() override
    {
        if (m_fd < 0) return 0;

        int bytes = 0;
        if (::ioctl(m_fd, FIONREAD, &bytes) != 0) {
            m_error = errno;
            return 0;
        }
        return bytes > 0 ? static_cast<size_t>(bytes) : 0;
    }

    /// @brief Read up to `bytes` bytes, and return what arrived.
    /// @return Empty when nothing was read, which is not an error: see `lastError()`.
    scl2::bytearray read(size_t bytes) override
    {
        if (m_fd < 0 || bytes == 0) return {};

        scl2::bytearray buffer(bytes);
        const ssize_t received = ::recv(m_fd, buffer.data(), bytes, 0);
        if (received <= 0) {
            m_error = received == 0 ? 0 : errno;   // 0 means the peer closed its end
            return {};
        }
        buffer.resize(static_cast<size_t>(received));
        return buffer;
    }

    /// @brief Read everything that is available right now.
    scl2::bytearray readAll() override { return read(available()); }

    /// @brief Send `data`.
    /// @return The number of bytes sent, 0 when nothing was sent.
    size_t write(const scl2::bytearray& data) override
    {
        if (m_fd < 0 || data.empty()) return 0;

        const ssize_t sent = ::send(m_fd, data.data(), data.size(), detail::send_flags);
        if (sent <= 0) {
            m_error = errno;
            return 0;
        }
        return static_cast<size_t>(sent);
    }

    /// @brief Drop the connection, and answer whether the stream is usable afterwards.
    /// @note Idempotent, as the stream contract asks.
    bool reset() override
    {
        disconnect();
        return true;
    }

    // ---- Diagnostics -----------------------------------------------

    /// @brief The descriptor, or -1. For `poll()` / `select()` on the caller's side.
    int native_handle() const noexcept { return m_fd; }

    /// @brief The system error code of the last failed call, 0 after a successful one.
    int lastError() const noexcept { return m_error; }

private:
    int m_fd = -1;
    int m_error = 0;
};


/// @brief A listening AF_UNIX socket: the server side of a local socket.
///
/// @code
///   scl2::posix::unix_server server;
///   if (!server.listen("/run/app.sock", 8, 0660)) return;
///
///   while (auto client = server.accept(std::chrono::seconds(1))) {
///       client->write(scl2::bytearray("hello"));
///   }
/// @endcode
class unix_server
{
public:
    unix_server() = default;
    ~unix_server() { close(); }

    // The descriptor moves, so the source has to end up without it - and the file it
    // created stays owned by whoever ends up holding the descriptor.
    unix_server(unix_server&& other) noexcept
        : m_fd(other.m_fd), m_error(other.m_error), m_name(std::move(other.m_name)),
          m_owns_file(other.m_owns_file)
    {
        other.m_fd = -1;
        other.m_owns_file = false;
    }

    unix_server& operator=(unix_server&& other) noexcept
    {
        if (this != &other) {
            close();
            m_fd = other.m_fd;
            m_error = other.m_error;
            m_name = std::move(other.m_name);
            m_owns_file = other.m_owns_file;
            other.m_fd = -1;
            other.m_owns_file = false;
        }
        return *this;
    }

    unix_server(const unix_server&) = delete;
    unix_server& operator=(const unix_server&) = delete;

    // ---- Listening -------------------------------------------------

    /// @brief Create the socket, and bind and listen at `name`.
    /// @param name A path, or "@name" for the abstract namespace (Linux / Android).
    /// @param backlog How many connections the kernel keeps waiting; 8 when not given.
    /// @param mode Permissions of the socket file, 0 to leave the umask in charge. An Android
    ///        service usually wants 0660, so that only its group can connect. Not used for an
    ///        abstract name, which has no file.
    /// @return true on success; `lastError()` holds the system error code when it is false.
    /// @note A socket file left at the path by a dead process is removed first. One that is
    ///       still answering is not, and that reports EADDRINUSE.
    bool listen(const std::string& name, int backlog = 8, mode_t mode = 0)
    {
        close();

        sockaddr_un addr{};
        socklen_t addr_len = 0;
        if (!detail::make_address(name, addr, addr_len)) {
            m_error = ENAMETOOLONG;
            return false;
        }

        const bool abstract = detail::is_abstract(name);

        if (!abstract && ::access(name.c_str(), F_OK) == 0) {
            if (detail::socket_is_live(addr, addr_len)) {
                m_error = EADDRINUSE;
                return false;
            }
            if (::unlink(name.c_str()) != 0) {
                m_error = errno;
                return false;
            }
        }

        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            m_error = errno;
            return false;
        }

        detail::set_close_on_exec(fd);
        detail::set_no_sigpipe(fd);

        if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), addr_len) != 0) {
            m_error = errno;
            ::close(fd);
            return false;
        }

        if (::listen(fd, backlog) != 0) {
            m_error = errno;
            ::close(fd);
            if (!abstract) ::unlink(name.c_str());
            return false;
        }

        if (!abstract && mode != 0 && ::chmod(name.c_str(), mode) != 0) {
            m_error = errno;
            ::close(fd);
            ::unlink(name.c_str());
            return false;
        }

        // The waiting is done with poll(), so accept() must not block on its own.
        detail::set_nonblocking(fd);

        m_fd = fd;
        m_name = name;
        m_owns_file = !abstract;
        m_error = 0;
        return true;
    }

    /// @brief Stop listening, and remove the socket file when we are the one who made it.
    /// @note Only a socket is removed: if something else has taken the path since, it stays.
    void close()
    {
        if (m_fd >= 0) {
            ::close(m_fd);
            m_fd = -1;
        }

        if (m_owns_file && !m_name.empty()) {
            struct stat info{};
            if (::lstat(m_name.c_str(), &info) == 0 && S_ISSOCK(info.st_mode))
                ::unlink(m_name.c_str());
        }

        m_owns_file = false;
        m_name.clear();
    }

    bool is_listening() const noexcept { return m_fd >= 0; }

    // ---- Accepting -------------------------------------------------

    /// @brief Accept one connection, waiting at most `timeout`.
    /// @param timeout How long to wait for a client. 0 does not wait; `tryAccept()` is the
    ///        clearer spelling of the same thing.
    /// @return The connected stream, or nullptr on timeout (lastError() is ETIMEDOUT) and on
    ///         failure (lastError() holds the system error code).
    std::unique_ptr<unix_stream> accept(std::chrono::milliseconds timeout = std::chrono::seconds(5))
    {
        if (m_fd < 0) {
            m_error = EBADF;
            return nullptr;
        }

        if (timeout.count() > 0) {
            pollfd p{ m_fd, POLLIN, 0 };
            const int ready = ::poll(&p, 1, static_cast<int>(timeout.count()));
            if (ready == 0) {
                m_error = ETIMEDOUT;
                return nullptr;
            }
            if (ready < 0) {
                m_error = errno;
                return nullptr;
            }
        }

        for (;;) {
            const int fd = ::accept(m_fd, nullptr, nullptr);
            if (fd >= 0) {
                detail::set_close_on_exec(fd);
                detail::set_no_sigpipe(fd);
                m_error = 0;
                return std::make_unique<unix_stream>(fd);
            }

            if (errno == EINTR) continue;   // a signal arrived, not a failure
            m_error = (errno == EAGAIN || errno == EWOULDBLOCK) ? ETIMEDOUT : errno;
            return nullptr;
        }
    }

    /// @brief Accept one connection if one is already waiting.
    std::unique_ptr<unix_stream> tryAccept()
    {
        return accept(std::chrono::milliseconds::zero());
    }

    // ---- Diagnostics -----------------------------------------------

    /// @brief The name this socket was opened with, empty when it is not listening.
    const std::string& name() const noexcept { return m_name; }

    /// @brief The listening descriptor, or -1. For `poll()` / `select()` on the caller's side.
    int native_handle() const noexcept { return m_fd; }

    /// @brief The system error code of the last failed call, 0 after a successful one.
    int lastError() const noexcept { return m_error; }

private:
    int m_fd = -1;
    int m_error = 0;
    std::string m_name;
    bool m_owns_file = false;
};

/// @brief The text of a system error code, as `lastError()` reports it.
inline std::string errorText(int err) { return detail::error_text(err); }

} // namespace scl2::posix

#else   // not POSIX

    #if defined(_MSC_VER)
        #pragma message("unixsocket: AF_UNIX sockets are POSIX only, so nothing is defined here.")
    #else
        #warning "unixsocket: AF_UNIX sockets are POSIX only, so nothing is defined here."
    #endif

#endif  // SCL2_UNIXSOCKET_SUPPORTED
