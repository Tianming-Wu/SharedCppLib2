#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <chrono>

#include "bytearray.hpp"

// namespace streamobj {

// template<typename T>
// struct stream_wrapper {
//     const T& ref;
//     explicit stream_wrapper(const T& obj) : ref(obj) {}
// };

// template<typename T>
// stream_wrapper<T> stream(const T& obj) {
//     return stream_wrapper<T>(obj);
// }

// }



// basic virtual stream object
class v_sclstream {
public:
    v_sclstream() = default;
    virtual ~v_sclstream() = default;

    virtual bool valid() = 0;
};

class basic_sclistream : public v_sclstream
{
public:
    basic_sclistream() = default;
    virtual ~basic_sclistream() = default;
    
    virtual bool readyRead() = 0;

    // this one comes with a default implementation, since it's fairly compatible to implement
    // Note that the check duration is 10ms. You should implement yourself if you need a different behavior.
    virtual bool waitForReadyRead(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    virtual size_t available() = 0;

    virtual std::bytearray read(size_t bytes) = 0;
    virtual std::bytearray readAll() = 0;
};

class basic_sclostream : public v_sclstream {
public:
    basic_sclostream() = default;
    virtual ~basic_sclostream() = default;

    virtual size_t write(const std::bytearray& data) = 0;
};

// template <typename CharT>
class basic_sclstream : public basic_sclistream, public basic_sclostream
{
};

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