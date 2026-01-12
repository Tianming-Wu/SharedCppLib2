#include "process.hpp"

#include <condition_variable>

struct ProcData {
#ifdef OS_WINDOWS
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    DWORD pid;

#else
    pid_t pid;

#endif

    bool running = false;
    int exitCode = -1;
    std::string lastError;
};


process::process()
{
    m_procData.si.cb = sizeof(STARTUPINFOA);
}

process::process(const fs::path &proc, const std::stringlist& arguments)
    : m_path(proc), m_args(arguments)
{
    m_procData.si.cb = sizeof(STARTUPINFOA);
}

process::~process()
{
    terminate();
    cleanup(); // close all handles
}

void process::setPath(const fs::path &path) { m_path = path; }
fs::path process::path() const { return m_path; }
void process::setArguments(const std::stringlist& args) { m_args = args; }
std::stringlist process::arguments() const { return m_args; }

bool process::start()
{
    if (m_procData.running) {
        m_procData.lastError = "Process is already running";
        return false;
    }

    STARTUPINFOA &si = m_procData.si;
    PROCESS_INFORMATION &pi = m_procData.pi;
    
    m_procStream.setup(si);
    
    if(!fs::exists(m_path)) throw std::runtime_error("Process executable does not exists.");

    std::string cmdLine = m_path.string();
    if(!m_args.empty()) {
        cmdLine += " " + m_args.xjoin();
    }

    char* cmdLinePtr = new char[cmdLine.size() + 1];
    strcpy(cmdLinePtr, cmdLine.c_str());

    m_procData.running = CreateProcessA(nullptr, cmdLinePtr,
        nullptr, nullptr,
        TRUE,
        0,
        nullptr, nullptr,
        &si, &pi
    );

    delete[] cmdLinePtr;

    if(!m_procData.running) {
        m_procData.lastError = "CreateProcess failed: " + platform::windows::TranslateLastError();
        cleanup();
        return false;
    }

    m_procStream.release();
    
    return true;
}

bool process::running()
{
    # ifdef OS_WINDOWS
    if (!m_procData.running || !m_procData.pi.hProcess) {
        return false;
    }
    
    DWORD exitCode = 0;
    GetExitCodeProcess(m_procData.pi.hProcess, &exitCode);
    return exitCode == STILL_ACTIVE;

    # else
    # endif
}

void process::cleanup()
{
    # ifdef OS_WINDOWS
    // auto closeHandle = [](HANDLE& h) {
    //     if (h && h != INVALID_HANDLE_VALUE) {
    //         CloseHandle(h);
    //         h = nullptr;
    //     }
    // };

    // reset procStream

    m_procStream.~process_stream();
    m_procStream = process_stream();

    # else
    # endif
}

void process::terminate()
{
    if (m_procData.running && m_procData.pi.hProcess) {
        TerminateProcess(m_procData.pi.hProcess, 0);
        waitForFinished();
    }
}

void process::kill() { terminate(); }

bool process::finished() {
    if (!m_procData.running || !m_procData.pi.hProcess) {
        return false;  // 从未启动过，不算 finished
    }
    
    DWORD exitCode = 0;
    if (GetExitCodeProcess(m_procData.pi.hProcess, &exitCode)) {
        return exitCode != STILL_ACTIVE;
    }
    
    return false;  // 获取退出码失败
}

bool process::readyRead() {
    return m_procStream.available();
}

#undef min
#undef max

bool process::waitForFinished(std::chrono::milliseconds timeout) {
    if (!m_procData.running || !m_procData.pi.hProcess) {
        return true;
    }
    
    if (timeout == std::chrono::milliseconds::max()) {
        # ifdef OS_WINDOWS
        WaitForSingleObject(m_procData.pi.hProcess, INFINITE);
        # else
        # endif
    } else {
        # ifdef OS_WINDOWS
        WaitForSingleObject(m_procData.pi.hProcess, timeout.count());
        # else
        # endif
    }
    
    DWORD exitCode = 0;
    if (GetExitCodeProcess(m_procData.pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
        m_procData.running = false;
        m_procData.exitCode = static_cast<int>(exitCode);
        return true;
    }
    
    return false;
}

bool process::waitForReadyRead(std::chrono::milliseconds timeout)
{
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (m_procStream.available()) {
            return true;
        }
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= timeout) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

bool process::write(const std::string data)
{
    return false;
}

int process::exitcode() const
{
    return m_procData.exitCode;
}

# ifdef OS_WINDOWS
DWORD process::processId() const { return m_procData.pid; }
# else
pid_t processId() const { return m_procData.pid; }
# endif



// RAII process_stream

process_stream::process_stream()
    : m_stdin_write(nullptr), m_stdout_read(nullptr), m_stderr_read(nullptr), m_valid(true)
{
    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};

    HANDLE stdin_read, stdout_write, stderr_write;

    if(!CreatePipe(&m_stdin_read,  &m_stdin_write,  &sa, 0)) m_valid = false;
    if(!CreatePipe(&m_stdout_read, &m_stdout_write, &sa, 0)) m_valid = false;
    if(!CreatePipe(&m_stderr_read, &m_stderr_write, &sa, 0)) m_valid = false;

    SetHandleInformation(m_stdin_write, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(m_stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(m_stderr_read, HANDLE_FLAG_INHERIT, 0);
}

process_stream::~process_stream()
{
    auto closeHandle = [](HANDLE& h) {
        if (h && h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            h = nullptr;
        }
    };

    closeHandle(m_stdin_write);
    closeHandle(m_stdout_read);
    closeHandle(m_stderr_read);

    closeHandle(m_stdin_read);
    closeHandle(m_stdout_write);
    closeHandle(m_stderr_write);
}

process_stream::process_stream(process_stream && another)
    : m_stdin_write(m_stdin_write), m_stdout_read(m_stdout_read), m_stderr_read(m_stderr_read)
    , m_stdin_read(m_stdin_read), m_stdout_write(m_stdout_write), m_stderr_write(m_stderr_write) 
    , m_valid(m_valid)
{
    another.m_stdin_write = another.m_stdout_read = another.m_stderr_read = nullptr;
    another.m_stdin_read = another.m_stdout_write = another.m_stderr_write = nullptr;
    another.m_valid = false;
}

process_stream &process_stream::operator=(process_stream && another)
{
    m_stdin_write   = another.m_stdin_write;
    m_stdout_read   = another.m_stdout_read;
    m_stderr_read   = another.m_stderr_read;
    m_stdin_read    = another.m_stdin_read;
    m_stdout_write  = another.m_stdout_write;
    m_stderr_write  = another.m_stderr_write;
    m_valid         = another.m_valid;

    another.m_stdin_write = another.m_stdout_read = another.m_stderr_read = nullptr;
    another.m_stdin_read = another.m_stdout_write = another.m_stderr_write = nullptr;
    another.m_valid = false;
}

bool process_stream::valid() const
{
    return m_valid;
}

bool process_stream::write(const std::bytearray &data)
{
    DWORD bytesWritten = 0;
    if (!WriteFile(m_stdin_write, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, nullptr)) {
        return false;
    }

    return bytesWritten == data.size();
}

std::bytearray process_stream::read(size_t bytesToRead)
{
    DWORD readBytes = std::min(bytesAvailable(), bytesToRead);

    std::bytearray result(readBytes, std::byte(0));
    DWORD bytesRead = 0;
    ReadFile(m_stdout_read, result.data(), readBytes, &bytesRead, nullptr);

    if (bytesRead < readBytes) {
        result.resize(bytesRead);
    }

    return result;
}

std::bytearray process_stream::readAll()
{
    return std::bytearray();
}

bool process_stream::available() const {
    return bytesAvailable() != 0;
}

size_t process_stream::bytesAvailable() const
{
    DWORD bytesAvailable = 0;
    if (!PeekNamedPipe(m_stdout_read, nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
        return 0;
    }

    return bytesAvailable;
}

void process_stream::setup(STARTUPINFOA &si)
{
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = m_stdin_read;
    si.hStdOutput = m_stdout_write;
    si.hStdError = m_stderr_write;
}

void process_stream::release()
{
    CloseHandle(m_stdin_read);
    CloseHandle(m_stdout_write);
    CloseHandle(m_stderr_write);
    m_stdin_read = m_stdout_write = m_stderr_write = nullptr;
}
