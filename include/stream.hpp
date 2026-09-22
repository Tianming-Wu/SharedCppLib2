/*
    Template virtual stream object interface.

    The goal is to provide a common interface for different stream types,
    such as file streams, pipe streams, network streams, etc.

    The detailed description is provided at the end of this file.

*/

#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <chrono>

#include "bytearray.hpp"

namespace scl2 {

// basic virtual stream object
class virtual_stream {
public:
    virtual_stream() = default;
    virtual ~virtual_stream() = default;

    // Should return true if the stream is in a valid state, false otherwise.
    virtual bool valid() = 0;
};

class basic_istream : public virtual_stream
{
public:
    basic_istream() = default;
    virtual ~basic_istream() = default;
    
    // Should return true if there is data available to read, false otherwise.
    virtual bool readyRead() = 0;

    // this one comes with a default implementation, since it's fairly compatible to implement
    // Note that the check duration is 10ms. You should implement yourself if you need a different behavior.
    virtual bool waitForReadyRead(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    // Should return the number of bytes available to read.
    virtual size_t available() = 0;

    // Should read the specified number of bytes from the stream, and return them as a bytearray.
    virtual bytearray read(size_t bytes) = 0;

    // Should read all available data from the stream, and return them as a bytearray.
    virtual bytearray readAll() = 0;

    // You'd better not add your own functions like readMessage(), you should reuse readAll()
    // instead.

    // Below are some functions that is also in the api definition, but not all streams need to
    // implement.
    
    // Note: reset() should reset the stream to an invalid (uninitialized) state, and release all
    // resources. Should return true if successful, false otherwise. The default placeholder
    // implementation (otherwise it won't compile) just returns false.
    // You should make the implementation of reset() idempotent, meaning that calling reset() on an
    // already reset stream should not cause any error, and should return true.
    virtual bool reset(); 
};

class basic_ostream : public virtual_stream {
public:
    basic_ostream() = default;
    virtual ~basic_ostream() = default;

    // Should write the given data to the stream, and return the number of bytes actually written.
    virtual size_t write(const bytearray& data) = 0;
};

// The I/O stream that can be used for both input and output.
class basic_iostream : public basic_istream, public basic_ostream
{};

// A connectable bidirectional stream — the transport abstraction used by
// protocol layers (HTTP, TLS, etc.) that sit on top of a connection.
//
// Concrete transports (e.g. network::tcp::client, a TLS client) inherit
// this and implement connect/disconnect/is_connected alongside the
// basic_iostream I/O methods. Protocol layers hold a transport_interface& and
// never need to know the concrete transport type.
class transport_interface : public basic_iostream {
public:
    transport_interface() = default;
    virtual ~transport_interface() = default;

    /// @brief Connect to a remote host.
    virtual bool connect(const std::string& host, uint16_t port) = 0;

    /// @brief Disconnect from the remote host.
    virtual void disconnect() = 0;

    /// @brief Whether a connection is currently established.
    virtual bool is_connected() const = 0;
};

} // namespace scl2

/*
Usage note:
    This is purely template, and only provides virtual interface.
    To implement a concrete stream, inherit from scl2::basic_istream or scl2::basic_ostream,
    and implement the virtual functions.

    For example, a pipe stream wrapper on windows platform may look like this:
    class pipe_stream : public scl2::basic_iostream {
    public:
        pipe_stream(HANDLE read_handle, HANDLE write_handle);
        virtual ~pipe_stream();

        disable_copy(pipe_stream)

        // move should be implemented, since you need to set the old handle to an invalid value.
        pipe_stream(pipe_stream&& another);
        pipe_stream& operator=(pipe_stream&& another);

        virtual bool valid() override;

        virtual bool readyRead() override;
        virtual bool waitForReadyRead(std::chrono::milliseconds timeout = std::chrono::seconds(5)) override;

        virtual size_t available() override;

        virtual scl2::bytearray read(size_t bytes) override;
        virtual scl2::bytearray readAll() override;

        virtual size_t write(const scl2::bytearray& data) override;
    private:
        HANDLE read_handle_;
        HANDLE write_handle_;
    };

*/