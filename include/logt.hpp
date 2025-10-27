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

class logt {
public:
    // 流式日志对象
    logt(LogLevel level = LogLevel::l_INFO);

    ~logt();

    // 禁止拷贝
    logt(const logt&) = delete;
    logt& operator=(const logt&) = delete;
    
    // 支持移动
    logt(logt&&) = default;
    logt& operator=(logt&&) = default;

    // 流输出操作符
    template<typename T>
    logt& operator<<(const T& value) {
        ss_ << value;
        return *this;
    }

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

    // 快捷方法 - 返回临时对象用于流式输出
    inline static logt info()  {  return logt(LogLevel::l_INFO);  }
    inline static logt warn()  {  return logt(LogLevel::l_WARN);  }
    inline static logt error() {  return logt(LogLevel::l_ERROR); }
    inline static logt fatal() {  return logt(LogLevel::l_FATAL); }
    inline static logt debug() {  return logt(LogLevel::l_DEBUG); }

private:
    static std::string get_thread_name();
    static void worker_thread();
    static void ensure_worker_started();

    static void write_message(const logt_message& message);
    static std::string format_timestamp(const std::chrono::system_clock::time_point& tp);

    std::stringstream ss_;
    LogLevel level_;

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

    static const char* level_labels_[];
};


// 全局函数，类似 qDebug() 的用法
inline logt tLog() { return logt(); }
inline logt tInfo() { return logt::info(); }
inline logt tWarn() { return logt::warn(); }
inline logt tError() { return logt::error(); }
inline logt tFatal() { return logt::fatal(); }
inline logt tDebug() { return logt::debug(); }