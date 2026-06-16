/*
    File monitor module for SharedCppLib2.
    Tianming Wu <github.com/Tianming-Wu> 2026.03.26

    This module provides a simple file monitoring utility that allows users
    to monitor changes in files or directories. It can be used for various
    purposes, such as watching for configuration file changes, or implementing
    a simple hot-reload mechanism.

    This module does not use loop-and-watch approach, but instead uses the
    system's native file monitoring APIs on specific platforms.
*/

#pragma once

#include <filesystem>
#include <functional>
#include <chrono>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

#include "platform.hpp"
#include "basics.hpp"
#include "enum.hpp"


namespace scl2 {

// Monitor a single file or a directory (including subdirectories) for changes.
class filemon {
public:
    
    enum class EventType {
        Null = 0,
        Created = 1 << 0,
        Deleted = 1 << 1,
        Modified = 1 << 2,
        Renamed = 1 << 3,
        All = Created | Deleted | Modified | Renamed
    };
    scl2_enum_bitop_inclass(EventType)
    

    using callback_t = std::function<void(const std::filesystem::path& path, filemon::EventType type)>;

    filemon();
    ~filemon();

    disable_copy_move(filemon)

    filemon(const fs::path& path, callback_t callback, EventType eventFilter = EventType::All);

    void start();
    void stop();

private:
    std::thread monitorThread;

#ifdef OS_WINDOWS
    HANDLE hDir;
    OVERLAPPED overlapped;
    std::byte buffer[1024];

    HANDLE hStopEvent;


#else
    int inotifyFd;

    
#endif

};

scl2_bitenum_traits(filemon::EventType, 0, 4) // only 4 bits are used for event types


} // namespace scl2