/*
    File IO operation module for SharedCppLib2.
    Tianming <github.com/Tianming-Wu> 2026.03.27

    This module does nothing different than the fstream in standard
    library, but it supports the compatible layer provided by api.
*/

#pragma once

#include <fstream>
#include <filesystem>

#include "api.hpp"
#include "bytearray.hpp" // You need to link basic anyway, this is also in basic
#include "enum.hpp"
#include "macros.hpp"

namespace fs = std::filesystem;

namespace scl2 {

enum class file_mode_flag {
    Read = 1 << 0,
    Write = 1 << 1,
    Append = 1 << 2,
    Text = 0 << 3, // default is text mode
    Binary = 1 << 3,
    Truncate = 1 << 4, // Only valid when Write is set, otherwise it will be ignored.
    Create = 1 << 5, // Only valid when Write is set, otherwise it will be ignored. Creates the file if it does not exist.

    ReadWrite = Read | Write,

    Default = Read
};

scl2_bitenum_op(file_mode_flag)


class fileio {
public:
    default_constructor_destructor(fileio)

    fileio(const std::string& path, file_mode_flag mode = file_mode_flag::Default);

private:
    std::fstream fileStream;

    template<typename T>
    requires (requires(const T& t) { fileStream << t; } && !std::is_same_v<T, std::bytearray>)
    // avoid collision with bytearray (which has an << operator)
    size_t write(const T& data) {
        fileStream << data;
        return sizeof(T);
    }

    size_t write(const std::bytearray& data);
    size_t write(const std::bytearray& data, size_t count);

    template<typename T>
    requires ::scl2::has_generic_serialize<T>
    size_t write(const T& data) {
        return write(generic_serialize(data));
    }

    template<typename T>
    requires ::scl2::has_generic_dump<T>
    size_t write(const T& data) {
        return write(generic_dump(data));
    }


    bool good() const;
    void close();

};


// This is the truly powerful part of SharedCppLib2's new generic api.
// A single line i/o! How cool is that!

template<typename T>
requires ::scl2::has_generic_serialize<T>
size_t writeFile(const fs::path& path, const T& data) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Failed to open file for writing: " + path.string());
    }
    std::string sei = scl2::generic_serialize(data);
    ofs << sei;
    return sei.size();
}

template<typename T>
requires ::scl2::has_generic_dump<T>
size_t writeFile(const fs::path& path, const T& data) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Failed to open file for writing: " + path.string());
    }
    std::bytearray ba = scl2::generic_dump(data);
    ofs << ba;
    return ba.size();
}

size_t writeFile(const fs::path& path, const std::bytearray& data);

template<typename T>
requires ::scl2::has_generic_load<T>
T readAndLoad(const fs::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open file for reading: " + path.string());
    }
    std::bytearray ba((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return scl2::generic_load<T>(ba);
}

std::bytearray readFile(const fs::path& path);

enum class sync_action : uint8_t { WriteFile, LoadFile, NoAction, Error };
using file_timestamp = std::filesystem::file_time_type;
namespace { std::strong_ordering compareFileTimestamp(file_timestamp src_tm, const fs::path& target); }

// This function provides a simple syncronizing machanism for configuration-like files.
// It only compares the timestamps of the source (data in memory) and the target (file
// on disk).
// You need to provide a callback. You can handle the update in the callback. You MUST
// update the timestamp in memory to the returned value once the operation is successful,
// otherwise this function does not work as expected.
template<typename T>
requires ::scl2::has_generic_dump<T> && ::scl2::has_generic_load<T>
bool syncWith(const fs::path& path, T& data, const file_timestamp& src_tm, const std::function<void(sync_action,file_timestamp)>& updateCallback)
{
    auto cmp_result = compareFileTimestamp(src_tm, path);
    if (cmp_result == std::strong_ordering::greater) {
        // src is newer, write to file
        writeFile(path, data);
        if (updateCallback) {
            updateCallback(sync_action::WriteFile, src_tm);
        }
        return true;
    } else if (cmp_result == std::strong_ordering::less) {
        // target is newer, load from file
        data = readAndLoad<T>(path);
        if (updateCallback) {
            // get the latest timestamp
            file_timestamp latest_tm;
            latest_tm = fs::last_write_time(path);
            updateCallback(sync_action::LoadFile, latest_tm);
        }
        return true;
    } else if (cmp_result == std::strong_ordering::equal) {
        // same timestamp, do nothing
        if (updateCallback) {
            updateCallback(sync_action::NoAction, src_tm);
        }
        return true;
    } else {
        // error comparing timestamps
        if (updateCallback) {
            updateCallback(sync_action::Error, 0);
        }
        return false;
    }
}

} // namespace scl2