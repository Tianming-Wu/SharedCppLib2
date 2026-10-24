/*
    WebSocket client (RFC 6455).

    One object is one connection. connect() resolves the host (inside the
    transport), upgrades the connection, and from then on receive() hands back
    complete messages: fragmentation, ping/pong and close are handled here.

    The transport is whatever scl2::transport_interface is given to the
    constructor -- plain TCP by default, a TLS client for wss.

    Not thread-safe: one object is driven by one thread at a time.
*/

#pragma once

#include "ws.hpp"
#include "basics.hpp"
#include "bytearray.hpp"
#include "stream.hpp"

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace network::ws {

class client {
public:
    /// @brief A client over plain TCP.
    client();

    /// @brief A client over @p transport (for example a TLS client for wss).
    /// @note Ownership of @p transport is transferred.
    explicit client(std::unique_ptr<scl2::transport_interface> transport);

    ~client();

    enable_move_only(client)

    /// @brief Connect and perform the opening handshake.
    /// @param host Host name or literal address; also what the Host header says.
    /// @param port Port to connect to.
    /// @param path Request target, query included; "/" when empty.
    /// @param timeout Time limit for resolving, connecting and the handshake.
    /// @return false on failure; lastError() says why.
    bool connect(const std::string& host, std::uint16_t port,
                 const std::string& path = "/",
                 std::chrono::milliseconds timeout = std::chrono::seconds(10));

    /// @brief Send a close frame and drop the connection. Idempotent.
    void close(std::uint16_t code = 1000, const std::string& reason = {});

    bool is_open() const;
    bool is_connected() const;

    /// @brief Send one complete text message as a single frame.
    /// @return false when the text is not valid UTF-8 or the connection is gone.
    bool send_text(const std::string& text);

    /// @brief Send one complete binary message as a single frame.
    bool send_binary(const scl2::bytearray& data);

    /// @brief Send a ping; the matching pong is consumed by receive().
    /// @note A ping payload is limited to 125 bytes.
    bool ping(const scl2::bytearray& payload = {});

    /// @brief Wait for and return one complete data message.
    /// @param timeout Negative waits forever; zero takes only what is already there.
    /// @note A ping or a close that arrives while waiting is handled here: the
    ///       caller only sees data messages, timeouts and the end of the session.
    receive_status receive(message& out,
                           std::chrono::milliseconds timeout = std::chrono::seconds(5));

    /// @brief The close code that was sent or received; 0 when there was none.
    std::uint16_t close_code() const { return m_close_code; }
    std::string close_reason() const { return m_close_reason; }

    /// @brief Why the last operation failed; empty when there is nothing to say.
    const std::string& lastError() const { return m_last_error; }

    /// @brief Largest message accepted, in bytes (default 32 MiB).
    void set_max_message_size(std::size_t bytes) { m_max_message_size = bytes; }
    std::size_t max_message_size() const { return m_max_message_size; }

    /// @brief Add a header to the opening handshake.
    void set_header(const std::string& name, const std::string& value);

    /// @brief Drop every header added with set_header().
    void clear_headers() { m_headers.clear(); }

private:
    enum class parse_result : std::uint8_t { message, closed, failed, need_more };

    bool handshake(const std::string& host, std::uint16_t port,
                   const std::string& path,
                   std::chrono::steady_clock::time_point deadline);
    parse_result parse(message& out);
    parse_result fail(std::uint16_t code, const std::string& reason);
    bool send_frame(opcode type, const scl2::bytearray& payload);
    bool write_all(const scl2::bytearray& bytes);
    bool wait_readable(std::chrono::steady_clock::time_point deadline);
    bool fill(std::chrono::steady_clock::time_point deadline, bool& timed_out);

    std::unique_ptr<scl2::transport_interface> m_transport;

    scl2::bytearray m_in;         // received, not framed yet
    scl2::bytearray m_fragment;   // a message split over several frames
    opcode m_fragment_type = opcode::text;
    bool m_fragmenting = false;

    bool m_open = false;
    bool m_close_sent = false;
    std::uint16_t m_close_code = 0;
    std::string m_close_reason;
    std::string m_last_error;

    std::size_t m_max_message_size = 32u << 20;
    std::map<std::string, std::string> m_headers;
};

} // namespace network::ws
