#include "process.hpp"

// Pulled in first: on Windows platform.hpp sets NOMINMAX (and deliberately not
// WIN32_LEAN_AND_MEAN) before it includes windows.h, which is why this file has
// no #undef min / #undef max of its own. On Unix it just gives us the OS_* macros.
#include "platform.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#ifndef OS_WINDOWS
    // platform.hpp only brings in unistd.h and sys/wait.h on its own. The plain
    // C <signal.h> is deliberate: we need the POSIX additions (sigprocmask and
    // friends), which <csignal> is not required to put in the global namespace.
    #include <cerrno>
    #include <fcntl.h>
    #include <poll.h>
    #include <signal.h>
    #include <sys/ioctl.h>
    #include <sys/wait.h>
    #include <time.h>
#endif

namespace scl2 {

// ── helpers shared by both platforms ─────────────────────────────────

namespace {

using clock_type = std::chrono::steady_clock;

/// Timeouts at or beyond this count as "wait forever".
constexpr auto infinite_timeout = std::chrono::hours(24 * 365);

inline bool is_infinite(std::chrono::milliseconds timeout)
{
    return timeout >= infinite_timeout;
}

inline clock_type::time_point make_deadline(std::chrono::milliseconds timeout)
{
    if (is_infinite(timeout)) return clock_type::time_point::max();
    if (timeout <= std::chrono::milliseconds::zero()) return clock_type::now();
    return clock_type::now() + timeout;
}

/// @brief How long we may block in one go, capped so that we keep re-checking
///        the pipes. Returns 0 once the deadline has passed.
inline int wait_slice(clock_type::time_point deadline, std::chrono::milliseconds cap)
{
    using ms = std::chrono::milliseconds;

    if (deadline == clock_type::time_point::max()) {
        return static_cast<int>(cap.count());
    }

    const auto left = std::chrono::duration_cast<ms>(deadline - clock_type::now());
    if (left <= ms::zero()) return 0;

    return static_cast<int>(std::max(std::min(left, cap).count(), 1LL));
}

/// @brief Throw away the first @p count bytes of a buffer.
void drop_front(scl2::bytearray& data, size_t count)
{
    if (count >= data.size()) {
        data.clear();
        return;
    }

    const size_t rest = data.size() - count;
    std::memmove(data.data(), data.data() + count, rest);
    data.resize(rest);
}

/// @brief Pull at most @p count bytes off the front of a buffer.
scl2::bytearray take_front(scl2::bytearray& data, size_t count)
{
    const size_t amount = std::min(count, data.size());
    if (amount == 0) return scl2::bytearray();

    scl2::bytearray result(data.data(), amount);
    drop_front(data, amount);
    return result;
}

} // namespace


// ── platform primitives ──────────────────────────────────────────────

#ifdef OS_WINDOWS

namespace {

using native_handle = process_stream::native_handle_type;

constexpr native_handle invalid_handle = process_stream::invalid_handle;

inline bool handle_valid(native_handle h) { return h != invalid_handle; }

inline HANDLE as_handle(native_handle h) { return reinterpret_cast<HANDLE>(h); }

inline void close_handle(native_handle& h)
{
    if (handle_valid(h)) {
        ::CloseHandle(as_handle(h));
        h = invalid_handle;
    }
}

/// @brief Ask the pipe how much data is waiting.
/// @return false when the pipe is broken, meaning every writer has gone away.
inline bool peek_bytes(native_handle h, size_t& count)
{
    DWORD available = 0;
    if (!::PeekNamedPipe(as_handle(h), nullptr, 0, nullptr, &available, nullptr)) {
        count = 0;
        return false;
    }
    count = available;
    return true;
}

/// Nothing to do on Windows: a child that we no longer track simply becomes an
/// orphan the system reaps, and CloseHandle() is all the bookkeeping there is.
inline void reap_if_exited(std::int64_t) {}

/// @brief Make a pipe whose read end stays with us and whose write end goes to
///        the child.
bool create_out_pipe(native_handle& parent_read, native_handle& child_write)
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_end = nullptr;
    HANDLE write_end = nullptr;
    if (!::CreatePipe(&read_end, &write_end, &sa, 0)) return false;

    // Only the child's end may be inherited, otherwise the child never sees EOF
    // on its input and our own reads never see the pipe break.
    ::SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);

    parent_read = reinterpret_cast<native_handle>(read_end);
    child_write = reinterpret_cast<native_handle>(write_end);
    return true;
}

/// @brief Make a pipe whose write end stays with us and whose read end goes to
///        the child.
bool create_in_pipe(native_handle& child_read, native_handle& parent_write)
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_end = nullptr;
    HANDLE write_end = nullptr;
    if (!::CreatePipe(&read_end, &write_end, &sa, 0)) return false;

    ::SetHandleInformation(write_end, HANDLE_FLAG_INHERIT, 0);

    child_read = reinterpret_cast<native_handle>(read_end);
    parent_write = reinterpret_cast<native_handle>(write_end);
    return true;
}

} // namespace

#else  // OS_UNIX / OS_ANDROID

namespace {

using native_handle = process_stream::native_handle_type;

constexpr native_handle invalid_handle = process_stream::invalid_handle;

/// Descriptors 0, 1 and 2 are the child's standard streams.
constexpr int reserved_fd_count = 3;

/// The exit code the child uses when exec() never got off the ground.
constexpr int exec_failed_exit_code = 127;

inline bool handle_valid(native_handle h) { return h != invalid_handle; }

inline int as_fd(native_handle h) { return static_cast<int>(h); }

inline void close_handle(native_handle& h)
{
    if (handle_valid(h)) {
        ::close(as_fd(h));
        h = invalid_handle;
    }
}

inline void close_fds(int fds[2])
{
    if (fds[0] >= 0) { ::close(fds[0]); fds[0] = -1; }
    if (fds[1] >= 0) { ::close(fds[1]); fds[1] = -1; }
}

inline bool peek_bytes(native_handle h, size_t& count)
{
    int pending = 0;
    if (::ioctl(as_fd(h), FIONREAD, &pending) < 0) {
        count = 0;
        return false;
    }
    count = pending > 0 ? static_cast<size_t>(pending) : 0;
    return true;
}

/// @brief Whether the write end of the pipe is gone.
/// FIONREAD cannot tell "empty" from "closed", so ask poll() instead: once every
/// writer has closed, the read end reports POLLHUP, together with POLLIN while
/// data is still buffered.
inline bool peer_closed(native_handle h)
{
    struct pollfd probe{};
    probe.fd = as_fd(h);
    probe.events = POLLIN;

    if (::poll(&probe, 1, 0) < 0) return true;
    if (probe.revents & (POLLERR | POLLNVAL)) return true;
    return (probe.revents & POLLHUP) != 0 && (probe.revents & POLLIN) == 0;
}

/// @brief Make a pipe. Every descriptor is close-on-exec, so a successful
///        exec() in the child cleans up by itself: the end the child needs is
///        moved onto a standard descriptor by dup2(), which clears the flag.
/// @param keep_read_end true when our end is the reading one. Read ends are made
///        non-blocking, since we always peek before taking anything; the write
///        end stays blocking so that write() matches the Windows behaviour.
bool create_pipe(int fds[2], bool keep_read_end)
{
    if (::pipe(fds) != 0) return false;

    ::fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(fds[1], F_SETFD, FD_CLOEXEC);

    if (keep_read_end) {
        const int flags = ::fcntl(fds[0], F_GETFL, 0);
        if (flags >= 0) ::fcntl(fds[0], F_SETFL, flags | O_NONBLOCK);
    }
    return true;
}

/// @brief Pick up a child that has already exited, so that it does not linger as
///        a zombie. Never blocks.
inline void reap_if_exited(std::int64_t pid)
{
    if (pid > 0) ::waitpid(static_cast<pid_t>(pid), nullptr, WNOHANG);
}

/// @brief write(2) that cannot take the whole process down.
/// Writing to a pipe whose reader is gone raises SIGPIPE, and the default action
/// for SIGPIPE is to terminate the process. Windows has no such signal, so a
/// library must not let a child that went away kill its parent on Unix: block
/// SIGPIPE around the call, swallow the pending signal, and report the EPIPE as
/// a plain write failure instead.
inline ssize_t write_ignoring_sigpipe(int fd, const void* data, size_t size)
{
    sigset_t blocked{};
    sigset_t previous{};
    ::sigemptyset(&blocked);
    ::sigaddset(&blocked, SIGPIPE);
    ::sigprocmask(SIG_BLOCK, &blocked, &previous);

    const ssize_t written = ::write(fd, data, size);

    if (written < 0 && errno == EPIPE) {
        // Take the signal we just blocked away, or it would fire the moment the
        // mask is restored.
        struct timespec immediately{0, 0};
        ::sigtimedwait(&blocked, nullptr, &immediately);
    }

    ::sigprocmask(SIG_SETMASK, &previous, nullptr);
    return written;
}

} // namespace

#endif


// ── process_stream: platform independent ─────────────────────────────

process_stream::process_stream() = default;

process_stream::process_stream(native_handle_type handle, bool readable, bool writable)
    : m_handle(handle)
    , m_readable(readable)
    , m_writable(writable)
{}

process_stream::~process_stream()
{
    close_handle(m_handle);
}

process_stream::process_stream(process_stream&& another) noexcept
    : m_handle(std::exchange(another.m_handle, invalid_handle))
    , m_readable(std::exchange(another.m_readable, false))
    , m_writable(std::exchange(another.m_writable, false))
    , m_at_end(std::exchange(another.m_at_end, true))
{}

process_stream& process_stream::operator=(process_stream&& another) noexcept
{
    if (this == &another) return *this;

    close_handle(m_handle);
    m_handle = std::exchange(another.m_handle, invalid_handle);
    m_readable = std::exchange(another.m_readable, false);
    m_writable = std::exchange(another.m_writable, false);
    m_at_end = std::exchange(another.m_at_end, true);
    return *this;
}

bool process_stream::valid()
{
    return handle_valid(m_handle);
}

bool process_stream::readyRead()
{
    return available() > 0;
}

scl2::bytearray process_stream::readAll()
{
    scl2::bytearray result;

    for (;;) {
        const size_t count = available();
        if (count == 0) break;

        scl2::bytearray chunk = read(count);
        if (chunk.size() == 0) break;

        result.append(chunk);
    }

    return result;
}

bool process_stream::reset()
{
    close_handle(m_handle);
    m_readable = false;
    m_writable = false;
    m_at_end = true;
    return true;
}

process_stream::native_handle_type process_stream::nativeHandle() const
{
    return m_handle;
}

bool process_stream::readable() const
{
    return m_readable;
}

bool process_stream::writable() const
{
    return m_writable;
}


// ── process_stream: platform specific ────────────────────────────────

#ifdef OS_WINDOWS

size_t process_stream::available()
{
    if (!m_readable || !handle_valid(m_handle)) return 0;

    size_t count = 0;
    if (!peek_bytes(m_handle, count)) {
        m_at_end = true;
        return 0;
    }
    return count;
}

scl2::bytearray process_stream::read(size_t bytes)
{
    if (!m_readable || !handle_valid(m_handle)) return scl2::bytearray();

    const size_t amount = std::min(available(), bytes);
    if (amount == 0) return scl2::bytearray();

    scl2::bytearray result(amount, std::byte(0));
    DWORD got = 0;
    if (!::ReadFile(as_handle(m_handle), result.data(), static_cast<DWORD>(amount), &got, nullptr)) {
        m_at_end = true;
        return scl2::bytearray();
    }

    if (got < amount) result.resize(got);
    return result;
}

size_t process_stream::write(const scl2::bytearray& data)
{
    if (!m_writable || !handle_valid(m_handle) || data.size() == 0) return 0;

    DWORD written = 0;
    if (!::WriteFile(as_handle(m_handle), data.data(),
                     static_cast<DWORD>(data.size()), &written, nullptr)) {
        return 0;
    }
    return written;
}

bool process_stream::atEnd() const
{
    if (!m_readable || !handle_valid(m_handle)) return true;
    if (m_at_end) return true;

    size_t count = 0;
    if (!peek_bytes(m_handle, count)) {
        m_at_end = true;
        return true;
    }
    return false;
}

bool process_stream::waitForReadyRead(std::chrono::milliseconds timeout)
{
    if (!m_readable || !handle_valid(m_handle)) return false;

    const auto deadline = make_deadline(timeout);

    for (;;) {
        if (available() > 0) return true;
        if (m_at_end) return false;

        // An anonymous pipe read handle becomes signalled once data arrives
        // (Windows 8 and later), and always once the writer closes. Waiting on
        // it is therefore the closest we get to a PeekNamedPipe with timeout;
        // the cap keeps the loop alive on any system where it never fires.
        const int slice = wait_slice(deadline, std::chrono::milliseconds(10));
        if (slice == 0) return false;

        ::WaitForSingleObject(as_handle(m_handle), static_cast<DWORD>(slice));
    }
}

#else  // OS_UNIX / OS_ANDROID

size_t process_stream::available()
{
    if (!m_readable || !handle_valid(m_handle)) return 0;

    size_t count = 0;
    if (!peek_bytes(m_handle, count)) {
        m_at_end = true;
        return 0;
    }
    return count;
}

scl2::bytearray process_stream::read(size_t bytes)
{
    if (!m_readable || !handle_valid(m_handle)) return scl2::bytearray();

    const size_t amount = std::min(available(), bytes);
    if (amount == 0) return scl2::bytearray();

    scl2::bytearray result(amount, std::byte(0));
    const ssize_t got = ::read(as_fd(m_handle), result.data(), amount);

    if (got < 0) {
        // EAGAIN cannot happen because we peeked first, EINTR is worth another
        // try later; anything else means the descriptor is unusable.
        if (errno != EINTR && errno != EAGAIN) m_at_end = true;
        return scl2::bytearray();
    }
    if (got == 0) {
        m_at_end = true;  // every writer is gone
        return scl2::bytearray();
    }

    if (static_cast<size_t>(got) < amount) result.resize(static_cast<size_t>(got));
    return result;
}

size_t process_stream::write(const scl2::bytearray& data)
{
    if (!m_writable || !handle_valid(m_handle) || data.size() == 0) return 0;

    // A child that closed its input must not take us down with SIGPIPE; the
    // failure simply shows up as "nothing was written".
    const ssize_t written = write_ignoring_sigpipe(as_fd(m_handle), data.data(), data.size());
    if (written < 0) return 0;
    return static_cast<size_t>(written);
}

bool process_stream::atEnd() const
{
    if (!m_readable || !handle_valid(m_handle)) return true;
    if (m_at_end) return true;

    if (peer_closed(m_handle)) {
        m_at_end = true;
        return true;
    }
    return false;
}

bool process_stream::waitForReadyRead(std::chrono::milliseconds timeout)
{
    if (!m_readable || !handle_valid(m_handle)) return false;

    const auto deadline = make_deadline(timeout);

    for (;;) {
        if (available() > 0) return true;
        if (m_at_end) return false;

        const int slice = wait_slice(deadline, std::chrono::milliseconds(10));
        if (slice == 0) return false;

        struct pollfd waiting{};
        waiting.fd = as_fd(m_handle);
        waiting.events = POLLIN;
        ::poll(&waiting, 1, slice);
    }
}

#endif


// ── process: construction and configuration ──────────────────────────

process::process() = default;

process::process(const fs::path& proc, const scl2::stringlist& arguments)
    : m_path(proc)
    , m_args(arguments)
{}

process::~process()
{
    if (m_started && !m_detached) kill();
    release();
}

process::process(process&& another) noexcept
    : m_path(std::move(another.m_path))
    , m_args(std::move(another.m_args))
    , m_working_dir(std::move(another.m_working_dir))
    , m_mode(another.m_mode)
    , m_read_channel(another.m_read_channel)
    , m_process_handle(std::exchange(another.m_process_handle, process_stream::invalid_handle))
    , m_thread_handle(std::exchange(another.m_thread_handle, process_stream::invalid_handle))
    , m_pid(std::exchange(another.m_pid, 0))
    , m_channels(std::move(another.m_channels))
    , m_started(std::exchange(another.m_started, false))
    , m_detached(std::exchange(another.m_detached, false))
    , m_exit_known(std::exchange(another.m_exit_known, false))
    , m_exit_code(std::exchange(another.m_exit_code, -1))
    , m_err(another.m_err)
    , m_err_text(std::move(another.m_err_text))
{
    // The moved-from object now looks like a process that was never started, so
    // it will not close anything or shut the child down when it dies.
    another.m_err = error_code::no_error;
    another.m_err_text.clear();
}

process& process::operator=(process&& another) noexcept
{
    if (this == &another) return *this;

    if (m_started && !m_detached) kill();
    release();

    m_path = std::move(another.m_path);
    m_args = std::move(another.m_args);
    m_working_dir = std::move(another.m_working_dir);
    m_mode = another.m_mode;
    m_read_channel = another.m_read_channel;
    m_process_handle = std::exchange(another.m_process_handle, process_stream::invalid_handle);
    m_thread_handle = std::exchange(another.m_thread_handle, process_stream::invalid_handle);
    m_pid = std::exchange(another.m_pid, 0);
    m_channels = std::move(another.m_channels);
    m_started = std::exchange(another.m_started, false);
    m_detached = std::exchange(another.m_detached, false);
    m_exit_known = std::exchange(another.m_exit_known, false);
    m_exit_code = std::exchange(another.m_exit_code, -1);
    m_err = another.m_err;
    m_err_text = std::move(another.m_err_text);

    another.m_err = error_code::no_error;
    another.m_err_text.clear();
    return *this;
}

void process::setPath(const fs::path& path) { m_path = path; }
fs::path process::path() const { return m_path; }

void process::setArguments(const scl2::stringlist& args) { m_args = args; }
scl2::stringlist process::arguments() const { return m_args; }

void process::setWorkingDirectory(const fs::path& dir) { m_working_dir = dir; }
fs::path process::workingDirectory() const { return m_working_dir; }

void process::setChannelMode(channel_mode mode)
{
    if (running()) return;
    m_mode = mode;
}

process::channel_mode process::channelMode() const
{
    return m_mode;
}

void process::setReadChannel(channel ch)
{
    // A merged child only has one channel, so asking for stderr makes no sense.
    if (m_mode == channel_mode::merged) {
        m_read_channel = channel::standard_output;
        return;
    }
    m_read_channel = ch;
}

process::channel process::readChannel() const
{
    return m_read_channel;
}


// ── process: platform independent lifecycle ──────────────────────────

void process::detach()
{
    if (!m_started || m_detached) return;

    // release() closes every pipe and forgets the child. No signal is sent, so
    // the child keeps running; it just loses its connection to us.
    release();
}

bool process::waitForFinished(std::chrono::milliseconds timeout)
{
    if (!m_started || m_detached) return true;

    if (wait_for_exit(timeout)) {
        pump();  // collect whatever the child left in the pipes
        return true;
    }

    m_err = error_code::timed_out;
    m_err_text = "the child process did not finish within the timeout";
    return false;
}

int process::exitcode() const
{
    if (!finished()) return -1;
    return m_exit_code;
}

std::int64_t process::processId() const
{
    return m_started ? m_pid : 0;
}

process::error_code process::error() const
{
    return m_err;
}

std::string process::errorString() const
{
    return m_err_text;
}

void process::pump()
{
    for (pipe_id id : { pipe_id::std_out, pipe_id::std_err }) {
        channel_slot& channel = slot(id);
        if (!channel.owned()) continue;

        std::shared_ptr<process_stream> stream = channel.lock();
        if (!stream || !stream->valid() || !stream->readable()) continue;

        const size_t count = stream->available();
        if (count == 0) continue;

        scl2::bytearray data = stream->read(count);
        if (data.size() != 0) channel.buffer.append(data);
    }
}


// ── process: reading and writing ─────────────────────────────────────

bool process::readyRead()
{
    pump();
    return slot(to_pipe(m_read_channel)).buffer.size() > 0;
}

size_t process::available()
{
    pump();
    return slot(to_pipe(m_read_channel)).buffer.size();
}

scl2::bytearray process::read(size_t bytes)
{
    pump();
    return take_front(slot(to_pipe(m_read_channel)).buffer, bytes);
}

scl2::bytearray process::readAll()
{
    pump();
    return take_front(slot(to_pipe(m_read_channel)).buffer, static_cast<size_t>(-1));
}

bool process::readyReadStandardOutput()
{
    pump();
    return slot(pipe_id::std_out).buffer.size() > 0;
}

bool process::readyReadStandardError()
{
    pump();
    return slot(pipe_id::std_err).buffer.size() > 0;
}

size_t process::availableStandardOutput()
{
    pump();
    return slot(pipe_id::std_out).buffer.size();
}

size_t process::availableStandardError()
{
    pump();
    return slot(pipe_id::std_err).buffer.size();
}

scl2::bytearray process::readAllStandardOutput()
{
    pump();
    return take_front(slot(pipe_id::std_out).buffer, static_cast<size_t>(-1));
}

scl2::bytearray process::readAllStandardError()
{
    pump();
    return take_front(slot(pipe_id::std_err).buffer, static_cast<size_t>(-1));
}

size_t process::write(const scl2::bytearray& data)
{
    std::shared_ptr<process_stream> stream = slot(pipe_id::std_in).lock();
    if (!stream || !stream->valid() || !stream->writable()) {
        m_err = error_code::write_error;
        m_err_text = "the child's stdin is not available";
        return 0;
    }

    const size_t written = stream->write(data);
    if (written < data.size()) {
        m_err = error_code::write_error;
        m_err_text = "only part of the data could be written to the child";
    }
    return written;
}

void process::closeWriteChannel()
{
    channel_slot& channel = slot(pipe_id::std_in);

    std::shared_ptr<process_stream> stream = channel.lock();
    if (stream) stream->reset();

    channel.owner.reset();
    channel.ref.reset();
}


// ── process: channel streams ─────────────────────────────────────────

std::shared_ptr<process_stream> process::stdinStream() const
{
    return slot(pipe_id::std_in).lock();
}

std::shared_ptr<process_stream> process::stdoutStream() const
{
    return slot(pipe_id::std_out).lock();
}

std::shared_ptr<process_stream> process::stderrStream() const
{
    return slot(pipe_id::std_err).lock();
}

std::shared_ptr<process_stream> process::detachStdInput()
{
    channel_slot& channel = slot(pipe_id::std_in);

    std::shared_ptr<process_stream> stream = channel.owner;
    channel.owner.reset();
    channel.buffer.clear();
    return stream;
}

std::shared_ptr<process_stream> process::detachStdOutput()
{
    channel_slot& channel = slot(pipe_id::std_out);

    std::shared_ptr<process_stream> stream = channel.owner;
    channel.owner.reset();
    channel.buffer.clear();
    return stream;
}

std::shared_ptr<process_stream> process::detachStdError()
{
    channel_slot& channel = slot(pipe_id::std_err);

    std::shared_ptr<process_stream> stream = channel.owner;
    channel.owner.reset();
    channel.buffer.clear();
    return stream;
}

bool process::attached(channel ch) const
{
    return slot(to_pipe(ch)).owned();
}


// ── process: stream state ────────────────────────────────────────────

bool process::valid()
{
    const channel_slot& channel = slot(to_pipe(m_read_channel));
    return channel.owned() && channel.lock() != nullptr;
}

bool process::reset()
{
    if (m_started && !m_detached) terminate();
    release();
    return true;
}

void process::release()
{
    // On Unix this picks up a child that already exited; on Windows it is a no-op.
    reap_if_exited(m_pid);

    close_handle(m_process_handle);
    close_handle(m_thread_handle);

    for (channel_slot& channel : m_channels) {
        channel.owner.reset();
        channel.ref.reset();
        channel.buffer.clear();
    }

    m_pid = 0;
    m_started = false;
    m_detached = false;
    m_exit_known = false;
    m_exit_code = -1;
}


// ── process: platform specific lifecycle ─────────────────────────────

#ifdef OS_WINDOWS

bool process::start()
{
    if (running()) {
        m_err = error_code::already_running;
        m_err_text = "the process is already running";
        return false;
    }

    // Starting over: drop whatever a previous run left behind.
    release();
    m_err = error_code::no_error;
    m_err_text.clear();

    if (m_path.empty()) {
        m_err = error_code::failed_to_start;
        m_err_text = "no program path has been set";
        return false;
    }

    // A bare name is resolved through PATH by CreateProcessW, so only check the
    // ones that carry a directory part.
    if (m_path.has_parent_path() && !fs::exists(m_path)) {
        m_err = error_code::failed_to_start;
        m_err_text = "the executable does not exist: " + m_path.string();
        return false;
    }

    const bool merge_stderr = (m_mode == channel_mode::merged);

    native_handle child_stdin = invalid_handle;
    native_handle parent_stdin = invalid_handle;
    native_handle parent_stdout = invalid_handle;
    native_handle child_stdout = invalid_handle;
    native_handle parent_stderr = invalid_handle;
    native_handle child_stderr = invalid_handle;

    if (!create_in_pipe(child_stdin, parent_stdin)
        || !create_out_pipe(parent_stdout, child_stdout)
        || (!merge_stderr && !create_out_pipe(parent_stderr, child_stderr)))
    {
        const DWORD last_error = ::GetLastError();

        close_handle(child_stdin);
        close_handle(parent_stdin);
        close_handle(parent_stdout);
        close_handle(child_stdout);
        close_handle(parent_stderr);
        close_handle(child_stderr);

        m_err = error_code::failed_to_start;
        m_err_text = scl2::wstr_to_str(platform::windows::TranslateErrorW(last_error));
        return false;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = as_handle(child_stdin);
    si.hStdOutput = as_handle(child_stdout);
    si.hStdError = as_handle(merge_stderr ? child_stdout : child_stderr);

    // The command line has to be built the way the CRT parses it back: the
    // program is quoted, and the arguments are packed by stringlist::pack(),
    // which is the exact counterpart of stringlist::unpack().
    std::wstring command = L"\"" + m_path.wstring() + L"\"";
    if (!m_args.empty()) {
        command += L" ";
        command += scl2::str_to_wstr(m_args.pack());
    }

    std::vector<wchar_t> command_buffer(command.begin(), command.end());
    command_buffer.push_back(L'\0');

    const std::wstring working_dir = m_working_dir.wstring();

    PROCESS_INFORMATION pi{};
    const BOOL ok = ::CreateProcessW(
        nullptr,
        command_buffer.data(),
        nullptr,
        nullptr,
        TRUE,       // the child inherits our pipe ends
        0,
        nullptr,
        working_dir.empty() ? nullptr : working_dir.c_str(),
        &si,
        &pi
    );
    const DWORD last_error = ::GetLastError();

    // Let go of the child's ends: as long as we hold them the child never sees
    // EOF on stdin, and our reads never see the output pipes break.
    close_handle(child_stdin);
    close_handle(child_stdout);
    close_handle(child_stderr);

    if (!ok) {
        close_handle(parent_stdin);
        close_handle(parent_stdout);
        close_handle(parent_stderr);

        m_err = error_code::failed_to_start;
        m_err_text = scl2::wstr_to_str(platform::windows::TranslateErrorW(last_error));
        return false;
    }

    m_process_handle = reinterpret_cast<native_handle_type>(pi.hProcess);
    m_thread_handle = reinterpret_cast<native_handle_type>(pi.hThread);
    m_pid = static_cast<std::int64_t>(pi.dwProcessId);
    m_started = true;

    slot(pipe_id::std_in).owner = std::make_shared<process_stream>(parent_stdin, false, true);
    slot(pipe_id::std_out).owner = std::make_shared<process_stream>(parent_stdout, true, false);
    if (!merge_stderr) {
        slot(pipe_id::std_err).owner = std::make_shared<process_stream>(parent_stderr, true, false);
    }

    for (channel_slot& channel : m_channels) {
        if (channel.owner) channel.ref = channel.owner;
        channel.buffer.clear();
    }

    return true;
}

bool process::startDetached(const fs::path& proc, const scl2::stringlist& arguments,
                            const fs::path& working_dir)
{
    if (proc.empty()) return false;
    if (proc.has_parent_path() && !fs::exists(proc)) return false;

    std::wstring command = L"\"" + proc.wstring() + L"\"";
    if (!arguments.empty()) {
        command += L" ";
        command += scl2::str_to_wstr(arguments.pack());
    }

    std::vector<wchar_t> command_buffer(command.begin(), command.end());
    command_buffer.push_back(L'\0');

    const std::wstring dir = working_dir.wstring();

    // No pipe at all, so the child simply borrows our console.
    STARTUPINFOW si{};
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi{};
    const BOOL ok = ::CreateProcessW(
        nullptr,
        command_buffer.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        dir.empty() ? nullptr : dir.c_str(),
        &si,
        &pi
    );

    if (!ok) return false;

    // Nothing on our side keeps the child alive any more: drop the handles and
    // let it run. Its exit code is out of reach from now on.
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    return true;
}

void process::terminate()
{
    if (!m_started || m_detached || !handle_valid(m_process_handle)) return;

    if (running()) {
        ::TerminateProcess(as_handle(m_process_handle), 0);
    }
    wait_for_exit(infinite_timeout);
}

void process::kill()
{
    terminate();
}

bool process::running() const
{
    if (!m_started || m_detached || !handle_valid(m_process_handle)) return false;

    DWORD code = 0;
    if (!::GetExitCodeProcess(as_handle(m_process_handle), &code)) return false;
    return code == STILL_ACTIVE;
}

bool process::finished() const
{
    // Never started does not count as finished.
    if (!m_started) return false;
    if (m_exit_known) return true;
    if (m_detached || !handle_valid(m_process_handle)) return false;

    DWORD code = 0;
    if (!::GetExitCodeProcess(as_handle(m_process_handle), &code)) return false;
    if (code == STILL_ACTIVE) return false;

    m_exit_known = true;
    m_exit_code = static_cast<int>(code);
    return true;
}

bool process::wait_for_exit(std::chrono::milliseconds timeout)
{
    if (!m_started || m_detached || !handle_valid(m_process_handle)) return false;

    const auto deadline = make_deadline(timeout);

    for (;;) {
        pump();
        if (finished()) return true;

        const int slice = wait_slice(deadline, std::chrono::milliseconds(50));
        if (slice == 0) return false;

        HANDLE handles[3];
        DWORD count = 0;
        handles[count++] = as_handle(m_process_handle);

        // Also wake up on the pipes: a child that fills one of them blocks, so
        // draining is what lets it reach its exit at all.
        for (pipe_id id : { pipe_id::std_out, pipe_id::std_err }) {
            channel_slot& channel = slot(id);
            if (!channel.owned()) continue;

            std::shared_ptr<process_stream> stream = channel.lock();
            if (!stream || !stream->valid() || !stream->readable() || stream->atEnd()) continue;

            handles[count++] = as_handle(stream->nativeHandle());
        }

        ::WaitForMultipleObjects(count, handles, FALSE, static_cast<DWORD>(slice));
    }
}

bool process::waitForReadyRead(std::chrono::milliseconds timeout)
{
    const channel wanted = readChannel();
    const auto deadline = make_deadline(timeout);

    for (;;) {
        pump();
        if (slot(to_pipe(wanted)).buffer.size() > 0) return true;

        // Learn about an exit, so that a dead child does not keep us waiting.
        if (m_started && !m_detached) finished();

        const int slice = wait_slice(deadline, std::chrono::milliseconds(50));
        if (slice == 0) return false;

        HANDLE handles[3];
        DWORD count = 0;
        bool any_pipe = false;

        if (m_started && !m_detached && !m_exit_known && handle_valid(m_process_handle)) {
            handles[count++] = as_handle(m_process_handle);
        }

        for (pipe_id id : { pipe_id::std_out, pipe_id::std_err }) {
            channel_slot& channel = slot(id);
            if (!channel.owned()) continue;

            std::shared_ptr<process_stream> stream = channel.lock();
            if (!stream || !stream->valid() || !stream->readable() || stream->atEnd()) continue;

            handles[count++] = as_handle(stream->nativeHandle());
            any_pipe = true;
        }

        // Every pipe we still hold is drained and broken, so nothing can ever
        // arrive through them again.
        if (!any_pipe) return false;

        ::WaitForMultipleObjects(count, handles, FALSE, static_cast<DWORD>(slice));
    }
}

#else  // OS_UNIX / OS_ANDROID

/// How long a SIGTERM gets before terminate() escalates to SIGKILL.
constexpr auto terminate_grace = std::chrono::seconds(3);

bool process::start()
{
    if (running()) {
        m_err = error_code::already_running;
        m_err_text = "the process is already running";
        return false;
    }

    // Starting over: drop whatever a previous run left behind.
    release();
    m_err = error_code::no_error;
    m_err_text.clear();

    if (m_path.empty()) {
        m_err = error_code::failed_to_start;
        m_err_text = "no program path has been set";
        return false;
    }

    // A bare name is looked up in PATH by execvp(), so only check the ones that
    // carry a directory part.
    if (m_path.has_parent_path() && !fs::exists(m_path)) {
        m_err = error_code::failed_to_start;
        m_err_text = "the executable does not exist: " + m_path.string();
        return false;
    }

    const bool merge_stderr = (m_mode == channel_mode::merged);

    // Everything the child needs is built here. After fork() only
    // async-signal-safe calls are allowed, and allocating is not one of them.
    const std::string program = m_path.string();

    std::vector<std::string> arg_store;
    arg_store.reserve(1 + m_args.size());
    arg_store.push_back(program);
    for (const std::string& arg : m_args) arg_store.push_back(arg);

    std::vector<char*> argv;
    argv.reserve(arg_store.size() + 1);
    for (std::string& arg : arg_store) argv.push_back(arg.data());
    argv.push_back(nullptr);

    const std::string working_dir = m_working_dir.string();

    int in_pipe[2] = {-1, -1};
    int out_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};
    int status_pipe[2] = {-1, -1};

    if (!create_pipe(in_pipe, false)        // we keep the write end
        || !create_pipe(out_pipe, true)     // we keep the read end
        || (!merge_stderr && !create_pipe(err_pipe, true)))
    {
        const int last_error = errno;
        close_fds(in_pipe);
        close_fds(out_pipe);
        close_fds(err_pipe);

        m_err = error_code::failed_to_start;
        m_err_text = std::strerror(last_error);
        return false;
    }

    // Reports exec() failures back to us: the write end is close-on-exec, so a
    // successful exec closes it and we read EOF, while a failed one hands us the
    // errno instead. This is what keeps start() failing on Unix the same way it
    // does on Windows.
    if (::pipe(status_pipe) != 0) {
        const int last_error = errno;
        close_fds(in_pipe);
        close_fds(out_pipe);
        close_fds(err_pipe);

        m_err = error_code::failed_to_start;
        m_err_text = std::strerror(last_error);
        return false;
    }
    ::fcntl(status_pipe[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(status_pipe[1], F_SETFD, FD_CLOEXEC);

    const pid_t child = ::fork();
    if (child < 0) {
        const int last_error = errno;
        close_fds(in_pipe);
        close_fds(out_pipe);
        close_fds(err_pipe);
        close_fds(status_pipe);

        m_err = error_code::failed_to_start;
        m_err_text = std::strerror(last_error);
        return false;
    }

    if (child == 0) {
        // ── child: async-signal-safe calls only ──
        int fd_source[4];
        int fd_target[4];

        fd_source[0] = in_pipe[0];                          fd_target[0] = STDIN_FILENO;
        fd_source[1] = out_pipe[1];                         fd_target[1] = STDOUT_FILENO;
        fd_source[2] = merge_stderr ? out_pipe[1] : err_pipe[1]; fd_target[2] = STDERR_FILENO;
        fd_source[3] = status_pipe[1];                      fd_target[3] = -1;  // stays open until exec()

        for (int i = 0; i < 4; ++i) {
            if (fd_target[i] >= 0 && fd_source[i] == fd_target[i]) {
                // dup2(fd, fd) is a no-op and would leave close-on-exec set on a
                // standard stream, so clear the flag by hand instead.
                (void)::fcntl(fd_source[i], F_SETFD, 0);
                fd_target[i] = -1;
                continue;
            }
            if (fd_source[i] >= 0 && fd_source[i] < reserved_fd_count) {
                // Move it out of the way first, otherwise an earlier dup2() would
                // clobber a source we still need. Every descriptor we created is
                // close-on-exec, so the duplicate cleans up after itself.
                const int moved = ::fcntl(fd_source[i], F_DUPFD, reserved_fd_count);
                if (moved < 0) ::_exit(exec_failed_exit_code);
                (void)::fcntl(moved, F_SETFD, FD_CLOEXEC);
                fd_source[i] = moved;
            }
        }

        for (int i = 0; i < 4; ++i) {
            if (fd_target[i] >= 0 && ::dup2(fd_source[i], fd_target[i]) < 0) {
                ::_exit(exec_failed_exit_code);
            }
        }

        if (!working_dir.empty() && ::chdir(working_dir.c_str()) != 0) {
            const int last_error = errno;
            (void)!::write(fd_source[3], &last_error, sizeof(last_error));
            ::_exit(exec_failed_exit_code);
        }

        ::execvp(argv[0], argv.data());

        const int last_error = errno;
        (void)!::write(fd_source[3], &last_error, sizeof(last_error));
        ::_exit(exec_failed_exit_code);
    }

    // ── parent ──
    // Let go of the child's ends, otherwise stdin never sees EOF and the output
    // pipes never break.
    ::close(in_pipe[0]);
    in_pipe[0] = -1;
    ::close(out_pipe[1]);
    out_pipe[1] = -1;
    if (!merge_stderr) {
        ::close(err_pipe[1]);
        err_pipe[1] = -1;
    }
    ::close(status_pipe[1]);
    status_pipe[1] = -1;

    int reported = 0;
    ssize_t got = -1;
    do {
        got = ::read(status_pipe[0], &reported, sizeof(reported));
    } while (got < 0 && errno == EINTR);
    ::close(status_pipe[0]);
    status_pipe[0] = -1;

    if (got == static_cast<ssize_t>(sizeof(reported))) {
        close_fds(in_pipe);
        close_fds(out_pipe);
        close_fds(err_pipe);

        int status = 0;
        while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {}

        m_err = error_code::failed_to_start;
        m_err_text = std::strerror(reported);
        return false;
    }

    m_pid = static_cast<std::int64_t>(child);
    m_started = true;

    slot(pipe_id::std_in).owner = std::make_shared<process_stream>(in_pipe[1], false, true);
    slot(pipe_id::std_out).owner = std::make_shared<process_stream>(out_pipe[0], true, false);
    if (!merge_stderr) {
        slot(pipe_id::std_err).owner = std::make_shared<process_stream>(err_pipe[0], true, false);
    }

    for (channel_slot& channel : m_channels) {
        if (channel.owner) channel.ref = channel.owner;
        channel.buffer.clear();
    }

    return true;
}

bool process::startDetached(const fs::path& proc, const scl2::stringlist& arguments,
                            const fs::path& working_dir)
{
    if (proc.empty()) return false;
    if (proc.has_parent_path() && !fs::exists(proc)) return false;

    const std::string program = proc.string();

    std::vector<std::string> arg_store;
    arg_store.reserve(1 + arguments.size());
    arg_store.push_back(program);
    for (const std::string& arg : arguments) arg_store.push_back(arg);

    std::vector<char*> argv;
    argv.reserve(arg_store.size() + 1);
    for (std::string& arg : arg_store) argv.push_back(arg.data());
    argv.push_back(nullptr);

    const std::string dir = working_dir.string();

    const pid_t middle = ::fork();
    if (middle < 0) return false;

    if (middle == 0) {
        // This process only exists to be the parent that is not us: it forks the
        // real child into its own session and goes away at once, so the child is
        // reparented to init and can never turn into our zombie.
        (void)::setsid();

        const pid_t detached = ::fork();
        if (detached != 0) ::_exit(0);

        if (!dir.empty() && ::chdir(dir.c_str()) != 0) ::_exit(exec_failed_exit_code);
        ::execvp(argv[0], argv.data());
        ::_exit(exec_failed_exit_code);
    }

    // Pick up the middle process; this also makes sure it is really gone before
    // we report success.
    int status = 0;
    while (::waitpid(middle, &status, 0) < 0 && errno == EINTR) {}
    return true;
}

void process::terminate()
{
    if (!m_started || m_detached || m_pid <= 0) return;

    if (!finished()) {
        ::kill(static_cast<pid_t>(m_pid), SIGTERM);

        // A child is free to ignore SIGTERM, and reset() and the destructor both
        // rely on this call coming back, so escalate when it stalls.
        if (!wait_for_exit(terminate_grace)) {
            ::kill(static_cast<pid_t>(m_pid), SIGKILL);
        }
    }
    wait_for_exit(infinite_timeout);
}

void process::kill()
{
    if (!m_started || m_detached || m_pid <= 0) return;

    if (!finished()) {
        ::kill(static_cast<pid_t>(m_pid), SIGKILL);
    }
    wait_for_exit(infinite_timeout);
}

bool process::running() const
{
    if (!m_started || m_detached || m_pid <= 0) return false;
    if (m_exit_known) return false;
    return !finished();
}

bool process::finished() const
{
    // Never started does not count as finished.
    if (!m_started) return false;
    if (m_exit_known) return true;
    if (m_detached || m_pid <= 0) return false;

    int status = 0;
    const pid_t result = ::waitpid(static_cast<pid_t>(m_pid), &status, WNOHANG);

    if (result == 0) return false;  // still running
    if (result < 0) {
        if (errno == EINTR) return false;
        // ECHILD: somebody else reaped it, so we will never learn the status.
        m_exit_known = true;
        m_exit_code = -1;
        return true;
    }

    m_exit_known = true;
    // A child that died from a signal has no exit code of its own, so it reports
    // -1 just like one that never ran.
    m_exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return true;
}

bool process::wait_for_exit(std::chrono::milliseconds timeout)
{
    if (!m_started || m_detached) return false;

    const auto deadline = make_deadline(timeout);

    for (;;) {
        pump();
        if (finished()) return true;

        const int slice = wait_slice(deadline, std::chrono::milliseconds(50));
        if (slice == 0) return false;

        struct pollfd waiting[2];
        nfds_t count = 0;

        // A child that fills a pipe blocks, so draining is what lets it reach its
        // exit at all. Pipes whose writer is gone are left out: poll() reports
        // POLLHUP on them immediately, which would turn this into a busy loop.
        for (pipe_id id : { pipe_id::std_out, pipe_id::std_err }) {
            channel_slot& channel = slot(id);
            if (!channel.owned()) continue;

            std::shared_ptr<process_stream> stream = channel.lock();
            if (!stream || !stream->valid() || !stream->readable() || stream->atEnd()) continue;

            waiting[count].fd = as_fd(stream->nativeHandle());
            waiting[count].events = POLLIN;
            waiting[count].revents = 0;
            ++count;
        }

        // count == 0 is fine: poll() then behaves like a plain sleep, which is
        // all we need while waiting for the child to be reaped.
        ::poll(waiting, count, slice);
    }
}

bool process::waitForReadyRead(std::chrono::milliseconds timeout)
{
    const channel wanted = readChannel();
    const auto deadline = make_deadline(timeout);

    for (;;) {
        pump();
        if (slot(to_pipe(wanted)).buffer.size() > 0) return true;

        // Learn about an exit, so that a dead child does not keep us waiting.
        if (m_started && !m_detached) finished();

        const int slice = wait_slice(deadline, std::chrono::milliseconds(50));
        if (slice == 0) return false;

        struct pollfd waiting[2];
        nfds_t count = 0;

        for (pipe_id id : { pipe_id::std_out, pipe_id::std_err }) {
            channel_slot& channel = slot(id);
            if (!channel.owned()) continue;

            std::shared_ptr<process_stream> stream = channel.lock();
            if (!stream || !stream->valid() || !stream->readable() || stream->atEnd()) continue;

            waiting[count].fd = as_fd(stream->nativeHandle());
            waiting[count].events = POLLIN;
            waiting[count].revents = 0;
            ++count;
        }

        // Every pipe we still hold is drained and broken, so nothing can ever
        // arrive through them again.
        if (count == 0) return false;

        ::poll(waiting, count, slice);
    }
}

#endif

} // namespace scl2
