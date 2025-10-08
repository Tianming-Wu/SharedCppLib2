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