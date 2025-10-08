#include "logt.hpp"

// 静态成员定义
std::mutex logt_eventbus::mutex_;
std::condition_variable logt_eventbus::cond_;
std::queue<logt_message> logt_eventbus::queue_;
std::atomic<bool> logt_eventbus::stopped_{false};


// 静态成员定义
std::ofstream logt::log_file_;
std::ostream* logt::output_stream_ = &std::cout;
bool logt::use_file_ = false;
std::mutex logt::file_mutex_;
std::mutex logt::thread_mutex_;
std::unordered_map<std::thread::id, std::string> logt::thread_names_;
std::thread logt::worker_;
std::once_flag logt::worker_flag_;

const char* logt::level_labels_[] = {
    "[INFO]",
    "[WARN]",
    "[ERROR]",
    "[DEBUG]"
};



// ------ function bodies ------

logt_message::logt_message(std::string msg) 
: timestamp(std::chrono::system_clock::now())
, content(std::move(msg)) {}


void logt_eventbus::push(const std::string& s) {
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.push(logt_message(s));
    cond_.notify_one();
}

bool logt_eventbus::pop(logt_message& result) {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [&]() { return !queue_.empty() || stopped_; });
    
    if (stopped_ && queue_.empty()) return false;
    if (queue_.empty()) return false;
    
    result = std::move(queue_.front());
    queue_.pop();
    return true;
}

void logt_eventbus::stop() {
    std::unique_lock<std::mutex> lock(mutex_);
    stopped_ = true;
    cond_.notify_all();
}



logt::logt(LogLevel level) {
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

logt::~logt()  {
    // 析构时自动推送完整日志（自动换行）
    logt_eventbus::push(ss_.str());
}

void logt::file(const std::string& filename) {
    ensure_worker_started();
    std::unique_lock<std::mutex> lock(file_mutex_);
    if (log_file_.is_open()) log_file_.close();
    log_file_.open(filename, std::ios::app);
    use_file_ = log_file_.is_open();
}

void logt::setostream(std::ostream& os) {
    ensure_worker_started();
    std::unique_lock<std::mutex> lock(file_mutex_);
    output_stream_ = &os;
    use_file_ = false;
}

void logt::claim(const std::string& name) {
    ensure_worker_started();
    std::unique_lock<std::mutex> lock(thread_mutex_);
    thread_names_[std::this_thread::get_id()] = name;
}

void logt::closefile() {
    std::unique_lock<std::mutex> lock(file_mutex_);
    if (log_file_.is_open()) log_file_.close();
    use_file_ = false;
    output_stream_ = &std::cout; // 回退到标准输出
}

void logt::shutdown() {
    logt_eventbus::stop();
    if (worker_.joinable()) worker_.join();

    std::unique_lock<std::mutex> lock(file_mutex_);
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

std::string logt::get_thread_name() {
    std::unique_lock<std::mutex> lock(thread_mutex_);
    auto it = thread_names_.find(std::this_thread::get_id());
    return it != thread_names_.end() ? it->second : "";
}

void logt::worker_thread() {
    logt_message message;
    while (logt_eventbus::pop(message)) {
        write_message(message);
    }
}

void logt::write_message(const logt_message& message) {
    std::unique_lock<std::mutex> lock(file_mutex_);
    std::string timestamp_str = format_timestamp(message.timestamp);
    if (use_file_ && log_file_.is_open()) {
        log_file_ << timestamp_str << " " << message.content << std::endl;
        log_file_.flush();
    } else if (output_stream_) {
        *output_stream_ << timestamp_str << " " << message.content << std::endl;
    }
}

// static std::string logt::format_timestamp(const std::chrono::system_clock::time_point& tp) {
//     // 使用 C++20 std::format 的时间格式化特性
//     auto now = std::chrono::current_zone()->to_local(tp);
//     auto days = std::chrono::floor<std::chrono::days>(now);
//     auto time = std::chrono::hh_mm_ss(now - days);
//     auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time.subseconds());
    
//     return std::format("[{:%Y-%m-%d %H:%M:%S}.{:03d}]", 
//                       std::chrono::year_month_day(days), 
//                       time, ms.count());
// }

std::string logt::format_timestamp(const std::chrono::system_clock::time_point& tp) {
    std::time_t currentTime = std::chrono::system_clock::to_time_t(tp);
    std::tm localTime;
# ifdef _WIN32
    localtime_s(&localTime, &currentTime);
# else
    localtime_r(&currentTime, &localTime);  // Linux 的线程安全版本
# endif
    return std::format("[{:04d}/{:02d}/{:02d} {:02d}:{:02d}:{:02d}]",
        localTime.tm_year+1900, localTime.tm_mon+1, localTime.tm_mday,
        localTime.tm_hour, localTime.tm_min, localTime.tm_sec
    );
}

void logt::ensure_worker_started() {
    std::call_once(worker_flag_, []() {
        worker_ = std::thread(worker_thread);
    });
}