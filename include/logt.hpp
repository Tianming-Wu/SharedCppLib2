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

#ifdef LOGT_WCHAR_SUPPORT
    #include <locale>
    #include <codecvt>
    #include <type_traits>
#endif

enum class LogLevel {
    l_QUIET = -1, // For not logging anything
    l_DEBUG =  0,
    l_INFO  =  1,
    l_WARN  =  2,
    l_ERROR =  3,
    l_FATAL =  4
};

struct logt_message {
    LogLevel level;
    std::chrono::system_clock::time_point timestamp;
    std::string content;
    
    logt_message() = default;
    logt_message(std::string msg, LogLevel level);
};

class logt_eventbus {
public:
    static void push(const std::string& s, LogLevel level);
    static bool pop(logt_message& result);
    static void stop();

private:
    static std::mutex mutex_;
    static std::condition_variable cond_;
    static std::queue<logt_message> queue_;
    static std::atomic<bool> stopped_;
};



typedef std::function<bool(logt_message&)> preprocessor_t;

class logt_sso {
public:
    logt_sso(LogLevel level, const std::string& signature = "");
    ~logt_sso();
    
    // 禁止拷贝
    logt_sso(const logt_sso&) = delete;
    logt_sso& operator=(const logt_sso&) = delete;
    
    // 支持移动
    logt_sso(logt_sso&&) = default;
    logt_sso& operator=(logt_sso&&) = default;
    
    // 流输出操作符
    template<typename T>
    requires requires(T t, std::stringstream& ss) {
        ss << t;  // 必须支持 stringstream 的 << 操作符
    }
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
    std::stringstream ss_;
    LogLevel level_;
};


class logt_sig {
public:
    logt_sig(const std::string& name) : name_(name) {}
    
    const std::string& name() const { return name_; }

    logt_sso info() const { return logt_sso(LogLevel::l_INFO, name_); }
    logt_sso warn() const { return logt_sso(LogLevel::l_WARN, name_); }
    logt_sso error() const { return logt_sso(LogLevel::l_ERROR, name_); }
    logt_sso fatal() const { return logt_sso(LogLevel::l_FATAL, name_); }
    logt_sso debug() const { return logt_sso(LogLevel::l_DEBUG, name_); }
    
private:
    std::string name_;
};

class logt {
    friend class logt_sso;
public:
    // 静态配置方法
    static void file(const std::string& filename);
    static void setostream(std::ostream& os);
    static void stdcout();
    static void claim(const std::string& name);
    static void closefile();
    inline static void setFilterLevel(LogLevel level) { filter_level_ = level; }

    // 静态关闭方法
    /// @brief Shutdown the logging system, ensuring all messages are processed.
    static void shutdown();

    /** @brief Install preprocessor function to process log messages.
     * @param preprocessor A function that takes a logt_message reference and returns a bool.
     */
    static void install_preprocessor(preprocessor_t preprocessor);

    inline static void enableSuperTimestamp(bool enabled) { 
        super_timestamp_enabled_ = enabled; 
    }
    
    inline static bool isSuperTimestampEnabled() { 
        return super_timestamp_enabled_; 
    }

private:
    static std::string get_thread_name();
    static void worker_thread();
    static void ensure_worker_started();

    static void write_message(const logt_message& message);
    static std::string format_timestamp(const std::chrono::system_clock::time_point& tp);

    // 静态成员
    static LogLevel filter_level_;

    static std::ofstream log_file_;
    static std::ostream* output_stream_;
    static bool use_file_;

    static preprocessor_t preprocessor_;
    
    static std::mutex file_mutex_;
    static std::mutex thread_mutex_;
    static std::unordered_map<std::thread::id, std::string> thread_names_;
    
    static std::thread worker_;
    static std::once_flag worker_flag_;

    static std::atomic<bool> super_timestamp_enabled_;

    static const char* level_labels_[];
};


#define LOGT_DECLARE \
private: \
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