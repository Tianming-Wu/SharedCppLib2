/*
    WebSocket (RFC 6455) shared types.

    Frames, masks, fragmentation and control frames are handled inside
    network::ws::client; what the caller sees is a message.
*/

#pragma once

#include <cstdint>

#include "bytearray.hpp"

namespace network::ws {

/// @brief Opcode of a frame.
enum class opcode : std::uint8_t {
    continuation = 0x0,
    text         = 0x1,
    binary       = 0x2,
    close        = 0x8,
    ping         = 0x9,
    pong         = 0xA,
};

/// @brief One data message, its fragments already joined.
struct message {
    opcode type = opcode::text;
    scl2::bytearray payload;
};

/// @brief How receive() ended.
/// @note A timeout, a closed connection and a protocol failure are told apart
///       on purpose: only `timeout` means "call again".
enum class receive_status : std::uint8_t {
    message = 0,  ///< the message was written to the output argument
    timeout,      ///< nothing complete arrived within the time limit
    closed,       ///< the connection is not open
    failed,       ///< protocol violation or transport failure; see lastError()
};

/// @brief Close status codes this module uses (RFC 6455 section 7.4.1).
enum class close_code : std::uint16_t {
    normal            = 1000,
    going_away        = 1001,
    protocol_error    = 1002,
    unsupported_data  = 1003,
    invalid_payload   = 1007,
    policy_violation  = 1008,
    message_too_big   = 1009,
    internal_error    = 1011,
};

} // namespace network::ws
