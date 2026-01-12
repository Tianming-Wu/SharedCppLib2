#include "stream.hpp"

#include <thread>
#include <chrono>

bool basic_sclistream::waitForReadyRead(std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    constexpr auto poll_interval = std::chrono::milliseconds(10);
    
    // Poll readyRead() until timeout
    do {
        if (readyRead()) {
            return true;
        }
        std::this_thread::sleep_for(poll_interval);
    } while (std::chrono::steady_clock::now() < deadline);
    
    // Final check at deadline
    return readyRead();
}
