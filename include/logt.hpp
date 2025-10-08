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

struct logt_message {
    std::chrono::system_clock::time_point timestamp;
    std::string content;
    
    logt_message() = default;
    logt_message(std::string msg) 
        : timestamp(std::chrono::system_clock::now())
        , content(std::move(msg)) {}
};

class logt_eventbus {
public:
    static void push(const std::string& s) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(logt_message(s));
        cond_.notify_one();
    }

    static bool pop(logt_message& result) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [&]() { return !queue_.empty() || stopped_; });
        
        if (stopped_ && queue_.empty()) return false;
        if (queue_.empty()) return false;
        
        result = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    static void stop() {
        std::unique_lock<std::mutex> lock(mutex_);
        stopped_ = true;
        cond_.notify_all();
    }

private:
    static std::mutex mutex_;
    static std::condition_variable cond_;
    static std::queue<logt_message> queue_;
    static std::atomic<bool> stopped_;
};

enum class LogLevel {
    INFO,
    WARN,
    ERROR,
    DEBUG
};

class logt {
public:
    // 流式日志对象
    logt(LogLevel level = LogLevel::INFO) {
        ensure_worker_started();
        
        // 日志标签
        ss_ << level_labels_[static_cast<int>(level)] << " ";

        // 自动添加线程标识
        auto thread_name = get_thread_name();
        if (!thread_name.empty()) {
            ss_ << "[" << thread_name << "] ";
        } else {
            ss_ << "[" << std::this_thread::get_id() << "] ";
        }
    }

    ~logt() {
        // 析构时自动推送完整日志（自动换行）
        logt_eventbus::push(ss_.str());
    }

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
    static void file(const std::string& filename) {
        ensure_worker_started();
        std::unique_lock<std::mutex> lock(file_mutex_);
        if (log_file_.is_open()) log_file_.close();
        log_file_.open(filename, std::ios::app);
        use_file_ = log_file_.is_open();
    }

    static void setostream(std::ostream& os) {
        ensure_worker_started();
        std::unique_lock<std::mutex> lock(file_mutex_);
        output_stream_ = &os;
        use_file_ = false;
    }

    static void claim(const std::string& name) {
        ensure_worker_started();
        std::unique_lock<std::mutex> lock(thread_mutex_);
        thread_names_[std::this_thread::get_id()] = name;
    }

    static void closefile() {
        std::unique_lock<std::mutex> lock(file_mutex_);
        if (log_file_.is_open()) log_file_.close();
        use_file_ = false;
        output_stream_ = &std::cout; // 回退到标准输出
    }

    static void shutdown() {
        logt_eventbus::stop();
        if (worker_.joinable()) worker_.join();

        std::unique_lock<std::mutex> lock(file_mutex_);
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

    // 快捷方法 - 返回临时对象用于流式输出
    inline static logt info() { 
        return logt(LogLevel::INFO);
    }

    inline static logt warn() {
        return logt(LogLevel::WARN);
    }

    inline static logt error() {
        return logt(LogLevel::ERROR);
    }

    inline static logt debug() {
        return logt(LogLevel::DEBUG);
    }

private:
    static std::string get_thread_name() {
        std::unique_lock<std::mutex> lock(thread_mutex_);
        auto it = thread_names_.find(std::this_thread::get_id());
        return it != thread_names_.end() ? it->second : "";
    }

    static void worker_thread() {
        logt_message message;
        while (logt_eventbus::pop(message)) {
            write_message(message);
        }
    }

    static void write_message(const logt_message& message) {
        std::unique_lock<std::mutex> lock(file_mutex_);
        std::string timestamp_str = format_timestamp(message.timestamp);
        if (use_file_ && log_file_.is_open()) {
            log_file_ << timestamp_str << " " << message.content << std::endl;
            log_file_.flush();
        } else if (output_stream_) {
            *output_stream_ << timestamp_str << " " << message.content << std::endl;
        }
    }

    // static std::string format_timestamp(const std::chrono::system_clock::time_point& tp) {
    //     // 使用 C++20 std::format 的时间格式化特性
    //     auto now = std::chrono::current_zone()->to_local(tp);
    //     auto days = std::chrono::floor<std::chrono::days>(now);
    //     auto time = std::chrono::hh_mm_ss(now - days);
    //     auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time.subseconds());
        
    //     return std::format("[{:%Y-%m-%d %H:%M:%S}.{:03d}]", 
    //                       std::chrono::year_month_day(days), 
    //                       time, ms.count());
    // }

    static std::string format_timestamp(const std::chrono::system_clock::time_point& tp) {
        std::time_t currentTime = std::chrono::system_clock::to_time_t(tp);
        std::tm localTime = *std::localtime(&currentTime);
        return std::format("[{:04d}/{:02d}/{:02d} {:02d}:{:02d}:{:02d}]",
            localTime.tm_year+1900, localTime.tm_mon+1, localTime.tm_mday,
            localTime.tm_hour, localTime.tm_min, localTime.tm_sec
        );
    }

    static void ensure_worker_started() {
        std::call_once(worker_flag_, []() {
            worker_ = std::thread(worker_thread);
        });
    }

    std::stringstream ss_;

    // 静态成员
    static std::ofstream log_file_;
    static std::ostream* output_stream_;
    static bool use_file_;
    
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
inline logt tDebug() { return logt::debug(); }