/*
    File IO operation module for SharedCppLib2.
    Tianming <github.com/Tianming-Wu> 2026.03.27

    This module does nothing different than the fstream in standard
    library, but it supports the compatible layer provided by api.
*/

#pragma once

#include <fstream>
#include <filesystem>
#include <system_error>
#include <type_traits>

#include "api.hpp"
#include "stringlist.hpp"
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
    requires (requires(const T& t) { fileStream << t; } && !std::is_same_v<T, scl2::bytearray>)
    // avoid collision with bytearray (which has an << operator)
    size_t write(const T& data) {
        fileStream << data;
        return sizeof(T);
    }

    size_t write(const scl2::bytearray& data);
    size_t write(const scl2::bytearray& data, size_t count);

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


/// @brief Push everything written for `path` out to the disk.
/// @note Writing a file normally only hands the bytes to the operating system, which keeps
///       them in a cache and writes them back later. If the machine loses power before
///       that happens, the file can end up empty or half written even though the write
///       call returned successfully. Call this when the file has to survive that, for
///       instance before renaming it over another file. Throws if the flush fails.
void flushFile(const fs::path& path);

/// @brief Move @p from onto @p to, replacing whatever is there, in one operating system call.
/// @note This is the second half of a safe write: put everything into a temporary file, then
///       replace the real one with it. Because the replacement is a single call, a reader —
///       another process, or this one after a restart — sees either the whole old file or the
///       whole new one, never a half written mix. Both paths have to be on the same volume
///       for this to be a rename rather than a copy, which is why the temporary is written
///       next to its target.
/// @note On Windows the call does not return until the change has reached the disk
///       (`MOVEFILE_WRITE_THROUGH`). Everywhere else it is `rename`, which is atomic, but the
///       directory entry itself is not flushed — a power loss can still undo it.
/// @throws std::runtime_error if the replacement fails; @p from is left where it was.
void replaceFile(const fs::path& from, const fs::path& to);

// This is the truly powerful part of SharedCppLib2's new generic api.
// A single line i/o! How cool is that!

// Genaric Single-line Call for Types that support generic_serialize protocol.
template<typename T>
requires ::scl2::has_generic_serialize<T>
size_t writeFile(const fs::path& path, const T& data, bool flush = false) {
    std::string sei = scl2::generic_serialize(data);
    {
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs) {
            throw std::runtime_error("Failed to open file for writing: " + path.string());
        }
        ofs << sei;
    }
    if (flush) scl2::flushFile(path);
    return sei.size();
}

// Generic Single-line Call for Types that support generic_dump protocol.
template<typename T>
requires ::scl2::has_generic_dump<T>
size_t writeFile(const fs::path& path, const T& data, bool flush = false) {
    scl2::bytearray ba = scl2::generic_dump(data);
    {
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs) {
            throw std::runtime_error("Failed to open file for writing: " + path.string());
        }
        ofs << ba;
    }
    if (flush) scl2::flushFile(path);
    return ba.size();
}

template<typename T>
requires std::is_trivially_assignable<T, T>::value && std::is_trivially_copyable<T>::value
size_t writeAs(const fs::path& path, const T& data, bool flush = false) {
    {
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs) {
            throw std::runtime_error("Failed to open file for writing: " + path.string());
        }
        ofs.write(reinterpret_cast<const char*>(&data), sizeof(T));
    }
    if (flush) scl2::flushFile(path);
    return sizeof(T);
}

// Single-line Call for writing a bytearray to file.
size_t writeFile(const fs::path& path, const scl2::bytearray& data, bool flush = false);

// Single-line Call for writing a string to file.
size_t writeFile(const fs::path& path, const std::string& data, bool flush = false);

// writeFile(), but through a temporary file that then replaces the target, so a run that
// dies halfway leaves the previous file untouched instead of a truncated one.
//
// The temporary is `path` + ".tmp", next to the target, and is removed again if anything
// fails. A crash leaves it behind; nothing here scans for it afterwards.
//
// `flush` defaults to true, unlike writeFile(): replacing a file with data that has not
// reached the disk yet is the very thing this exists to avoid.

namespace detail {
/// @brief Write through `path` + ".tmp" and replace `path` with it.
/// @param write Called with the temporary path; it has to put the data there.
/// @return Whatever @p write returned.
/// @throws Whatever @p write, the flush or the replace throws, after removing the temporary.
template<typename WriteFn>
inline size_t atomicWriteFile(const fs::path& path, bool flush, WriteFn&& write) {
    fs::path tmp = path;
    tmp += ".tmp";

    try {
        const size_t written = write(tmp);
        if (flush) flushFile(tmp);
        replaceFile(tmp, path);
        return written;
    }
    catch (...) {
        // The temporary is ours, and it has no value once the replacement failed.
        std::error_code ignored;
        fs::remove(tmp, ignored);
        throw;
    }
}
} // namespace detail

template<typename T>
requires ::scl2::has_generic_serialize<T>
size_t writeFileAtomic(const fs::path& path, const T& data, bool flush = true) {
    return detail::atomicWriteFile(path, flush, [&](const fs::path& tmp) {
        return writeFile(tmp, data);
    });
}

template<typename T>
requires ::scl2::has_generic_dump<T>
size_t writeFileAtomic(const fs::path& path, const T& data, bool flush = true) {
    return detail::atomicWriteFile(path, flush, [&](const fs::path& tmp) {
        return writeFile(tmp, data);
    });
}

// Single-line Call for writing a bytearray to file, through a temporary and a replace.
size_t writeFileAtomic(const fs::path& path, const scl2::bytearray& data, bool flush = true);

// Single-line Call for writing a string to file, through a temporary and a replace.
size_t writeFileAtomic(const fs::path& path, const std::string& data, bool flush = true);

template<typename T>
requires ::scl2::has_generic_load<T>
T readAndLoad(const fs::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open file for reading: " + path.string());
    }
    // Note: a char iterator range does not fit bytearray's range constructor — bytearray holds
    // std::byte, and a char does not convert to it. Read the stream instead.
    scl2::bytearray ba;
    ba.readAllFromStream(ifs);
    return scl2::generic_load<T>(ba);
}

template<typename T>
requires std::is_trivially_copyable_v<T>
T readAs(const fs::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open file for reading: " + path.string());
    }
    T data;
    ifs.read(reinterpret_cast<char*>(&data), sizeof(T));
    return data;
}

scl2::bytearray readFile(const fs::path& path);

// Read a range of bytes from a file. If length is -1, read until the end of the file.
scl2::bytearray readFileRange(const fs::path& path, size_t offset = 0, size_t length = -1);

scl2::string readFileAsString(const fs::path& path);

// Syncing system:
// automatically syncs the data in memory and the file on disk based on timestamps.
// Warning: This is not thread-safe, nor process-safe. You need to implement your own locking
// machanism if you want to use this in a multi-threaded or multi-process environment.

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
            updateCallback(sync_action::Error, file_timestamp{});
        }
        return false;
    }
}

// Line based text file helpers.

/// @brief Read the file and process each line.
/// @note In your lambda, return false to stop processing, and true to continue.
/// @return The number of lines processed.
/// @throws std::runtime_error if the file cannot be opened.
unsigned int foreachLine(const fs::path& path, const std::function<bool(unsigned int, const std::string&)>& func);

/// @brief Read all lines from a text file and return them as a stringlist.
/// @param path 
/// @return The stringlist, each element is a line from the file
/// @note Does not include newline characters, and does not remove empty lines.
/// @throws std::runtime_error if the file cannot be opened.
scl2::stringlist readAllLines(const fs::path& path);



} // namespace scl2