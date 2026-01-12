#pragma once
#include <platform.hpp>
#include <stringlist.hpp>

#include <filesystem>
#include <chrono>
#include <mutex>

namespace fs = std::filesystem;

#undef stdin
#undef stdout

// RAII for handles
struct process_stream
{

    process_stream();
    ~process_stream();

    // disable copy
    disable_copy(process_stream)

    // move support
    process_stream(process_stream&& another);
    process_stream& operator=(process_stream&& another);

    bool valid() const;

    bool write(const std::bytearray& data);
    std::bytearray read(size_t bytesToRead);
    std::bytearray readAll();

    bool available() const;
    size_t bytesAvailable() const;

    void setup(STARTUPINFOA &si);
    void release();

protected:
    HANDLE m_stdin_write, m_stdout_read, m_stderr_read;
    HANDLE m_stdin_read, m_stdout_write, m_stderr_write;
    bool m_valid;
};

class process
{
public:

    enum options {
        Detached
    };

    process();
    explicit process(const fs::path& proc, const std::stringlist& arguments = std::stringlist());
    ~process();

    disable_copy(process)
    enable_move(process)

    void setPath(const fs::path& path);
    fs::path path() const;

    void setArguments(const std::stringlist& args);
    std::stringlist arguments() const;

    bool start();
    static bool start(const fs::path& proc, const std::stringlist& arguments = std::stringlist());

    bool running();

    void detach();
    void terminate();
    void kill();

    bool finished();
    bool readyRead();

    bool waitForFinished(std::chrono::milliseconds timeout = std::chrono::seconds(5));
    bool waitForReadyRead(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    std::string readAll();
    std::string readAllStdOutput();
    std::string readAllStdError();

    bool write(const std::string data);

    int exitcode() const;

#ifdef OS_WINDOWS
    DWORD processId() const;
#else
    pid_t processId() const;
#endif

private:
    bool createPipes();
    void cleanup();

private:
    fs::path m_path;
    std::stringlist m_args;
    bool active;

    options m_flags;

    mutable std::mutex mutex;

    struct ProcData m_procData;

    process_stream m_procStream;
    // process_stream istream, ostream, estream;
}; 