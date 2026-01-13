/*

Log Threaded (logt) - A simple asynchronous logging library for C++23

Note: to output to console with colors, include logc.hpp and use `logt::install_preprocessor(logc::logPreprocessor);`
Doesn't work well in file mode, disable by yourself.

*/
#pragma once

#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <string>
#include <atomic>
#include <format>
#include <sstream>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <filesystem>
#include <bitset>
#include <map>
#include <initializer_list>
#include <new>

#include "basics.hpp" // for disable_copy, disable_move

#ifdef UNICODE
    #define LOGT_WCHAR_SUPPORT
#endif

#ifdef LOGT_WCHAR_SUPPORT
    #include <locale>
    #include <codecvt>
    #include <type_traits>
#endif

#ifndef LOGT_MAX_CHANNEL
    #define LOGT_MAX_CHANNEL 16
#endif

enum class LogLevel {
    Quiet = -1, // For not logging anything (filter only)
    Debug =  0,
    Info  =  1,
    Warn  =  2,
    Error =  3,
    Fatal =  4
};

struct logt_channel {
    enum class ChannelType {
        invalid = -1, stdcout = 0, file = 1, custom_ostream = 2
    };

    logt_channel() :type(ChannelType::invalid), enabled(false) {}
    inline ~logt_channel() { close(); }

    ChannelType type;
    bool valid;
    bool enabled;

    union {
        std::ofstream fileobj;
        std::ostream* ostream;
    };

    void open(); // reserved
    void close(); // close the stream and set it to invalid

    mutable std::mutex _mutex;
    [[nodiscard]] auto lock();
};

struct logt_channelinfo {
    std::bitset<LOGT_MAX_CHANNEL> channels;
    inline bool operator[](size_t index) const { return bool(channels[index]); }
    inline void set(size_t index, bool value = true) { channels[index] = value; }

    inline bool stdoutput() const { return bool(channels[0]); }
};

struct logt_message {
    LogLevel level;
    logt_channelinfo channels;
    std::chrono::system_clock::time_point timestamp;
    std::string content;
    
    logt_message() = default;
    logt_message(std::string msg, LogLevel level, logt_channelinfo channels);
};

class logt_eventbus {
public:
    static void push(const std::string& s, LogLevel level, logt_channelinfo channel);
    static bool pop(logt_message& result);
    static void stop();

private:
    static std::mutex mutex_;
    static std::condition_variable cond_;
    static std::queue<logt_message> queue_;
    static std::atomic<bool> stopped_;
};


class logt_format {
public:
    logt_format() {}
    
    logt_format(const logt_format&) = default;
    logt_format& operator=(const logt_format&) = default;
    logt_format(logt_format&&) = default;
    logt_format& operator=(logt_format&&) = default;

    struct formatSettings {
        bool enableTimestamp = true;
        bool enableThreadTag = true;
        bool enableLocalTag = true;
    };

    struct formatInfo {
        LogLevel level;
        std::string signature;
    };

    typedef std::function<std::string(const formatSettings&, const formatInfo&)> format_func;

    template<typename T>
    static std::string format_var(const std::string& format, const T& value) {
        return std::vformat(format, std::make_format_args(value));
    }

    format_func formatFunc = nullptr;
    formatSettings settings;
};


typedef std::function<bool(logt_message&)> preprocessor_t;

class logt_sso {
public:
    logt_sso(LogLevel level, const logt_format& formatter, const logt_channelinfo& channels, const std::string& signature = "");
    ~logt_sso();
     
    enable_move_only(logt_sso) // 禁止拷贝，支持移动
    
    // Serialize support (serialize() member function returning std::string)
    template<typename T>
    requires requires(const T& t) {
        requires std::is_class_v<T>;  // 必须是类类型
        { t.serialize() } -> std::convertible_to<std::string>;  // 返回 std::string
    }
    logt_sso& operator<<(const T& value) {
        ss_ << value.serialize();
        return *this;
    }

    // Universial stream support (excluded serialize() supported types)
    // using stringstream
    template<typename T>
    requires (!requires(const T& t) { 
        requires std::is_class_v<T>;
        { t.serialize() } -> std::same_as<std::string>;
    }) && requires(T t, std::stringstream& test_ss) { test_ss << t; }
    logt_sso& operator<<(const T& value) {
        ss_ << value;
        return *this;
    }

#ifdef LOGT_WCHAR_SUPPORT
    logt_sso& operator<<(const std::wstring& value) {
        // 宽字符串转多字节字符串
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        ss_ << converter.to_bytes(value);
        return *this;
    }
#endif
    
private:
    std::string default_formatter(const logt_format::formatSettings& settings, const logt_format::formatInfo& info);

private:
    std::stringstream ss_;
    LogLevel level_;
    logt_channelinfo channels;
};


class logt_sig {
public:
    logt_sig(const std::string& name);
    
    const std::string& name() const { return name_; }

    logt_sso info()  const { return logt_sso(LogLevel::Info,  formatter, channels, name_); }
    logt_sso warn()  const { return logt_sso(LogLevel::Warn,  formatter, channels, name_); }
    logt_sso error() const { return logt_sso(LogLevel::Error, formatter, channels, name_); }
    logt_sso fatal() const { return logt_sso(LogLevel::Fatal, formatter, channels, name_); }
    logt_sso debug() const { return logt_sso(LogLevel::Debug, formatter, channels, name_); }

    bool setFormatter(logt_format::format_func formatter_);
    bool setChannel(int channelid, bool enable);
    bool setChannels(std::initializer_list<int> enabled_channels);
    
private:
    std::string name_;
    logt_format formatter;
    logt_channelinfo channels;
};

class logt {
    friend class logt_sso;
    friend class logt_sig;
public:
    // 静态配置方法

    /// @brief Add a log file to the logging system.
    /// @param filename The path to the log file
    /// @param default_enable Whether logging to this file is enabled by default
    /// @return An integer identifier for the added log file
    static int addfile(const std::filesystem::path& filename, bool default_enable = true);

    /// @brief Add an output stream to the logging system.
    /// @param os The output stream
    /// @param default_enable Whether logging to this stream is enabled by default
    /// @return An integer identifier for the added output stream
    static int addostream(std::ostream& os, bool default_enable = true);

    /// @brief Enable or disable logging to standard output (console).
    /// @param enable Whether to enable logging to standard output
    /// @param default_enable Whether logging to standard output is enabled by default
    static void stdcout(bool enable, bool default_enable = true);

    /// @brief Claim the current thread with a name for logging purposes.
    /// @param name The name to associate with the current thread
    static void claim(const std::string& name);

    /// @brief Set the global log filter level, by default is `LogLevel::Info`.
    /// @param level The minimum log level to output
    inline static void setFilterLevel(LogLevel level) { filter_level_ = level; }

    // 静态关闭方法
    /// @brief Shutdown the logging system, ensuring all messages are processed.
    static void shutdown();

    /// @brief Shutdown the logging system then close the application.
    static void exit(int exitcode);

    /// @brief Install preprocessor function to process log messages.
    /// @param preprocessor A function that takes a logt_message reference and returns a bool.
    static void install_preprocessor(preprocessor_t preprocessor);

    /// @brief Set a custom formatter function for log messages.
    /// @param formatter the formatting function
    void setFormatter(logt_format::format_func formatter);

    /// @brief Enable super timestamp format (with nanoseconds).
    /// @param enabled Whether to enable the super timestamp format
    inline static void enableSuperTimestamp(bool enabled) { 
        super_timestamp_enabled_ = enabled; 
    }
    
    inline static bool isSuperTimestampEnabled() { 
        return super_timestamp_enabled_; 
    }

    /// @brief Format a system_clock timestamp.
    /// @param tp a time point from system_clock. You can use something like `std::chrono::system_clock::now()`.
    /// @return The formatted timestamp string.
    static std::string format_timestamp(const std::chrono::system_clock::time_point& tp);

    /// @brief Format a steady_clock timestamp.
    /// @param tp a time point from steady_clock. You can use something like `std::chrono::steady_clock::now()`.
    /// @return The formatted timestamp string.
    static std::string format_timestamp(const std::chrono::steady_clock::time_point& tp);

private:
    static std::string get_thread_name();
    static void worker_thread();
    static void ensure_worker_started();

    static void write_message(const logt_message& message);

    // 静态成员
    static LogLevel filter_level_;

    static logt_channel channels_[LOGT_MAX_CHANNEL];
    static logt_channelinfo channelinfo_;  // track registered channels
    static int last_channel_id;
    static logt_channelinfo default_channels_;

    static preprocessor_t preprocessor_;
    static std::mutex thread_mutex_;
    static std::unordered_map<std::thread::id, std::string> thread_names_;
    
    static std::thread worker_;
    static std::once_flag worker_flag_;

    static std::atomic<bool> super_timestamp_enabled_;

    static const char* level_labels_[];

    static logt_format formatter_;
};

// 日志文件管理
// class logt_manager {
// public:
//     logt_manager(const std::filesystem::path& logFolder);
//     ~logt_manager();

// private:
//     std::filesystem::path m_logFolder;
// };

// 类内签名定义宏
// 不建议使用，因为类成员函数的自定义签名目前会覆盖这个宏定义的签名，所以它不太实用
#define LOGT_DECLARE \
private: \
    static ::logt_sig logt;

// 类内签名定义宏 - 公共访问权限
#define LOGT_PUBLIC_DECLARE \
public: \
    static ::logt_sig logt;

// 类外签名定义宏
#define LOGT_DEFINE(Class, Name) \
    ::logt_sig Class::logt(Name)

// 全局模块签名 - 在全局或命名空间作用域使用
#define LOGT_MODULE(Name) \
    static ::logt_sig logt(Name)

// 函数内局部签名 - 在函数作用域使用
#define LOGT_LOCAL(Name) \
    static ::logt_sig logt(Name)

// 临时签名（用于一次性使用）
#define LOGT_TEMP(Name) \
    ::logt_sig(Name)