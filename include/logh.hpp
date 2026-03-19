/*
    Log Header-Only (logh) - A simple header-only logging library for C++23

    Note that this library actually use the same method as stb - single header implementation.
    You need to define `LOGH_IMPLEMENTATION` in exactly one source file before including this
    header, otherwise you will get linker errors.

    You can include this header as #include <SharedCppLib2/logh.hpp> or copy it to your project
    and include it as #include "logh.hpp".

    This library DOES NOT have any dependency (except those included in the C++23 standard).
    One of the cases when this library is actually useful is temporary logging for debugging
    purposes, where you want your simple program to be completely silent in production.

    The idea is similar to logt, but does not support multiple channels, and is not asynchronous.
    The grammer is also different. Unlike logt, it does not work like a stream (like qDebug),
    and instead it's more like std::format, and is similar to C++26's std::print.

    By default, logh outputs to console. You can use logh::setChannel() to change the output channel.
    You cannot construct multiple logh instance to output to multiple channels at the same time. This
    is intentional.

    You can only choose to output to console or file, using setChannel().

    Available compile-control macros:
        LOGH_IMPLEMENTATION - Define this in exactly one source file to include the implementation of the library.
        LOGH_CONDITIONAL - Define this to enable conditional logging macros (LOGHC_DEBUG, LOGHC_INFO, etc).
        LOGH_DISCARD - Define this to disable all logh content, and only preserve the LOGH_CONDITIONAL macros.
            You can use this to completely exclude logging features.
*/

#pragma once

#ifndef LOGH_DISCARD

#include <string>
#include <fstream>
#include <iostream>
#include <chrono> // for timestamp features

// output control helper object for logh
// not for user.
class logh_out
{
    friend class logh;
private:
    enum class outStreamType { Console, File };
    logh_out(outStreamType type, const std::string& filename = "");
    ~logh_out();

    logh_out(const logh_out&) = delete;
    logh_out& operator=(const logh_out&) = delete;
    logh_out(logh_out&& other) noexcept;
    logh_out& operator=(logh_out&& other) noexcept;

    std::ostream& stream();

    outStreamType m_type;
    std::ostream* p_out_stream;
};

// basic logh object
// the overall control logic is all static
class logh
{
protected:
    // internal format and output function.
    template <typename... Args>
    static void __ (const std::string& format_str, Args&&... args) {
        try {
            std::string formatted_message = std::vformat(format_str, std::make_format_args(args...));
            p_out.stream() << formatted_message << std::endl;
        } catch (const std::format_error& e) {
            p_out.stream() << "Log formatting error: " << e.what() << " | Original format string: " << format_str << std::endl;
        }
    }

    // internal format-only helper function
    template <typename... Args>
    static std::string ___ (const std::string& format_str, Args&&... args) {
        try {
            std::string formatted_message = std::vformat(format_str, std::make_format_args(args...));
            return formatted_message;
        } catch (const std::format_error& e) {
            // should raise another exception here later.
            return "";
        }
    }

    // internal timestamp function
    static std::string timestamp(std::chrono::system_clock::time_point tp);

public:
    template <typename... Args>
    static void log(const std::string& format_str, Args&&... args) {
        __ (format_str, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void info(const std::string& format_str, Args&&... args) {
        __ ( "[INFO] {} {}", timestamp(std::chrono::system_clock::now()), ___ (format_str, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void warn(const std::string& format_str, Args&&... args) {
        __ ( "[WARN] {} {}", timestamp(std::chrono::system_clock::now()), ___ (format_str, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void error(const std::string& format_str, Args&&... args) {
        __ ( "[ERROR] {} {}", timestamp(std::chrono::system_clock::now()), ___ (format_str, std::forward<Args>(args)...));
    }
    template <typename... Args>
    static void fatal(const std::string& format_str, Args&&... args) {
        __ ( "[FATAL] {} {}", timestamp(std::chrono::system_clock::now()), ___ (format_str, std::forward<Args>(args)...));
    }
    template <typename... Args>
    static void debug(const std::string& format_str, Args&&... args) {
        __ ( "[DEBUG] {} {}", timestamp(std::chrono::system_clock::now()), ___ (format_str, std::forward<Args>(args)...));
    }

    void setChannel(logh_out::outStreamType type, const std::string& filename = "") {
        p_out = logh_out(type, filename);
    }

private:
    static logh_out p_out;
};


#endif // LOGH_DISCARD

/*
    These are macros to simplify conditional logging.
    These macros will be replaced with empty if program LOGH_CONDITIONAL is not defined.

    For example, you can set these at the beginning of your source file:
    #ifdef DEBUG
        #define LOGH_CONDITIONAL
    #endif

    And then you can use LOGHC_INFO etc and only get the logh outputs when DEBUG is defined.

*/

#ifdef LOGH_CONDITIONAL
    #define LOGHC_INFO(...) logh::info(__VA_ARGS__)
    #define LOGHC_WARN(...) logh::warn(__VA_ARGS__)
    #define LOGHC_ERROR(...) logh::error(__VA_ARGS__)
    #define LOGHC_FATAL(...) logh::fatal(__VA_ARGS__)
    #define LOGHC_DEBUG(...) logh::debug(__VA_ARGS__)

#else // discard
    #define LOGHC_INFO(...) // discard
    #define LOGHC_WARN(...)
    #define LOGHC_ERROR(...)
    #define LOGHC_FATAL(...)
    #define LOGHC_DEBUG(...)

#endif // LOGH_CONDITIONAL

/*
    The below code is the implementation part of logh library.
*/

#ifdef LOGH_IMPLEMENTATION

logh_out logh::p_out(logh_out::outStreamType::Console);

logh_out::logh_out(outStreamType type, const std::string& filename)
    : m_type(type)
{
    if(type == outStreamType::Console) {
        p_out_stream = &std::cout;
    } else if(type == outStreamType::File) {
        p_out_stream = new std::ofstream(filename, std::ios::app);
        if (!static_cast<std::ofstream*>(p_out_stream)->is_open()) {
            delete static_cast<std::ofstream*>(p_out_stream);
            p_out_stream = nullptr;
            throw std::runtime_error("Failed to open log file: " + filename);
        }
    } else {
        p_out_stream = nullptr;
        throw std::invalid_argument("Invalid output stream type");
    }
}

logh_out::~logh_out() {
    if (m_type == outStreamType::File && p_out_stream) {
        delete p_out_stream;
    }
}

logh_out::logh_out(logh_out&& other) noexcept
    : m_type(other.m_type), p_out_stream(other.p_out_stream)
{
    other.m_type = outStreamType::Console;
    other.p_out_stream = &std::cout;
}

logh_out& logh_out::operator=(logh_out&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    if (m_type == outStreamType::File && p_out_stream) {
        delete p_out_stream;
    }
    m_type = other.m_type;
    p_out_stream = other.p_out_stream;
    other.m_type = outStreamType::Console;
    other.p_out_stream = &std::cout;
    return *this;
}

inline std::ostream &logh_out::stream()
{
    if (!p_out_stream) {
        throw std::runtime_error("Output stream is not initialized");
    }
    return *p_out_stream;
}

std::string logh::timestamp(std::chrono::system_clock::time_point tp)
{
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;

    return std::format("[{}]", std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S"));
}


#endif // LOGH_IMPLEMENTATION