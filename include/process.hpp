/*
    Child process management over pipes.

    scl2::process starts an external executable and talks to its standard
    streams. The interface follows Qt's QProcess, adapted to this library's
    synchronous, event loop free style:

      - scl2::process is itself a scl2::basic_iostream: reads come from the
        child's current read channel (stdout by default), writes go to the
        child's stdin. Use setReadChannel() to read stderr instead.
      - The two output channels can be kept apart (channel_mode::separate, the
        default) or merged into a single pipe (channel_mode::merged).
      - Every channel is backed by a process_stream, which can be torn off and
        owned by the caller (see detachStdError() and friends).
      - There is no event loop, so the blocking functions are the ones that
        move data into the process level buffers. waitForFinished() and
        waitForReadyRead() drain both output channels while they wait, so a
        chatty child can never deadlock on a full pipe.

    Windows and Unix are both implemented. Both use anonymous pipes, driven
    through PeekNamedPipe / WaitForMultipleObjects on Windows and through
    FIONREAD / poll() everywhere else. Only process.cpp is platform specific,
    which is why this header needs no platform header at all: the native handle
    travels as an opaque std::intptr_t and everything else is plain data.

    classes:
        scl2::process_stream
        scl2::process

    link target:
        SharedCppLib2::process
*/

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "stringlist.hpp"
#include "stream.hpp"

namespace fs = std::filesystem;

namespace scl2 {

/// @brief One end of a single pipe belonging to a child process.
///
/// A process_stream wraps exactly one native pipe handle: a read only end (the
/// child's stdout or stderr) or a write only end (the child's stdin). read() on
/// a write only stream, and write() on a read only stream, report zero bytes
/// instead of failing.
///
/// The handle is kept as an opaque integer, so this header stays free of any
/// platform header. Use nativeHandle() if you need the real thing.
class process_stream : public scl2::basic_iostream
{
public:
    /// @brief Opaque native handle (HANDLE on Windows, file descriptor on Unix).
    /// @note invalid_handle is INVALID_HANDLE_VALUE on Windows and -1 elsewhere.
    ///       It is never a real pipe, not even when the caller has closed its own
    ///       standard descriptors so that a pipe lands on 0, 1 or 2.
    using native_handle_type = std::intptr_t;
    static constexpr native_handle_type invalid_handle = -1;

    /// @brief Construct a stream that holds no handle. valid() returns false.
    process_stream();

    /// @brief Construct a stream that takes over @p handle .
    /// @param handle native pipe handle, ownership is taken over
    /// @param readable whether this end may be read from
    /// @param writable whether this end may be written to
    process_stream(native_handle_type handle, bool readable, bool writable);

    ~process_stream() override;

    disable_copy(process_stream)

    /// @note Hand written rather than defaulted: a moved-from stream has to give
    ///       up its handle, or both objects would end up closing it.
    process_stream(process_stream&& another) noexcept;
    process_stream& operator=(process_stream&& another) noexcept;

    // ── scl2::basic_iostream ─────────────────────────────────────────

    bool valid() override;
    bool readyRead() override;
    size_t available() override;

    scl2::bytearray read(size_t bytes) override;
    scl2::bytearray readAll() override;

    size_t write(const scl2::bytearray& data) override;

    /// @brief Wait until the pipe has data, or until the writer goes away.
    /// @note The base implementation can only poll. Here the wait goes through
    ///       poll() on Unix, and through the pipe handle on Windows; both wake up
    ///       as soon as data lands instead of sleeping a flat 10ms.
    bool waitForReadyRead(std::chrono::milliseconds timeout = std::chrono::seconds(5)) override;

    /// @brief Close the handle and become invalid. Idempotent, always succeeds.
    bool reset() override;

    // ── additional ───────────────────────────────────────────────────

    native_handle_type nativeHandle() const;

    bool readable() const;
    bool writable() const;

    /// @brief Whether the other end of the pipe has been closed.
    /// @note Once true it stays true: no more data can ever arrive.
    bool atEnd() const;

private:
    // Kept platform free on purpose: the only thing that depends on the OS is
    // how these integers are interpreted, and that lives in process.cpp.
    native_handle_type m_handle = invalid_handle;
    bool m_readable = false;
    bool m_writable = false;
    mutable bool m_at_end = false;  ///< sticky: once the peer is gone it stays gone
};


/// @brief A child process, with pipes attached to its standard streams.
class process : public scl2::basic_iostream
{
public:
    /// @brief Which output channel the plain read functions work on.
    enum class channel : std::uint8_t
    {
        standard_output = 0,
        standard_error = 1,
    };

    /// @brief How the child's two output channels are wired up.
    enum class channel_mode : std::uint8_t
    {
        /// stdout and stderr each get their own pipe. This is the default.
        separate = 0,
        /// stderr is redirected into the stdout pipe. The child's own write
        /// order is preserved, but the two channels can no longer be told
        /// apart, and readAllStandardError() always returns nothing.
        merged = 1,
    };

    /// @brief The reason the last start() or wait function failed.
    enum class error_code : std::uint8_t
    {
        no_error = 0,
        failed_to_start,  ///< the executable could not be launched
        already_running,  ///< start() was called while a child was still alive
        timed_out,        ///< a wait function hit its timeout
        write_error,      ///< writing to the child's stdin failed
        read_error,       ///< reading from the child's output failed
        unknown_error,
    };

    /// @brief Same opaque handle type as process_stream::native_handle_type.
    using native_handle_type = process_stream::native_handle_type;

    process();
    explicit process(const fs::path& proc, const scl2::stringlist& arguments = scl2::stringlist());

    /// @note Kills the child if it is still running (see kill()), then releases
    ///       every handle and buffer.
    ~process() override;

    disable_copy(process)

    /// @note Hand written rather than defaulted, for the same reason as
    ///       process_stream: the moved-from object must let go of its handles
    ///       and must not try to shut the child down when it is destroyed.
    process(process&& another) noexcept;
    process& operator=(process&& another) noexcept;

    // ── configuration ────────────────────────────────────────────────

    void setPath(const fs::path& path);
    fs::path path() const;

    void setArguments(const scl2::stringlist& args);
    scl2::stringlist arguments() const;

    /// @brief Working directory for the child. Empty means "inherit ours".
    void setWorkingDirectory(const fs::path& dir);
    fs::path workingDirectory() const;

    /// @brief Has no effect while the process is running.
    void setChannelMode(channel_mode mode);
    channel_mode channelMode() const;

    // ── lifecycle ────────────────────────────────────────────────────

    /// @brief Launch the child. Returns false on failure, see error().
    /// @note Fails with error_code::already_running when a child is still alive;
    ///       call reset() or waitForFinished() first.
    bool start();

    /// @brief Launch the child without attaching any pipe, then forget about it.
    /// The child keeps running after the returned object is gone.
    /// @note Unix: a double fork puts the child in its own session, so it
    ///       outlives us and never becomes a zombie. Windows: nothing is
    ///       inherited and the child keeps sharing our console.
    static bool startDetached(const fs::path& proc,
                              const scl2::stringlist& arguments = scl2::stringlist(),
                              const fs::path& working_dir = fs::path());

    /// @brief Release our handles and leave the child running.
    /// The pipes are closed, which the child sees as EOF / broken pipe. After
    /// this the child can no longer be waited for, read from or terminated.
    /// @note Unix: nobody reaps the child any more, so it stays a zombie once it
    ///       exits. Hold on to processId() and wait for it yourself if that
    ///       matters.
    void detach();

    /// @brief Ask the child to stop, and make sure it does.
    /// Unix: SIGTERM, escalated to SIGKILL if it is still alive after a few
    /// seconds. Windows: TerminateProcess, which cannot be refused.
    void terminate();

    /// @brief Take the child down without giving it a chance to clean up.
    /// Unix: SIGKILL. Windows: TerminateProcess, exactly like terminate().
    void kill();

    bool running() const;
    bool finished() const;

    /// @brief The child's exit code, or -1 if it has not exited.
    /// @note A child killed by a signal also reports -1 on Unix.
    int exitcode() const;

    /// @brief The child's process id, or 0 if it was never started.
    std::int64_t processId() const;

    error_code error() const;
    std::string errorString() const;

    // ── waiting ──────────────────────────────────────────────────────

    /// @brief Wait for the child to exit, draining its output into our buffers
    /// so that it can not block on a full pipe.
    bool waitForFinished(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    /// @brief Wait until the current read channel has data.
    /// Both channels are drained while waiting, so this can not deadlock.
    bool waitForReadyRead(std::chrono::milliseconds timeout = std::chrono::seconds(5)) override;

    // ── reading ──────────────────────────────────────────────────────

    /// @brief Pick the channel used by the inherited read functions.
    /// @note Ignored in channel_mode::merged, where only stdout exists.
    void setReadChannel(channel ch);
    channel readChannel() const;

    bool readyRead() override;
    size_t available() override;
    scl2::bytearray read(size_t bytes) override;
    scl2::bytearray readAll() override;

    bool readyReadStandardOutput();
    bool readyReadStandardError();
    size_t availableStandardOutput();
    size_t availableStandardError();
    scl2::bytearray readAllStandardOutput();
    scl2::bytearray readAllStandardError();

    // ── writing ──────────────────────────────────────────────────────

    size_t write(const scl2::bytearray& data) override;

    /// @brief Close the child's stdin, so that it sees EOF.
    void closeWriteChannel();

    // ── channel streams ──────────────────────────────────────────────

    /// @brief The stream of a channel, or nullptr when it does not exist (a
    /// merged stderr) or has already been torn off.
    /// @note While we own a channel we also buffer it, so reading from the
    ///       stream directly competes with the read functions above. Prefer
    ///       detachStd*() when you want to drive a channel yourself.
    std::shared_ptr<process_stream> stdinStream() const;
    std::shared_ptr<process_stream> stdoutStream() const;
    std::shared_ptr<process_stream> stderrStream() const;

    /// @brief Take over a channel stream. The process stops owning it, so it is
    /// no longer drained or waited on, and we will not close it either.
    /// @note The weak reference we keep lets stdoutStream() and friends report
    ///       nullptr once you drop the stream, and lets attached() answer
    ///       whether the process is still in charge of that channel.
    std::shared_ptr<process_stream> detachStdInput();
    std::shared_ptr<process_stream> detachStdOutput();
    std::shared_ptr<process_stream> detachStdError();

    /// @brief Whether the process still owns (and therefore drains) a channel.
    /// @note Returns false after the matching detachStd*() / closeWriteChannel().
    bool attached(channel ch) const;

    // ── scl2::basic_iostream ─────────────────────────────────────────

    bool valid() override;

    /// @brief Terminate the child if needed, close every handle and drop all
    /// buffers: the object returns to its freshly constructed state and can be
    /// start()ed again. Idempotent, and does not clear error().
    bool reset() override;

private:
    /// @brief Which standard stream a channel carries. Internal.
    enum class pipe_id : std::size_t
    {
        std_in = 0,
        std_out = 1,
        std_err = 2,
    };

    /// @brief One channel we may own, plus whatever we drained out of it.
    ///        Internal, not part of the API.
    struct channel_slot
    {
        std::shared_ptr<process_stream> owner;  ///< set while we own the stream
        std::weak_ptr<process_stream> ref;      ///< always set, so we notice it going away
        scl2::bytearray buffer;                 ///< pulled out of the pipe, not read yet

        std::shared_ptr<process_stream> lock() const { return ref.lock(); }
        bool owned() const { return owner != nullptr; }
    };

    static pipe_id to_pipe(channel ch)
    {
        return ch == channel::standard_error ? pipe_id::std_err : pipe_id::std_out;
    }

    channel_slot& slot(pipe_id id) { return m_channels[static_cast<std::size_t>(id)]; }
    const channel_slot& slot(pipe_id id) const { return m_channels[static_cast<std::size_t>(id)]; }

    /// @brief Move everything the pipes currently hold into our buffers.
    void pump();

    /// @brief Close every handle and stream, drop the buffers and the state.
    void release();

    /// @brief Wait for the child to exit, without touching the error state.
    bool wait_for_exit(std::chrono::milliseconds timeout);

private:
    fs::path m_path;
    scl2::stringlist m_args;
    fs::path m_working_dir;

    channel_mode m_mode = channel_mode::separate;
    channel m_read_channel = channel::standard_output;

    // Native bookkeeping. Only Windows uses the two handles; on Unix they stay
    // invalid and the pid is the whole story.
    native_handle_type m_process_handle = process_stream::invalid_handle;
    native_handle_type m_thread_handle = process_stream::invalid_handle;
    std::int64_t m_pid = 0;

    std::array<channel_slot, 3> m_channels;

    bool m_started = false;
    bool m_detached = false;
    mutable bool m_exit_known = false;
    mutable int m_exit_code = -1;

    error_code m_err = error_code::no_error;
    std::string m_err_text;
};

} // namespace scl2
