#include "logt.hpp"

#include "basics.hpp"

// 静态成员定义
std::mutex logt_eventbus::mutex_;
std::condition_variable logt_eventbus::cond_;
std::queue<logt_message> logt_eventbus::queue_;
std::atomic<bool> logt_eventbus::stopped_{false};


// 静态成员定义
LogLevel logt::filter_level_ = LogLevel::l_INFO;
// std::ofstream logt::log_file_;
// std::ostream* logt::output_stream_ = &std::cout;
logt_channel logt::channels_[LOGT_MAX_CHANNEL];
int logt::last_channel_id = 1;
// bool logt::use_file_ = false;
preprocessor_t logt::preprocessor_;
std::mutex logt::file_mutex_;
std::mutex logt::thread_mutex_;
std::unordered_map<std::thread::id, std::string> logt::thread_names_;
std::thread logt::worker_;
std::once_flag logt::worker_flag_;
std::atomic<bool> logt::super_timestamp_enabled_{false};
logt_format logt::formatter_;
logt_channelinfo logt::channelinfo_;

const char* logt::level_labels_[] = {
    "[DEBUG]",
    "[INFO]",
    "[WARN]",
    "[ERROR]",
    "[FATAL]",
};



// ------ function bodies ------


void logt_channel::open() {
    switch(type) {
        case ChannelType::stdcout:
            break;
        case ChannelType::file:
            break;
    }
}

void logt_channel::close() {
    if(valid) {
        switch(type) {
        case ChannelType::stdcout:
            break;
        case ChannelType::file:
            fileobj.close();
            break;
        }
        valid = false;
    }
}


logt_message::logt_message(std::string msg, LogLevel level, logt_channelinfo channels) 
: timestamp(std::chrono::system_clock::now())
, content(std::move(msg))
, level(level)
, channels(channels) {}


void logt_eventbus::push(const std::string& s, LogLevel level, logt_channelinfo channel) {
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.push(logt_message(s, level, channel));
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



logt_sso::logt_sso(LogLevel level, const logt_format& formatter, const logt_channelinfo& channels, const std::string& signature) 
    : level_(level), channels(channels)
{
    logt::ensure_worker_started();

    logt_format::formatInfo info;
    info.level = level;
    info.signature = signature;

    if(formatter.formatFunc) {
        formatter.formatFunc(formatter.settings, info);
    } else {
        default_formatter(formatter.settings, info);
    }
}

logt_sso::~logt_sso() {
    // 析构时自动推送完整日志
    if (level_ >= logt::filter_level_) {
        logt_eventbus::push(ss_.str(), level_, channels);
    }
}

std::string logt_sso::default_formatter(const logt_format::formatSettings& settings, const logt_format::formatInfo &info)
{
    std::string format_result;
    format_result = logt::level_labels_[static_cast<int>(info.level)];

    if(settings.enableThreadTag) {
        format_result += std::string(" ");
        auto thread_name = logt::get_thread_name();
        if (!thread_name.empty()) {
            format_result += "[" + thread_name + "] ";
        } else {
            format_result += "[" + streamed_to_string(std::this_thread::get_id()) + "] ";
        }
    }

    if (!info.signature.empty()) {
        ss_ << "[" << info.signature << "] ";
    }

    return format_result;
}



logt_sig::logt_sig(const std::string& name)
    : name_(name)
{
    formatter = logt::formatter_;
}

void logt_sig::setFormatter(logt_format::format_func formatter_) {
    formatter.formatFunc = formatter_;
}

void logt_sig::setChannel(int channelid, bool enable) {
    channels.channels[channelid] = enable;
}

void logt_sig::setChannels(std::initializer_list<int> enabled_channels) {
    std::bitset<LOGT_MAX_CHANNEL> newbs(0);

    for(int i : enabled_channels) {
        newbs[i] = true;
    }

    channels.channels = newbs; // overwrite
}



int logt::addfile(const std::filesystem::path& filename) {
    ensure_worker_started();
    std::unique_lock<std::mutex> lock(file_mutex_);

    int newid = last_channel_id++;
    logt_channel &channel = channels_[newid];

    channel.type = logt_channel::ChannelType::file;
    channel.fileobj = std::ofstream(filename, std::ios::app);
    channel.valid = channel.fileobj.is_open();
    channel.enabled = true;

    return newid;
}

int logt::addostream(std::ostream& os) {
    ensure_worker_started();
    std::unique_lock<std::mutex> lock(file_mutex_);

    int newid = last_channel_id++;
    logt_channel &channel = channels_[newid];

    channel.type = logt_channel::ChannelType::custom_ostream;
    channel.ostream = &os;
    channel.valid = (&os != nullptr); // may be useless (
    channel.enabled = true;

    return newid;
}

void logt::stdcout(bool enable) {
    ensure_worker_started();
    channels_[0].enabled = true;
}

void logt::claim(const std::string& name) {
    ensure_worker_started();
    std::unique_lock<std::mutex> lock(thread_mutex_);
    thread_names_[std::this_thread::get_id()] = name;
}

void logt::shutdown() {
    logt_eventbus::stop();
    if (worker_.joinable()) worker_.join();

    std::unique_lock<std::mutex> lock(file_mutex_);
    // for(logt_channel& ch : channels_) {
    //     if(channels_.type == logt_channel::ChannelType::stdcout)
    // }
}

void logt::exit(int exitcode)
{
    shutdown();
    ::exit(exitcode);
}

void logt::install_preprocessor(preprocessor_t preprocessor)
{
    preprocessor_ = std::move(preprocessor);
}

void logt::setFormatter(logt_format::format_func formatter)
{
    formatter_.formatFunc = formatter;
}

std::string logt::get_thread_name() {
    std::unique_lock<std::mutex> lock(thread_mutex_);
    auto it = thread_names_.find(std::this_thread::get_id());
    return it != thread_names_.end() ? it->second : "";
}

void logt::worker_thread() {
    // make sure stdcout is configured.
    channels_[0].type = logt_channel::ChannelType::stdcout;
    channels_[0].ostream = &std::cout;
    channels_[0].valid = true;
    channels_[0].enabled = true;

    logt_message message;
    while (logt_eventbus::pop(message)) {
        if(preprocessor_) {
            preprocessor_(message);
            // return value was originally designed to undo changes, but now it is ignored.
        }
        write_message(message);
    }
}

void logt::write_message(const logt_message& message) {
    std::unique_lock<std::mutex> lock(file_mutex_);
    std::string timestamp_str = (formatter_.settings.enableTimestamp)?(format_timestamp(message.timestamp) + " "):"";

    if(message.channels.stdoutput()) {
        *channels_[0].ostream << timestamp_str << message.content << std::endl;
    }

    for(int i = 1; i < LOGT_MAX_CHANNEL; i++) {
        if(message.channels[i] && channels_[i].valid) {
            switch(channels_[i].type) {
            case logt_channel::ChannelType::file:
                channels_[i].fileobj << timestamp_str << message.content << std::endl;
                channels_[i].fileobj.flush();
                break;
            case logt_channel::ChannelType::custom_ostream:
                *channels_[0].ostream << timestamp_str << message.content << std::endl;
                // endl already contains flush
                break;
            }
        }
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
#ifdef _WIN32
    localtime_s(&localTime, &currentTime);
#else
    localtime_r(&currentTime, &localTime);
#endif
    
    if (super_timestamp_enabled_) {
        // 高精度时间戳：包含毫秒和微秒
        auto since_epoch = tp.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(since_epoch - seconds);
        
        int milliseconds = static_cast<int>(micros.count() / 1000);
        int microseconds = micros.count() % 1000;
        
        return std::format("[{:04d}/{:02d}/{:02d} {:02d}:{:02d}:{:02d}.{:03d}.{:03d}]",
            localTime.tm_year+1900, localTime.tm_mon+1, localTime.tm_mday,
            localTime.tm_hour, localTime.tm_min, localTime.tm_sec,
            milliseconds, microseconds);
    } else {
        // 普通时间戳
        return std::format("[{:04d}/{:02d}/{:02d} {:02d}:{:02d}:{:02d}]",
            localTime.tm_year+1900, localTime.tm_mon+1, localTime.tm_mday,
            localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    }
}

void logt::ensure_worker_started() {
    std::call_once(worker_flag_, []() {
        worker_ = std::thread(worker_thread);
    });
}
