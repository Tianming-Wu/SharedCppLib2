#include "logf.hpp"

namespace logfd {

logf::logf()
    :path(fs::read_symlink("/proc/self/exe").filename()/".log"), ofs(path)
{}

logf::logf(std::string filename)
    :path(filename), ofs(path)
{}

logf::logf(fs::path path)
    :path(path), ofs(path)
{}


void logf::write(std::string str) {
    ofs << str;
}

void logf::writeln(std::string str) {
    ofs << str << std::endl;
}

void logf::log(loglevel lv, std::string str) {
    ofs << "[" << timestamp() << "] [" << level(lv) << "] " << str << std::endl;
}

void logf::log(loglevel lv, std::string str, std::initializer_list<variant> l) {
    ofs << "[" << timestamp() << "] [" << level(lv) << "] " << unpack(str, l) << std::endl;
}

std::string logf::unpack(std::string src, std::initializer_list<variant> l) {
    std::vector<std::string> d;
    for(const variant &t : l) d.push_back(t.toString());
    size_t lpos = 0, pos, tmax = l.size();
    while((pos = src.find('$', lpos)) != std::string::npos) {
        if(pos+1 < src.length() && src[pos+1] == '$') {
            src.replace(pos, 2, "$");
            lpos = pos + 1;
            continue;
        }
        std::string rp;
        try {
            int t = std::stoi(src.substr(pos, 1));
            if(t > tmax) rp = "";
                else rp = d[t];
        } catch(...) {
            rp = "";
        }

        src.replace(pos, 2, rp);
        lpos = pos;
    }
    return src;
}

std::string logf::level(loglevel lv) {
    std::string lvs;
    switch(lv) {
        case verbose: { lvs = "VERBOSE"; break; }
        case debug: { lvs = "DEBUG"; break; }
        case info: { lvs = "INFO"; break; }
        case warn: { lvs = "WARN"; break; }
        case error: { lvs = "ERROR"; break; }
    }
    return lvs;
}

std::string logf::timestamp() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
#ifdef _WIN32
    localtime_s(&localTime, &currentTime);
#else
    localtime_r(&currentTime, &localTime);
#endif

    return std::format("{:04d}/{:02d}/{:02d} {:02d}:{:02d}:{:02d}",
        localTime.tm_year+1900, localTime.tm_mon+1, localTime.tm_mday,
        localTime.tm_hour, localTime.tm_min, localTime.tm_sec
    );
}

std::string logf::timestamp_microsecond() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
#ifdef _WIN32
    localtime_s(&localTime, &currentTime);
#else
    localtime_r(&currentTime, &localTime);
#endif
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count() % 1000000;
    return std::format("{:04d}/{:02d}/{:02d} {:02d}:{:02d}:{:02d}.{:06}",
        localTime.tm_year+1900, localTime.tm_mon+1, localTime.tm_mday,
        localTime.tm_hour, localTime.tm_min, localTime.tm_sec,
        microseconds
    );
}

};