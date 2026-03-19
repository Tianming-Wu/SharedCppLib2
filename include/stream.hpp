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

// basic virtual stream object
class v_sclstream {
public:
    v_sclstream() = default;
    virtual ~v_sclstream() = default;

    // Should return true if the stream is in a valid state, false otherwise.
    virtual bool valid() = 0;
};

class basic_sclistream : public v_sclstream
{
public:
    basic_sclistream() = default;
    virtual ~basic_sclistream() = default;
    
    // Should return true if there is data available to read, false otherwise.
    virtual bool readyRead() = 0;

    // this one comes with a default implementation, since it's fairly compatible to implement
    // Note that the check duration is 10ms. You should implement yourself if you need a different behavior.
    virtual bool waitForReadyRead(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    // Should return the number of bytes available to read.
    virtual size_t available() = 0;

    // Should read the specified number of bytes from the stream, and return them as a bytearray.
    virtual std::bytearray read(size_t bytes) = 0;

    // Should read all available data from the stream, and return them as a bytearray.
    virtual std::bytearray readAll() = 0;

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

class basic_sclostream : public v_sclstream {
public:
    basic_sclostream() = default;
    virtual ~basic_sclostream() = default;

    // Should write the given data to the stream, and return the number of bytes actually written.
    virtual size_t write(const std::bytearray& data) = 0;
};

// The I/O stream that can be used for both input and output.
class basic_sclstream : public basic_sclistream, public basic_sclostream
{};

/*
Usage note:
    This is purely template, and only provides virtual interface.
    To implement a concrete stream, inherit from basic_sclistream or basic_sclostream,
    and implement the virtual functions.

    For example, a pipe stream wrapper on windows platform may look like this:
    class pipe_stream : public basic_sclstream {
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

        virtual std::bytearray read(size_t bytes) override;
        virtual std::bytearray readAll() override;

        virtual size_t write(const std::bytearray& data) override;
    private:
        HANDLE read_handle_;
        HANDLE write_handle_;
    };

*/