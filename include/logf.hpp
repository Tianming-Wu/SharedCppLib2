#pragma once

#include <filesystem>
namespace fs = std::filesystem;

#include <ctime>
#include <cstdio>
#include <fstream>
#include <format>

#include "logd.hpp"

namespace logfd {

enum loglevel {
    verbose, debug, info, warn, error
};

class logf {
    fs::path path;
    std::ofstream ofs;

public:
    logf();
    logf(std::string);
    logf(fs::path);

    /// @brief Write raw string content directly to file.
    /// @param str String to write
    void write(std::string);
    void writeln(std::string);

    void log(loglevel, std::string);
    void log(loglevel, std::string, std::initializer_list<variant>);

    static std::string timestamp();
    static std::string timestamp_microsecond();

protected:
    std::string unpack(std::string, std::initializer_list<variant>);
    std::string level(loglevel);

};

};