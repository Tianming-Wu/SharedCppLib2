#include "wsclient.hpp"

#include "sha1.hpp"
#include "tcpclient.hpp"

#include <algorithm>
#include <cstring>
#include <random>
#include <string>
#include <utility>

namespace network::ws {

namespace {

using clock_type = std::chrono::steady_clock;

// RFC 6455, section 1.3: appended to the client's key before hashing.
constexpr const char* kAcceptGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// A handshake response is a few hundred bytes; anything this big is not one.
constexpr std::size_t kMaxHandshakeBytes = 64u << 10;

std::string lower(std::string text)
{
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return text;
}

std::string trim(const std::string& text)
{
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

/// @brief Whether a comma separated header value carries @p token.
bool has_token(const std::string& value, const std::string& token)
{
    std::size_t at = 0;
    while (at <= value.size()) {
        const std::size_t comma = value.find(',', at);
        const std::string one =
            trim(value.substr(at, comma == std::string::npos ? std::string::npos : comma - at));
        if (lower(one) == token) return true;
        if (comma == std::string::npos) break;
        at = comma + 1;
    }
    return false;
}

scl2::bytearray random_bytes(std::size_t count)
{
    static thread_local std::mt19937 engine{ std::random_device{}() };
    std::uniform_int_distribution<int> octet(0, 255);
    scl2::bytearray raw(count);
    for (std::size_t i = 0; i < count; ++i) {
        raw[i] = std::byte(octet(engine));
    }
    return raw;
}

/// @brief Whether @p size bytes at @p bytes are one well-formed UTF-8 sequence stream.
bool valid_utf8(const std::byte* bytes, std::size_t size)
{
    std::size_t i = 0;
    while (i < size) {
        const std::uint8_t lead = static_cast<std::uint8_t>(bytes[i]);
        if (lead < 0x80) {
            ++i;
            continue;
        }

        std::size_t extra = 0;
        std::uint32_t code = 0;
        if ((lead & 0xE0) == 0xC0) {
            extra = 1;
            code = lead & 0x1F;
        } else if ((lead & 0xF0) == 0xE0) {
            extra = 2;
            code = lead & 0x0F;
        } else if ((lead & 0xF8) == 0xF0) {
            extra = 3;
            code = lead & 0x07;
        } else {
            return false;
        }
        if (size - i <= extra) return false;

        for (std::size_t k = 1; k <= extra; ++k) {
            const std::uint8_t next = static_cast<std::uint8_t>(bytes[i + k]);
            if ((next & 0xC0) != 0x80) return false;
            code = (code << 6) | (next & 0x3F);
        }

        if ((extra == 1 && code < 0x80) || (extra == 2 && code < 0x800) ||
            (extra == 3 && code < 0x10000)) {
            return false;   // overlong
        }
        if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) return false;
        i += extra + 1;
    }
    return true;
}

bool is_control(opcode type)
{
    return (static_cast<std::uint8_t>(type) & 0x08) != 0;
}

} // namespace

client::client()
    : m_transport(std::make_unique<network::tcp::client>())
{
}

client::client(std::unique_ptr<scl2::transport_interface> transport)
    : m_transport(std::move(transport))
{
}

client::~client()
{
    close();
}

bool client::connect(const std::string& host, std::uint16_t port, const std::string& path,
                     std::chrono::milliseconds timeout)
{
    close();
    m_last_error.clear();
    m_close_code = 0;
    m_close_reason.clear();
    m_close_sent = false;

    if (!m_transport) {
        m_last_error = "there is no transport to connect with";
        return false;
    }

    const clock_type::time_point deadline = timeout.count() < 0
        ? (clock_type::time_point::max)()
        : clock_type::now() + timeout;

    if (!m_transport->connect(host, port, timeout)) {
        m_last_error = "could not connect to " + host + ":" + std::to_string(port);
        m_transport->disconnect();
        return false;
    }
    if (!handshake(host, port, path, deadline)) {
        m_transport->disconnect();
        return false;
    }

    m_open = true;
    return true;
}

void client::close(std::uint16_t code, const std::string& reason)
{
    if (m_transport && m_open && !m_close_sent && m_transport->is_connected()) {
        const std::string text = reason.substr(0, 123);   // control frame limit is 125 bytes
        scl2::bytearray payload;
        payload.appendByte(static_cast<std::uint8_t>(code >> 8));
        payload.appendByte(static_cast<std::uint8_t>(code & 0xFF));
        payload.appendRawString(text);

        m_close_sent = true;
        send_frame(opcode::close, payload);
        m_close_code = code;
        m_close_reason = text;
    }

    m_open = false;
    m_fragmenting = false;
    m_fragment.clear();
    m_in.clear();
    if (m_transport) m_transport->disconnect();
}

bool client::is_open() const
{
    return m_open && is_connected();
}

bool client::is_connected() const
{
    return m_transport && m_transport->is_connected();
}

bool client::send_text(const std::string& text)
{
    if (!m_open) {
        m_last_error = "the connection is not open";
        return false;
    }
    if (!valid_utf8(reinterpret_cast<const std::byte*>(text.data()), text.size())) {
        m_last_error = "the text is not valid UTF-8";
        return false;
    }
    return send_frame(opcode::text, scl2::bytearray(text));
}

bool client::send_binary(const scl2::bytearray& data)
{
    if (!m_open) {
        m_last_error = "the connection is not open";
        return false;
    }
    return send_frame(opcode::binary, data);
}

bool client::ping(const scl2::bytearray& payload)
{
    if (!m_open) {
        m_last_error = "the connection is not open";
        return false;
    }
    if (payload.size() > 125) {
        m_last_error = "a ping payload is limited to 125 bytes";
        return false;
    }
    return send_frame(opcode::ping, payload);
}

receive_status client::receive(message& out, std::chrono::milliseconds timeout)
{
    if (!m_open) {
        if (m_last_error.empty()) m_last_error = "the connection is not open";
        return receive_status::closed;
    }

    const clock_type::time_point deadline = timeout.count() < 0
        ? (clock_type::time_point::max)()
        : clock_type::now() + timeout;

    for (;;) {
        switch (parse(out)) {
        case parse_result::message:
            return receive_status::message;
        case parse_result::closed:
            return receive_status::closed;
        case parse_result::failed:
            return receive_status::failed;
        case parse_result::need_more:
            break;
        }

        bool timed_out = false;
        if (fill(deadline, timed_out)) continue;

        if (!timed_out) return receive_status::closed;
        if (!is_connected()) {
            if (m_last_error.empty()) m_last_error = "the connection was closed";
            return receive_status::closed;
        }
        return receive_status::timeout;
    }
}

void client::set_header(const std::string& name, const std::string& value)
{
    m_headers[name] = value;
}

bool client::handshake(const std::string& host, std::uint16_t port, const std::string& path,
                       clock_type::time_point deadline)
{
    const std::string key = random_bytes(16).toBase64();

    std::string target = path.empty() ? "/" : path;
    if (target.front() != '/') target.insert(target.begin(), '/');

    std::string host_header = host;
    if (host_header.find(':') != std::string::npos && host_header.front() != '[') {
        host_header = "[" + host_header + "]";
    }
    host_header += ":" + std::to_string(port);

    std::string request;
    request.reserve(256);
    request += "GET " + target + " HTTP/1.1\r\n";
    request += "Host: " + host_header + "\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: " + key + "\r\n";
    request += "Sec-WebSocket-Version: 13\r\n";
    for (const auto& [name, value] : m_headers) {
        request += name + ": " + value + "\r\n";
    }
    request += "\r\n";

    if (!write_all(scl2::bytearray(request))) return false;

    // The response up to the blank line. Whatever follows it is already the
    // first frame, so it is kept rather than read again.
    std::string head;
    while (head.find("\r\n\r\n") == std::string::npos) {
        if (!wait_readable(deadline)) {
            m_last_error = "the handshake timed out";
            return false;
        }

        std::size_t bytes = m_transport->available();
        if (bytes == 0) bytes = 4096;   // a TLS transport reports only decrypted bytes
        bytes = bytes < std::size_t(4096) ? bytes : std::size_t(4096);
        const scl2::bytearray chunk = m_transport->read(bytes);
        if (chunk.empty()) {
            m_last_error = "the connection closed during the handshake";
            return false;
        }
        head.append(chunk.toStdString());
        const std::size_t blank = head.find("\r\n\r\n");
        if ((blank == std::string::npos && head.size() > kMaxHandshakeBytes) ||
            (blank != std::string::npos && blank + 4 > kMaxHandshakeBytes)) {
            m_last_error = "the handshake response is too long";
            return false;
        }
    }

    const std::size_t blank = head.find("\r\n\r\n");
    const std::size_t line_end = head.find("\r\n");
    if (line_end == std::string::npos || line_end > blank) {
        m_last_error = "the handshake response has no status line";
        return false;
    }

    const std::string status = head.substr(0, line_end);
    if (status.rfind("HTTP/", 0) != 0) {
        m_last_error = "the server did not answer with HTTP";
        return false;
    }
    const std::size_t space = status.find(' ');
    if (space == std::string::npos || status.compare(space + 1, 3, "101") != 0) {
        m_last_error = "the server answered \"" + status + "\" instead of switching protocols";
        return false;
    }

    std::map<std::string, std::string> fields;
    std::size_t at = line_end + 2;
    while (at < blank) {
        const std::size_t end = head.find("\r\n", at);
        const std::size_t stop = (end == std::string::npos || end > blank) ? blank : end;
        const std::string line = head.substr(at, stop - at);
        at = stop + 2;

        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        fields[lower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
    }

    const auto field = [&fields](const char* name) {
        const auto it = fields.find(name);
        return it == fields.end() ? std::string() : it->second;
    };

    if (!has_token(field("upgrade"), "websocket")) {
        m_last_error = "the server did not upgrade the connection to websocket";
        return false;
    }
    if (!has_token(field("connection"), "upgrade")) {
        m_last_error = "the server's Connection header does not confirm the upgrade";
        return false;
    }
    const std::string accept =
        scl2::sha1::hash(scl2::bytearray(key + kAcceptGuid)).toBase64();
    if (field("sec-websocket-accept") != accept) {
        m_last_error = "the server's Sec-WebSocket-Accept does not match the key";
        return false;
    }
    if (!field("sec-websocket-extensions").empty()) {
        m_last_error = "the server selected an extension that was not requested";
        return false;
    }
    if (!field("sec-websocket-protocol").empty()) {
        m_last_error = "the server selected a subprotocol that was not requested";
        return false;
    }

    m_in.clear();
    if (head.size() > blank + 4) {
        m_in.append(reinterpret_cast<const std::byte*>(head.data() + blank + 4),
                    head.size() - blank - 4);
    }
    return true;
}

client::parse_result client::parse(message& out)
{
    for (;;) {
        if (m_in.size() < 2) return parse_result::need_more;

        const std::uint8_t first = static_cast<std::uint8_t>(m_in[0]);
        const std::uint8_t second = static_cast<std::uint8_t>(m_in[1]);

        if ((first & 0x70) != 0) {
            return fail(static_cast<std::uint16_t>(close_code::protocol_error),
                        "the peer set reserved frame bits");
        }
        if ((second & 0x80) != 0) {
            return fail(static_cast<std::uint16_t>(close_code::protocol_error),
                        "the peer sent a masked frame");
        }

        const bool final = (first & 0x80) != 0;
        const opcode type = static_cast<opcode>(first & 0x0F);
        std::uint64_t size = second & 0x7F;
        std::size_t offset = 2;

        if (size == 126) {
            if (m_in.size() < 4) return parse_result::need_more;
            size = (static_cast<std::uint64_t>(static_cast<std::uint8_t>(m_in[2])) << 8) |
                   static_cast<std::uint8_t>(m_in[3]);
            offset = 4;
        } else if (size == 127) {
            if (m_in.size() < 10) return parse_result::need_more;
            size = 0;
            for (std::size_t i = 0; i < 8; ++i) {
                size = (size << 8) | static_cast<std::uint8_t>(m_in[2 + i]);
            }
            if ((size & (std::uint64_t(1) << 63)) != 0) {
                return fail(static_cast<std::uint16_t>(close_code::protocol_error),
                            "the peer sent an invalid frame length");
            }
            offset = 10;
        }

        if (is_control(type)) {
            if (!final) {
                return fail(static_cast<std::uint16_t>(close_code::protocol_error),
                            "the peer fragmented a control frame");
            }
            if (size > 125) {
                return fail(static_cast<std::uint16_t>(close_code::protocol_error),
                            "the peer sent a control frame larger than 125 bytes");
            }
        } else if (size > m_max_message_size ||
                   m_fragment.size() + static_cast<std::size_t>(size) > m_max_message_size) {
            return fail(static_cast<std::uint16_t>(close_code::message_too_big),
                        "the peer sent a message larger than the limit");
        }

        if (size > m_in.size() || offset > m_in.size() - static_cast<std::size_t>(size)) {
            return parse_result::need_more;
        }

        scl2::bytearray payload;
        if (size != 0) {
            payload = scl2::bytearray(m_in.data() + offset, static_cast<std::size_t>(size));
        }
        m_in.erase(0, offset + static_cast<std::size_t>(size));

        switch (type) {
        case opcode::text:
        case opcode::binary: {
            if (m_fragmenting) {
                return fail(static_cast<std::uint16_t>(close_code::protocol_error),
                            "a new data frame started inside a fragmented message");
            }
            if (!final) {
                m_fragment = payload;
                m_fragment_type = type;
                m_fragmenting = true;
                break;
            }
            if (type == opcode::text && !valid_utf8(payload.data(), payload.size())) {
                return fail(static_cast<std::uint16_t>(close_code::invalid_payload),
                            "the peer sent text that is not valid UTF-8");
            }
            out.type = type;
            out.payload = payload;
            return parse_result::message;
        }

        case opcode::continuation: {
            if (!m_fragmenting) {
                return fail(static_cast<std::uint16_t>(close_code::protocol_error),
                            "the peer sent a continuation frame without a message");
            }
            m_fragment.append(payload);
            if (!final) break;

            if (m_fragment_type == opcode::text &&
                !valid_utf8(m_fragment.data(), m_fragment.size())) {
                return fail(static_cast<std::uint16_t>(close_code::invalid_payload),
                            "the peer sent text that is not valid UTF-8");
            }
            out.type = m_fragment_type;
            out.payload = std::move(m_fragment);
            m_fragment.clear();
            m_fragmenting = false;
            return parse_result::message;
        }

        case opcode::ping:
            if (!send_frame(opcode::pong, payload)) return parse_result::failed;
            break;

        case opcode::pong:
            break;

        case opcode::close: {
            if (payload.size() == 1) {
                return fail(static_cast<std::uint16_t>(close_code::protocol_error),
                            "the peer sent a malformed close frame");
            }
            if (payload.size() >= 2) {
                m_close_code = static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(static_cast<std::uint8_t>(payload[0])) << 8) |
                    static_cast<std::uint8_t>(payload[1]));
                m_close_reason.assign(reinterpret_cast<const char*>(payload.data()) + 2,
                                      payload.size() - 2);
            }
            if (!m_close_sent) {
                m_close_sent = true;
                send_frame(opcode::close, payload);   // echo the status and the reason
            }
            m_open = false;
            m_in.clear();
            if (m_transport) m_transport->disconnect();
            return parse_result::closed;
        }

        default:
            return fail(static_cast<std::uint16_t>(close_code::protocol_error),
                        "the peer sent a frame with an unknown opcode");
        }
    }
}

client::parse_result client::fail(std::uint16_t code, const std::string& reason)
{
    const std::string saved = reason;
    if (m_open) close(code, reason);
    else if (m_transport) m_transport->disconnect();
    m_last_error = saved;
    return parse_result::failed;
}

bool client::send_frame(opcode type, const scl2::bytearray& payload)
{
    if (!m_transport || !m_transport->is_connected()) {
        if (m_last_error.empty()) m_last_error = "the connection is closed";
        m_open = false;
        return false;
    }

    const std::uint64_t size = payload.size();
    scl2::bytearray frame;
    frame.reserve(payload.size() + 14);
    frame.appendByte(static_cast<std::uint8_t>(0x80 | static_cast<std::uint8_t>(type)));
    if (size < 126) {
        frame.appendByte(static_cast<std::uint8_t>(0x80 | size));
    } else if (size <= 0xFFFF) {
        frame.appendByte(static_cast<std::uint8_t>(0x80 | 126));
        frame.appendByte(static_cast<std::uint8_t>(size >> 8));
        frame.appendByte(static_cast<std::uint8_t>(size));
    } else {
        frame.appendByte(static_cast<std::uint8_t>(0x80 | 127));
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.appendByte(static_cast<std::uint8_t>(size >> shift));
        }
    }

    // A client always masks, with a key taken from the random generator and
    // not derived from the handshake key.
    const scl2::bytearray mask = random_bytes(4);
    frame.append(mask);
    for (std::size_t i = 0; i < payload.size(); ++i) {
        frame.appendByte(static_cast<std::uint8_t>(payload[i]) ^
                         static_cast<std::uint8_t>(mask[i % 4]));
    }

    return write_all(frame);
}

bool client::write_all(const scl2::bytearray& bytes)
{
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const scl2::bytearray part(bytes.data() + sent, bytes.size() - sent);
        const std::size_t written = m_transport->write(part);
        if (written == 0) {
            m_last_error = "the connection stopped accepting data";
            m_open = false;
            return false;
        }
        sent += written;
    }
    return true;
}

bool client::wait_readable(clock_type::time_point deadline)
{
    for (;;) {
        if (!m_transport || !m_transport->is_connected()) return false;
        if (m_transport->readable()) return true;

        const auto now = clock_type::now();
        if (now >= deadline) return false;

        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        if (left > std::chrono::milliseconds(1000)) left = std::chrono::milliseconds(1000);
        if (left <= std::chrono::milliseconds(0)) return false;

        // A false here is just this slice of the wait; the loop re-checks the
        // deadline, so a transport that polls internally is fine as well.
        if (m_transport->waitForReadyRead(left)) return true;
    }
}

bool client::fill(clock_type::time_point deadline, bool& timed_out)
{
    timed_out = false;
    if (!m_transport) return false;

    std::size_t bytes = m_transport->available();
    if (bytes == 0) {
        if (!wait_readable(deadline)) {
            timed_out = true;
            return false;
        }
        bytes = m_transport->available();
        if (bytes == 0) bytes = 4096;   // a TLS transport reports only decrypted bytes
    }

    bytes = bytes < std::size_t(16u * 1024u) ? bytes : std::size_t(16u * 1024u);
    const scl2::bytearray chunk = m_transport->read(bytes);
    if (chunk.empty()) {
        if (m_last_error.empty()) m_last_error = "the connection was closed";
        m_open = false;
        return false;
    }
    m_in.append(chunk);
    return true;
}

} // namespace network::ws
