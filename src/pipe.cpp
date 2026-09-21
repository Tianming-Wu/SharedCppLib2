#include "pipe.hpp"

#if SCL2_PIPE_SUPPORTED

#include "platform.hpp"

#include <accctrl.h>
#include <aclapi.h>

#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>

namespace scl2::pipe {

namespace {

// The class members hold winhandle_t, an scl2 type the header can name without pulling in a
// Windows header. Win32 wants HANDLE, so both conversions live here.
inline HANDLE H(winhandle_t h) noexcept { return scl2::to_handle<HANDLE>(h); }
inline winhandle_t as_winhandle(HANDLE h) noexcept { return scl2::from_handle(h); }

// Whether a handle is set, and is not INVALID_HANDLE_VALUE.
inline bool is_open(winhandle_t h) noexcept { return h != nullptr && H(h) != INVALID_HANDLE_VALUE; }

// m_overlapped_connect is a void* in the header, for the same reason.
inline OVERLAPPED* ov(void* p) noexcept { return static_cast<OVERLAPPED*>(p); }

// A connection can be asked to stop waiting in two ways: by itself (cancel()) and by its server
// (stop() signals the event every connection it handed out holds a copy of). Both are just
// events, so one wait can cover the I/O and both of them at once.
//
// The waits in this file are at most three objects long, and callers pass an array of three.
DWORD append_cancel_events(winhandle_t own, winhandle_t shared, HANDLE* waits, DWORD count) noexcept
{
    if (is_open(own)) {
        waits[count++] = H(own);
    }
    if (is_open(shared)) {
        waits[count++] = H(shared);
    }
    return count;
}

// Whether either cancel event is signalled. For the paths that poll instead of waiting.
bool cancel_signalled(winhandle_t own, winhandle_t shared) noexcept
{
    return (is_open(own) && WaitForSingleObject(H(own), 0) == WAIT_OBJECT_0)
        || (is_open(shared) && WaitForSingleObject(H(shared), 0) == WAIT_OBJECT_0);
}

// FlushFileBuffers does not return until the other end of the pipe has read everything this
// end has written, and it takes no timeout and cannot be cancelled. A timeout therefore needs
// a helper thread: the handle is duplicated first, so that the worker still holds a valid
// handle when the caller gives up, and so that the connection going away is what releases it.
struct flush_job {
    HANDLE pipe = INVALID_HANDLE_VALUE;
    HANDLE finished = nullptr;
    BOOL ok = FALSE;
    DWORD error = 0;

    ~flush_job() { if (finished) CloseHandle(finished); }
};

// True when the peer has read everything written so far. A negative timeout waits it out on
// this thread instead, which needs no helper and - a blocking call being uncancellable - cannot
// be stopped by a cancel either. `error` is filled in only when the flush actually ran and
// failed; a timeout or a cancel leaves it at 0.
bool flush_pipe(winhandle_t pipe, std::chrono::milliseconds timeout,
                winhandle_t cancel_event, winhandle_t shared_cancel_event, DWORD* error)
{
    if (timeout.count() < 0) {
        const BOOL ok = FlushFileBuffers(H(pipe));
        if (!ok && error) {
            *error = GetLastError();
        }
        return ok != 0;
    }

    HANDLE duplicate = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), H(pipe), GetCurrentProcess(), &duplicate,
                         0, FALSE, DUPLICATE_SAME_ACCESS)) {
        if (error) {
            *error = GetLastError();
        }
        return false;
    }

    auto job = std::make_shared<flush_job>();
    job->pipe = duplicate;
    job->finished = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!job->finished) {
        CloseHandle(duplicate);
        return false;
    }

    // The worker keeps a reference to the job, so the job outlives this call when the flush
    // is left waiting: an unanswered one ends when the peer reads, or when the pipe is gone.
    std::thread([job] {
        job->ok = FlushFileBuffers(job->pipe);
        if (!job->ok) {
            job->error = GetLastError();
        }
        CloseHandle(job->pipe);
        job->pipe = INVALID_HANDLE_VALUE;
        SetEvent(job->finished);
    }).detach();

    // Giving up here does not stop that work: the flush itself has no way to be cancelled, so
    // the helper thread keeps waiting and ends when the peer reads or the connection closes.
    HANDLE waits[3] = { job->finished, nullptr, nullptr };
    const DWORD wait_count = append_cancel_events(cancel_event, shared_cancel_event, waits, 1);

    const DWORD waited = WaitForMultipleObjects(wait_count, waits, FALSE,
                                               static_cast<DWORD>(timeout.count()));
    if (waited != WAIT_OBJECT_0) {
        return false;
    }

    if (!job->ok && error) {
        *error = job->error;
    }
    return job->ok != 0;
}

// The exception message: what went wrong, in the system's own words. platform already owns the
// FormatMessage plumbing, so this is only the pipe-specific framing around it.
std::string errorText(DWORD error)
{
    if (!error) {
        return "pipe connection broken";
    }

    std::string text = "pipe connection broken (error " + std::to_string(error) + ")";
    const std::string message = platform::windows::TranslateError(error);
    if (!message.empty()) {
        text += ": ";
        text += message;
    }
    return text;
}

// Records that the connection is gone. Throwing is opt-in, and this is the only place that
// decides it, so both classes behave the same way.
void mark_broken(bool& broken, bool throw_on_broken, DWORD error)
{
    broken = true;
    if (throw_on_broken) {
        throw connection_broken(errorText(error));
    }
}

} // namespace

static inline bool isBrokenError(DWORD err)
{
    return err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA || err == ERROR_PIPE_NOT_CONNECTED;
}

// Returns the PIPE_TYPE_* | PIPE_READMODE_* flags for a given mode.
static inline DWORD pipeModeFlags(mode pipe_mode)
{
    switch (pipe_mode) {
    case mode::Message:
    case mode::MessageChunk:
        return PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT;
    case mode::Byte:
    default:
        return PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;
    }
}

// For client handles, the read mode may need to be set explicitly after CreateFile.
static inline DWORD pipeReadModeFlag(mode pipe_mode)
{
    return (pipe_mode == mode::Byte) ? PIPE_READMODE_BYTE : PIPE_READMODE_MESSAGE;
}

static inline bool applyPipeReadMode(winhandle_t hpipe, mode pipe_mode)
{
    DWORD readMode = pipeReadModeFlag(pipe_mode);
    return SetNamedPipeHandleState(H(hpipe), &readMode, nullptr, nullptr) != 0;
}

server_client::server_client(winhandle_t hpipe, size_t buffer_size, mode pipe_mode,
                             winhandle_t cancel_event)
    : m_pipe(hpipe),
      m_cancel_event(as_winhandle(CreateEventA(nullptr, TRUE, FALSE, nullptr))),
      m_shared_cancel_event(cancel_event),
      m_mode(pipe_mode),
      m_buffer_size(buffer_size)
{
}

server_client::~server_client()
{
    reset();
}

server_client::server_client(server_client&& another)
    : m_pipe(another.m_pipe),
      m_cancel_event(another.m_cancel_event),
      m_shared_cancel_event(another.m_shared_cancel_event),
      m_read_buffer(std::move(another.m_read_buffer)),
      m_broken(another.m_broken),
      m_throw_on_broken(another.m_throw_on_broken),
      m_message_incomplete(another.m_message_incomplete),
      m_mode(another.m_mode),
      m_buffer_size(another.m_buffer_size)
{
    another.m_pipe = nullptr;
    another.m_cancel_event = nullptr;
    another.m_shared_cancel_event = nullptr;
}

server_client& server_client::operator=(server_client&& another)
{
    if (this != &another) {
        reset();
        m_pipe = another.m_pipe;
        m_cancel_event = another.m_cancel_event;
        m_shared_cancel_event = another.m_shared_cancel_event;
        m_read_buffer = std::move(another.m_read_buffer);
        m_broken = another.m_broken;
        m_throw_on_broken = another.m_throw_on_broken;
        m_message_incomplete = another.m_message_incomplete;
        m_mode = another.m_mode;
        m_buffer_size = another.m_buffer_size;
        another.m_pipe = nullptr;
        another.m_cancel_event = nullptr;
        another.m_shared_cancel_event = nullptr;
    }
    return *this;
}

permissions::permissions()
    : m_preset(permission_preset::Default)
{
}

permissions::permissions(permission_preset preset)
    : m_preset(preset)
{
}

void *permissions::getSecurityDescriptor() const
{
    if(m_preset == permission_preset::None) {
        return nullptr;
    }

    switch(m_preset) {
    case permission_preset::Default:
        return nullptr; // 使用默认安全描述符
    case permission_preset::Everyone: {
        // Use the well-known SID for "Everyone" (S-1-1-0) instead of a locale-dependent
        // name like L"Everyone". On non-English Windows the name lookup in
        // SetEntriesInAcl fails silently, causing getSecurityDescriptor() to return
        // nullptr, which makes CreateNamedPipeA fall back to the process default SD
        // (typically Administrator-only for privileged services).
        PSID pEveryoneSid = nullptr;
        SID_IDENTIFIER_AUTHORITY worldAuth = SECURITY_WORLD_SID_AUTHORITY;
        if (!AllocateAndInitializeSid(&worldAuth, 1, SECURITY_WORLD_RID,
                                      0, 0, 0, 0, 0, 0, 0, &pEveryoneSid)) {
            return nullptr;
        }

        EXPLICIT_ACCESS ea = {};
        ea.grfAccessPermissions = GENERIC_READ | GENERIC_WRITE;
        ea.grfAccessMode        = SET_ACCESS;
        ea.grfInheritance       = NO_INHERITANCE;
        ea.Trustee.TrusteeForm  = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType  = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea.Trustee.ptstrName    = (LPTSTR)pEveryoneSid;

        PACL acl = nullptr;
        PSECURITY_DESCRIPTOR sd = nullptr;

        if (SetEntriesInAcl(1, &ea, nullptr, &acl) == ERROR_SUCCESS) {
            // The DACL goes into the same block as the descriptor, right after it (20 bytes,
            // still DWORD aligned), so that the caller only has to free what it is given.
            // SetEntriesInAcl allocates the ACL separately, and a descriptor only points at
            // its DACL - freeing the descriptor alone would leave that block behind.
            const DWORD aclSize = acl->AclSize;
            sd = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH + aclSize);
            if (sd) {
                PACL embedded = (PACL)((BYTE*)sd + SECURITY_DESCRIPTOR_MIN_LENGTH);
                memcpy(embedded, acl, aclSize);
                if (InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION) &&
                    SetSecurityDescriptorDacl(sd, TRUE, embedded, FALSE)) {
                    FreeSid(pEveryoneSid);
                    LocalFree(acl);
                    return sd;
                }
                LocalFree(sd);
            }
            LocalFree(acl);
        }
        FreeSid(pEveryoneSid);
        return nullptr;
    }
    case permission_preset::None:
    default:
        return nullptr;
    }
}

bool server_client::valid()
{
    return (is_open(m_pipe) && !m_broken);
}

bool server_client::broken() const
{
    return m_broken;
}

void server_client::setThrowOnBroken(bool enabled)
{
    m_throw_on_broken = enabled;
}

bool server_client::throwOnBroken() const
{
    return m_throw_on_broken;
}

void server_client::cancel()
{
    if (is_open(m_cancel_event)) {
        SetEvent(H(m_cancel_event));
    }
}

bool server_client::cancelled() const
{
    return cancel_signalled(m_cancel_event, m_shared_cancel_event);
}

size_t server_client::appendCancelEvents(void** waits, size_t count) const
{
    // The server's stop event is a second way to be cancelled: server::stop() signals the one
    // copy every connection it handed out holds.
    return append_cancel_events(m_cancel_event, m_shared_cancel_event, waits, static_cast<DWORD>(count));
}

size_t server_client::available()
{
    // Return buffered data + pipe data
    DWORD bytesAvailable = 0;
    if (PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
        return m_read_buffer.size() + bytesAvailable;
    }

    // The only reason this can fail is that the connection is gone, so even a plain look
    // records it.
    const DWORD error = GetLastError();
    if (isBrokenError(error)) {
        mark_broken(m_broken, m_throw_on_broken, error);
    }
    return m_read_buffer.size();
}

scl2::bytearray server_client::read(size_t bytes)
{
    // In Message mode, read() is intentionally disabled:
    // a partial read would silently discard the rest of the message.
    // Call readAll() to consume a complete message.
    if (m_mode == mode::Message) {
        return scl2::bytearray();
    }

    scl2::bytearray result;
    
    // First, read from internal buffer
    if (!m_read_buffer.empty()) {
        size_t fromBuffer = (std::min)(bytes, m_read_buffer.size());
        result.append(m_read_buffer.data(), fromBuffer);
        
        // Remove consumed bytes from buffer
        if (fromBuffer < m_read_buffer.size()) {
            scl2::bytearray remaining(m_read_buffer.data() + fromBuffer, m_read_buffer.size() - fromBuffer);
            m_read_buffer = remaining;
        } else {
            m_read_buffer.clear();
        }
        
        bytes -= fromBuffer;
        
        if (bytes == 0) {
            return result;
        }
    }
    
    // Then read from pipe
    DWORD bytesRead = 0;
    scl2::bytearray buffer;
    buffer.resize(bytes);

    if (!ReadFile(H(m_pipe), buffer.data(), static_cast<DWORD>(bytes), &bytesRead, nullptr)) {
        if (isBrokenError(GetLastError())) mark_broken(m_broken, m_throw_on_broken, GetLastError());
        return result;
    }
    
    if (bytesRead > 0) {
        buffer.resize(bytesRead);
        result.append(buffer.data(), bytesRead);
    }
    
    return result;
}

scl2::bytearray server_client::readAll()
{
    scl2::bytearray result;
    
    // First, get buffered data (applies to all modes)
    if (!m_read_buffer.empty()) {
        result = m_read_buffer;
        m_read_buffer.clear();
    }

    if (m_mode == mode::Message || m_mode == mode::MessageChunk) {
        // A wait may have pulled a whole message in already, and then there is nothing left to
        // read - reading on would block until the next message arrives.
        if (!result.empty() && !m_message_incomplete) {
            return result;
        }

        // In message mode, ReadFile returns ERROR_MORE_DATA when the buffer is smaller
        // than the message. Loop until one complete message is read (ReadFile returns TRUE)
        // or a real error occurs. Do NOT use PeekNamedPipe here; its bytesAvailable only
        // reflects the first chunk and would cause us to loop endlessly on large messages.
        while (true) {
            scl2::bytearray chunk;
            chunk.resize(m_buffer_size);
            DWORD bytesRead = 0;
            BOOL ok = ReadFile(H(m_pipe), chunk.data(), static_cast<DWORD>(m_buffer_size), &bytesRead, nullptr);
            if (bytesRead > 0) {
                chunk.resize(bytesRead);
                result.append(chunk.data(), bytesRead);
            }
            if (ok) {
                m_message_incomplete = false;
                break; // complete message consumed
            }
            DWORD err = GetLastError();
            if (err == ERROR_MORE_DATA) {
                m_message_incomplete = true;
                continue; // more chunks of this message remain
            }
            if (isBrokenError(err)) mark_broken(m_broken, m_throw_on_broken, err);
            break;
        }
        return result;
    }

    // Byte mode: drain all currently available data
    DWORD bytesAvailable = 0;
    if (!PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
        if (isBrokenError(GetLastError())) mark_broken(m_broken, m_throw_on_broken, GetLastError());
        return result;
    }

    while (bytesAvailable > 0) {
        DWORD toRead = static_cast<DWORD>((bytesAvailable > m_buffer_size) ? m_buffer_size : bytesAvailable);
        scl2::bytearray buffer;
        buffer.resize(toRead);
        DWORD bytesRead = 0;
        
        if (!ReadFile(H(m_pipe), buffer.data(), toRead, &bytesRead, nullptr)) {
            if (isBrokenError(GetLastError())) mark_broken(m_broken, m_throw_on_broken, GetLastError());
            break;
        }

        if (bytesRead == 0) {
            // A blocking read that answers with nothing means the other end closed.
            mark_broken(m_broken, m_throw_on_broken, ERROR_BROKEN_PIPE);
            break;
        }

        buffer.resize(bytesRead);
        result.append(buffer.data(), bytesRead);

        if (!PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
            if (isBrokenError(GetLastError())) mark_broken(m_broken, m_throw_on_broken, GetLastError());
            break;
        }
    }

    return result;
}

bool server_client::readyRead()
{
    return available() > 0;
}

bool server_client::waitForReadyRead(std::chrono::milliseconds timeout)
{
    if (!is_open(m_pipe)) {
        return false;
    }

    // Being asked to stop wins over data that is already waiting: a cancelled connection is on
    // its way out, and the caller is not going to handle what is left.
    if (cancelled()) {
        return false;
    }

    // Quick check: if buffer already has data, return immediately
    if (!m_read_buffer.empty()) {
        return true;
    }

    // Quick check: if pipe has data available, return immediately
    DWORD bytesAvailable = 0;
    if (PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
        if (bytesAvailable > 0) {
            return true;
        }
    } else if (isBrokenError(GetLastError())) {
        mark_broken(m_broken, m_throw_on_broken, GetLastError());
        return false;
    }

    // One path for both modes: an overlapped read brings the data into the buffer, and the wait
    // covers the timeout and both cancel events. Message mode used to poll every 5 ms instead so
    // that it never touched the pipe - readAll() reassembles from the buffer, so it may.

    // Create an event for overlapped read
    HANDLE hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!hEvent || hEvent == INVALID_HANDLE_VALUE) {
        return false;
    }

    OVERLAPPED overlapped = {};
    ZeroMemory(&overlapped, sizeof(OVERLAPPED));
    overlapped.hEvent = hEvent;

    // Prepare buffer for reading
    scl2::bytearray tempBuffer;
    tempBuffer.resize(m_buffer_size);
    DWORD bytesRead = 0;

    // Initiate overlapped read
    BOOL readResult = ReadFile(H(m_pipe), tempBuffer.data(), static_cast<DWORD>(m_buffer_size), &bytesRead, &overlapped);
    
    // Wait for the read to complete, or for this connection to be cancelled. A cancel and a
    // timeout land in the same place below: the operation is dropped and false is answered.
    HANDLE waits[3] = { hEvent, nullptr, nullptr };
    const DWORD wait_count = static_cast<DWORD>(appendCancelEvents(waits, 1));

    DWORD dwTimeout = (timeout.count() < 0) ? INFINITE : static_cast<DWORD>(timeout.count());
    
    if (!readResult) {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_IO_PENDING) {
            // Operation is pending, wait for it
            DWORD dwWaitResult = WaitForMultipleObjects(wait_count, waits, FALSE, dwTimeout);
            
            if (dwWaitResult == WAIT_OBJECT_0) {
                // Get the result
                DWORD gorErr = 0;
                if (GetOverlappedResult(H(m_pipe), &overlapped, &bytesRead, FALSE)) {
                    gorErr = 0; // complete
                } else {
                    gorErr = GetLastError();
                }
                // ERROR_MORE_DATA means the buffer was too small for the message -
                // data WAS written; treat as success (readAll will reassemble the rest).
                if (bytesRead > 0 && (gorErr == 0 || gorErr == ERROR_MORE_DATA)) {
                    tempBuffer.resize(bytesRead);
                    m_read_buffer.append(tempBuffer.data(), bytesRead);
                    m_message_incomplete = (gorErr == ERROR_MORE_DATA);
                    CloseHandle(hEvent);
                    return true;
                }
                if (gorErr != 0 && gorErr != ERROR_MORE_DATA) {
                    if (isBrokenError(gorErr)) mark_broken(m_broken, m_throw_on_broken, gorErr);
                }
            }
            
            // Giving up does not throw away what arrived: the read may have completed just
            // before, and those bytes are already out of the pipe.
            DWORD arrived = 0;
            DWORD arrivalError = 0;
            if (!GetOverlappedResult(H(m_pipe), &overlapped, &arrived, FALSE)) {
                arrivalError = GetLastError();
            }
            if (arrived > 0 && (arrivalError == 0 || arrivalError == ERROR_MORE_DATA)) {
                tempBuffer.resize(arrived);
                m_read_buffer.append(tempBuffer.data(), arrived);
                m_message_incomplete = (arrivalError == ERROR_MORE_DATA);
            }

            // Timeout, cancel or error - the read is dropped either way.
            CancelIoEx(H(m_pipe), &overlapped);
            CloseHandle(hEvent);
            return false;
        } else {
            // Immediate error
            if (isBrokenError(dwError)) mark_broken(m_broken, m_throw_on_broken, dwError);
            CloseHandle(hEvent);
            return false;
        }
    }

    // ReadFile succeeded immediately
    // Even for immediate success in overlapped mode, use GetOverlappedResult for accurate bytesRead
    {
        DWORD gorErr = 0;
        if (!GetOverlappedResult(H(m_pipe), &overlapped, &bytesRead, FALSE)) {
            gorErr = GetLastError();
        }
        if (bytesRead > 0 && (gorErr == 0 || gorErr == ERROR_MORE_DATA)) {
            tempBuffer.resize(bytesRead);
            m_read_buffer.append(tempBuffer.data(), bytesRead);
            m_message_incomplete = (gorErr == ERROR_MORE_DATA);
            CloseHandle(hEvent);
            return true;
        }
        if (gorErr != 0 && gorErr != ERROR_MORE_DATA) {
            if (isBrokenError(gorErr)) mark_broken(m_broken, m_throw_on_broken, gorErr);
        }
    }
    
    CloseHandle(hEvent);
    return false;
}

size_t server_client::write(const scl2::bytearray &data)
{
    DWORD bytesWritten;
    if (!WriteFile(H(m_pipe), data.data(), DWORD(data.size()), &bytesWritten, nullptr)) {
        if (isBrokenError(GetLastError())) mark_broken(m_broken, m_throw_on_broken, GetLastError());
        return 0;
    }
    return bytesWritten;
}

bool server_client::waitForFinished(std::chrono::milliseconds timeout)
{
    if (!is_open(m_pipe)) {
        return false;
    }

    DWORD error = 0;
    const bool finished = flush_pipe(m_pipe, timeout, m_cancel_event, m_shared_cancel_event, &error);
    if (isBrokenError(error)) {
        mark_broken(m_broken, m_throw_on_broken, error);
    }
    return finished;
}

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4996)  // the acknowledge helpers are deprecated
#endif

bool server_client::acknowledge()
{
    scl2::bytearray ack;
    ack.append(acknowledge_token);
    return write(ack) == 1;
}

bool server_client::waitForAcknowledged(std::chrono::milliseconds timeout)
{
    if (!waitForReadyRead(timeout)) {
        return false;
    }

    scl2::bytearray data = readAll();
    return data.size() == 1 && data.data()[0] == acknowledge_token;
}

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

bool server_client::close()
{
    const bool was_open = is_open(m_pipe);
    reset();
    return was_open;
}

bool server_client::reset()
{
    if (!is_open(m_pipe)) {
        return true;    // already reset is not a failure
    }

    DisconnectNamedPipe(H(m_pipe));
    CloseHandle(H(m_pipe));
    m_pipe = nullptr;

    // The cancel events belong to this connection, and a released one is of no use to anyone.
    if (is_open(m_cancel_event)) {
        CloseHandle(H(m_cancel_event));
        m_cancel_event = nullptr;
    }
    if (is_open(m_shared_cancel_event)) {
        CloseHandle(H(m_shared_cancel_event));
        m_shared_cancel_event = nullptr;
    }

    m_message_incomplete = false;   // a released connection has no message in progress
    return true;
}

size_t server_client::bufferSize() const
{
    return m_buffer_size;
}

server::server(const std::string &name, const permissions &permissions)
    : m_name(name), m_permissions(permissions), m_pipe(nullptr),
      m_connect_event(nullptr), m_stop_event(nullptr), m_overlapped_connect(nullptr)
{
}

server::~server()
{
    stop();
    cleanup();
}

bool server::start()
{
    // Starting from any state replaces whatever was there: a second start() used to overwrite
    // the handles instead of closing them.
    cleanup();

    m_stopped = false; // We do allow restarting a stopped server, so reset the flag here.
    
    // Create stop event for clean shutdown
    m_stop_event = as_winhandle(CreateEventA(nullptr, TRUE, FALSE, nullptr));
    if (!is_open(m_stop_event)) {
        return false;
    }

    // Create event for connect notification
    m_connect_event = as_winhandle(CreateEventA(nullptr, TRUE, FALSE, nullptr));
    if (!is_open(m_connect_event)) {
        CloseHandle(H(m_stop_event));
        m_stop_event = nullptr;
        return false;
    }

    // Allocate OVERLAPPED structure for async connect
    m_overlapped_connect = new OVERLAPPED();
    if (!m_overlapped_connect) {
        CloseHandle(H(m_stop_event));
        m_stop_event = nullptr;
        CloseHandle(H(m_connect_event));
        m_connect_event = nullptr;
        return false;
    }

    ZeroMemory(m_overlapped_connect, sizeof(OVERLAPPED));
    ov(m_overlapped_connect)->hEvent = m_connect_event;

    PSECURITY_DESCRIPTOR sd = m_permissions.getSecurityDescriptor();
    SECURITY_ATTRIBUTES sa = {0};

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = sd;
    sa.bInheritHandle = FALSE;

    m_pipe = as_winhandle(CreateNamedPipeA(
        m_name.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,  // Enable overlapped I/O
        pipeModeFlags(m_mode),
        m_client_limit + 1,
        static_cast<DWORD>(m_buffer_size), static_cast<DWORD>(m_buffer_size),
        NMPWAIT_USE_DEFAULT_WAIT,
        sd ? &sa : nullptr
    ));

    if (sd) {
        LocalFree(sd);
    }

    if (!is_open(m_pipe)) {
        delete ov(m_overlapped_connect);
        m_overlapped_connect = nullptr;
        CloseHandle(H(m_connect_event));
        m_connect_event = nullptr;
        CloseHandle(H(m_stop_event));
        m_stop_event = nullptr;
        return false;
    }

    // Initiate first async connect
    BOOL connectResult = ConnectNamedPipe(H(m_pipe), ov(m_overlapped_connect));
    if (!connectResult) {
        DWORD dwError = GetLastError();
        if (dwError != ERROR_IO_PENDING && dwError != ERROR_PIPE_CONNECTED) {
            CloseHandle(H(m_pipe));
            m_pipe = nullptr;
            delete ov(m_overlapped_connect);
            m_overlapped_connect = nullptr;
            CloseHandle(H(m_connect_event));
            m_connect_event = nullptr;
            CloseHandle(H(m_stop_event));
            m_stop_event = nullptr;
            return false;
        }
        // ERROR_IO_PENDING is expected for overlapped operation
        // ERROR_PIPE_CONNECTED means client already connected
    }

    return true;
}

server_client server::queryNextConnection()
{
    if (!is_open(m_connect_event)) {
        return server_client(as_winhandle(INVALID_HANDLE_VALUE), m_buffer_size, m_mode, nullptr);
    }

    // Check if the connect event is signaled (connection completed)
    DWORD dwWaitResult = WaitForSingleObject(H(m_connect_event), 0);
    
    if (dwWaitResult != WAIT_OBJECT_0) {
        // No pending connection ready
        return server_client(nullptr, m_buffer_size, m_mode, nullptr);
    }

    // Connection is ready - prepare to return it to caller
    winhandle_t hConnectedPipe = m_pipe;

    // The connection gets its own handle to the stop event, so that stop() reaches every worker
    // that is waiting on a connection this server handed out - without the server keeping a
    // registry of them, and without a worker outliving the server's own handle.
    winhandle_t cancel_event = nullptr;
    if (is_open(m_stop_event)) {
        HANDLE duplicate = nullptr;
        if (DuplicateHandle(GetCurrentProcess(), H(m_stop_event), GetCurrentProcess(), &duplicate,
                            0, FALSE, DUPLICATE_SAME_ACCESS)) {
            cancel_event = as_winhandle(duplicate);
        }
    }

    // Create new pipe instance for next connection
    ResetEvent(H(m_connect_event));
    
    PSECURITY_DESCRIPTOR sd = m_permissions.getSecurityDescriptor();
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = sd;
    sa.bInheritHandle = FALSE;

    m_pipe = as_winhandle(CreateNamedPipeA(
        m_name.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        pipeModeFlags(m_mode),
        m_client_limit + 1,
        static_cast<DWORD>(m_buffer_size), static_cast<DWORD>(m_buffer_size),
        NMPWAIT_USE_DEFAULT_WAIT,
        sd ? &sa : nullptr
    ));

    if (sd) {
        LocalFree(sd);
    }

    if (!is_open(m_pipe)) {
        return server_client(hConnectedPipe, m_buffer_size, m_mode, cancel_event);
    }

    // Initiate async connect on new pipe instance
    BOOL connectResult = ConnectNamedPipe(H(m_pipe), ov(m_overlapped_connect));
    if (!connectResult) {
        DWORD dwError = GetLastError();
        if (dwError != ERROR_IO_PENDING && dwError != ERROR_PIPE_CONNECTED) {
            // Connection initiation failed, but return the established connection anyway
        }
    }

    return server_client(hConnectedPipe, m_buffer_size, m_mode, cancel_event);
}

bool server::hasPendingConnection() const
{
    if (!is_open(m_connect_event)) {
        return false;
    }

    // Check if connect event is signaled (non-blocking)
    DWORD dwWaitResult = WaitForSingleObject(H(m_connect_event), 0);
    return (dwWaitResult == WAIT_OBJECT_0);
}

bool server::waitForNextConnection(std::chrono::milliseconds timeout) const
{
    if (!is_open(m_connect_event)) {
        return false;
    }

    if (!is_open(m_stop_event)) {
        return false;
    }

    // Wait for either connect event or stop event
    HANDLE events[2] = { m_connect_event, m_stop_event };
    DWORD dwWaitResult = WaitForMultipleObjects(2, events, FALSE, static_cast<DWORD>(timeout.count()));
    
    // WAIT_OBJECT_0 means connect event signaled
    // WAIT_OBJECT_0 + 1 means stop event signaled
    return (dwWaitResult == WAIT_OBJECT_0);
}

void server::setBufferSize(size_t size)
{
    if(is_open(m_pipe)) {
        throw std::runtime_error("Cannot change buffer size while server is active");
    }
    m_buffer_size = size;
}

size_t server::bufferSize() const
{
    return m_buffer_size;
}

void server::setPermissions(const permissions &permissions)
{
    if(is_open(m_pipe)) {
        throw std::runtime_error("Cannot change permissions while server is active");
    }
    m_permissions = permissions;
}

permissions server::getPermissions() const
{
    return m_permissions;
}

void server::setPipeMode(mode pipe_mode)
{
    if(is_open(m_pipe)) {
        throw std::runtime_error("Cannot change pipe mode while server is active");
    }
    m_mode = pipe_mode;
}

mode server::getPipeMode() const
{
    return m_mode;
}

void server::setClientLimit(int limit)
{
    if(is_open(m_pipe)) {
        throw std::runtime_error("Cannot change the client limit while server is active");
    }

    // The extra +1 slot is reserved for the brief overlap during queryNextConnection(), where
    // the just-accepted handle and the new listening handle exist at the same time, so
    // maximumClientLimit is the largest limit that still fits in Windows' 255 instances.
    if (limit <= 0 || limit > maximumClientLimit) {
        throw std::out_of_range("Client limit must be between 1 and maximumClientLimit (254)");
    }

    m_client_limit = limit;
}

int server::clientLimit() const
{
    return m_client_limit;
}

bool server::stop()
{
    m_stopped = true;

    // Signal the stop event to unblock waiting threads, and to release the connections this
    // server handed out: they hold a copy of this event and watch it while they wait.
    if (is_open(m_stop_event)) {
        SetEvent(m_stop_event);
    }

    // Cancel any pending I/O operations
    if (is_open(m_pipe)) {
        CancelIoEx(H(m_pipe), nullptr);
    }

    return true;
}

bool server::cleanup()
{
    // Cancel pending operations and close main pipe
    if (is_open(m_pipe)) {
        CancelIoEx(H(m_pipe), nullptr);
        CloseHandle(H(m_pipe));
        m_pipe = nullptr;
    }

    // Clean up overlapped structure
    if (m_overlapped_connect) {
        delete ov(m_overlapped_connect);
        m_overlapped_connect = nullptr;
    }

    // Close connect event
    if (is_open(m_connect_event)) {
        CloseHandle(H(m_connect_event));
        m_connect_event = nullptr;
    }

    // Close stop event
    if (is_open(m_stop_event)) {
        CloseHandle(H(m_stop_event));
        m_stop_event = nullptr;
    }

    return true;
}

bool server::active() const
{
    return (is_open(m_pipe));
}

bool server::stopped() const
{
    return m_stopped;
}

// Client

client::client(const std::string& name)
    : m_name(name),
      m_pipe(nullptr),
      m_cancel_event(as_winhandle(CreateEventA(nullptr, TRUE, FALSE, nullptr)))
{
}

client::~client()
{
    reset();

    if (is_open(m_cancel_event)) {
        CloseHandle(H(m_cancel_event));
        m_cancel_event = nullptr;
    }
}

client::client(client&& another)
    : m_name(another.m_name),
      m_pipe(another.m_pipe),
      m_cancel_event(another.m_cancel_event),
      m_read_buffer(std::move(another.m_read_buffer)),
      m_broken(another.m_broken),
      m_throw_on_broken(another.m_throw_on_broken),
      m_message_incomplete(another.m_message_incomplete),
      m_mode(another.m_mode),
      m_buffer_size(another.m_buffer_size)
{
    another.m_pipe = nullptr;
    another.m_cancel_event = nullptr;
}

client& client::operator=(client&& another)
{
    if (this != &another) {
        reset();
        m_name = another.m_name;
        m_pipe = another.m_pipe;
        m_cancel_event = another.m_cancel_event;
        m_read_buffer = std::move(another.m_read_buffer);
        m_broken = another.m_broken;
        m_throw_on_broken = another.m_throw_on_broken;
        m_message_incomplete = another.m_message_incomplete;
        m_mode = another.m_mode;
        m_buffer_size = another.m_buffer_size;
        another.m_pipe = nullptr;
        another.m_cancel_event = nullptr;
    }
    return *this;
}

bool client::connect(std::chrono::milliseconds timeout)
{
    if (is_open(m_pipe)) {
        return true; // Already connected
    }

    const bool infiniteWait = (timeout.count() < 0);
    auto deadline = std::chrono::steady_clock::now() + timeout;
    constexpr auto retry_interval = std::chrono::milliseconds(100);

    // Loop until timeout, trying to connect
    while (true) {
        if (cancelled()) {
            return false;
        }

        // Try to open the pipe
        m_pipe = as_winhandle(CreateFileA(
            m_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,  // No share access
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,  // Support overlapped I/O
            nullptr
        ));

        if (is_open(m_pipe)) {
            // Detect the server's pipe mode so read/readAll behave correctly
            DWORD pipeFlags = 0;
            if (GetNamedPipeInfo(H(m_pipe), &pipeFlags, nullptr, nullptr, nullptr)) {
                m_mode = (pipeFlags & PIPE_TYPE_MESSAGE) ? mode::Message : mode::Byte;
            }

            // Ensure this handle reads in the intended mode (especially important for Message).
            if (!applyPipeReadMode(m_pipe, m_mode)) {
                CloseHandle(H(m_pipe));
                m_pipe = nullptr;
                return false;
            }
            return true; // Successfully connected
        }

        auto now = std::chrono::steady_clock::now();
        if (!infiniteWait && now >= deadline) {
            return false;
        }

        // Handle expected transient errors while waiting for server/instance
        DWORD dwError = GetLastError();
        if (dwError == ERROR_FILE_NOT_FOUND) {
            auto sleepTime = retry_interval;
            if (!infiniteWait) {
                auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
                if (remain <= std::chrono::milliseconds(0)) {
                    return false;
                }
                sleepTime = (std::min)(sleepTime, remain);
            }
            std::this_thread::sleep_for(sleepTime);
            continue;
        }

        if (dwError == ERROR_PIPE_BUSY) {
            DWORD waitMs = 100;
            if (infiniteWait) {
                waitMs = NMPWAIT_WAIT_FOREVER;
            } else {
                auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
                if (remain <= std::chrono::milliseconds(0)) {
                    return false;
                }
                waitMs = static_cast<DWORD>((std::min)(remain, std::chrono::milliseconds(100)).count());
            }

            if (WaitNamedPipeA(m_name.c_str(), waitMs)) {
                continue;
            }

            DWORD waitError = GetLastError();
            if (waitError == ERROR_SEM_TIMEOUT || waitError == ERROR_FILE_NOT_FOUND || waitError == ERROR_PIPE_BUSY) {
                continue;
            }

            return false;
        }

        // Non-retriable error
        return false;
    }
}

bool client::waitForConnection(std::chrono::milliseconds timeout)
{
    return connect(timeout);
}

bool client::serverExists() const
{
    if (WaitNamedPipeA(m_name.c_str(), 0)) {
        return true;
    }

    DWORD err = GetLastError();
    if (err == ERROR_FILE_NOT_FOUND || err == ERROR_BAD_PATHNAME) {
        return false;
    }

    return true;
}

mode client::pipeMode() const
{
    return m_mode;
}

void client::setPipeMode(mode pipe_mode)
{
    m_mode = pipe_mode;

    if (is_open(m_pipe)) {
        applyPipeReadMode(m_pipe, m_mode);
    }
}

bool client::valid()
{
    return (is_open(m_pipe) && !m_broken);
}

bool client::broken() const
{
    return m_broken;
}

void client::setThrowOnBroken(bool enabled)
{
    m_throw_on_broken = enabled;
}

bool client::throwOnBroken() const
{
    return m_throw_on_broken;
}

void client::cancel()
{
    if (is_open(m_cancel_event)) {
        SetEvent(H(m_cancel_event));
    }
}

bool client::cancelled() const
{
    return cancel_signalled(m_cancel_event, nullptr);
}

size_t client::appendCancelEvents(void** waits, size_t count) const
{
    return append_cancel_events(m_cancel_event, nullptr, waits, static_cast<DWORD>(count));
}

bool client::readyRead()
{
    return available() > 0;
}

bool client::waitForReadyRead(std::chrono::milliseconds timeout)
{
    if (!is_open(m_pipe)) {
        return false;
    }

    // Being asked to stop wins over data that is already waiting.
    if (cancelled()) {
        return false;
    }

    // Quick check: if buffer already has data, return immediately
    if (!m_read_buffer.empty()) {
        return true;
    }

    // Quick check: if pipe has data available, return immediately
    DWORD bytesAvailable = 0;
    if (PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
        if (bytesAvailable > 0) {
            return true;
        }
    } else if (isBrokenError(GetLastError())) {
        mark_broken(m_broken, m_throw_on_broken, GetLastError());
        return false;
    }

    // One path for both modes: an overlapped read brings the data into the buffer, and the wait
    // covers the timeout and both cancel events. Message mode used to poll every 5 ms instead so
    // that it never touched the pipe - readAll() reassembles from the buffer, so it may.

    // Create an event for overlapped read
    HANDLE hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!hEvent || hEvent == INVALID_HANDLE_VALUE) {
        return false;
    }

    OVERLAPPED overlapped = {};
    ZeroMemory(&overlapped, sizeof(OVERLAPPED));
    overlapped.hEvent = hEvent;

    // Prepare buffer for reading
    scl2::bytearray tempBuffer;
    tempBuffer.resize(m_buffer_size);
    DWORD bytesRead = 0;

    // Initiate overlapped read
    BOOL readResult = ReadFile(H(m_pipe), tempBuffer.data(), static_cast<DWORD>(m_buffer_size), &bytesRead, &overlapped);
    
    // Wait for the read to complete, or for this connection to be cancelled. A cancel and a
    // timeout land in the same place below: the operation is dropped and false is answered.
    HANDLE waits[3] = { hEvent, nullptr, nullptr };
    const DWORD wait_count = static_cast<DWORD>(appendCancelEvents(waits, 1));

    DWORD dwTimeout = (timeout.count() < 0) ? INFINITE : static_cast<DWORD>(timeout.count());
    
    if (!readResult) {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_IO_PENDING) {
            // Operation is pending, wait for it
            DWORD dwWaitResult = WaitForMultipleObjects(wait_count, waits, FALSE, dwTimeout);
            
            if (dwWaitResult == WAIT_OBJECT_0) {
                // Get the result
                DWORD gorErr = 0;
                if (GetOverlappedResult(H(m_pipe), &overlapped, &bytesRead, FALSE)) {
                    gorErr = 0;
                } else {
                    gorErr = GetLastError();
                }
                // ERROR_MORE_DATA: buffer too small for message, but data was written - treat as success
                if (bytesRead > 0 && (gorErr == 0 || gorErr == ERROR_MORE_DATA)) {
                    tempBuffer.resize(bytesRead);
                    m_read_buffer.append(tempBuffer.data(), bytesRead);
                    m_message_incomplete = (gorErr == ERROR_MORE_DATA);
                    CloseHandle(hEvent);
                    return true;
                }
                if (gorErr != 0 && gorErr != ERROR_MORE_DATA) {
                    if (isBrokenError(gorErr)) mark_broken(m_broken, m_throw_on_broken, gorErr);
                }
            }
            
            // Giving up does not throw away what arrived: the read may have completed just
            // before, and those bytes are already out of the pipe.
            DWORD arrived = 0;
            DWORD arrivalError = 0;
            if (!GetOverlappedResult(H(m_pipe), &overlapped, &arrived, FALSE)) {
                arrivalError = GetLastError();
            }
            if (arrived > 0 && (arrivalError == 0 || arrivalError == ERROR_MORE_DATA)) {
                tempBuffer.resize(arrived);
                m_read_buffer.append(tempBuffer.data(), arrived);
                m_message_incomplete = (arrivalError == ERROR_MORE_DATA);
            }

            // Timeout, cancel or error - the read is dropped either way.
            CancelIoEx(H(m_pipe), &overlapped);
            CloseHandle(hEvent);
            return false;
        } else {
            // Immediate error
            if (isBrokenError(dwError)) mark_broken(m_broken, m_throw_on_broken, dwError);
            CloseHandle(hEvent);
            return false;
        }
    }

    // ReadFile succeeded immediately
    // Even for immediate success in overlapped mode, use GetOverlappedResult for accurate bytesRead
    {
        DWORD gorErr = 0;
        if (!GetOverlappedResult(H(m_pipe), &overlapped, &bytesRead, FALSE)) {
            gorErr = GetLastError();
        }
        if (bytesRead > 0 && (gorErr == 0 || gorErr == ERROR_MORE_DATA)) {
            tempBuffer.resize(bytesRead);
            m_read_buffer.append(tempBuffer.data(), bytesRead);
            m_message_incomplete = (gorErr == ERROR_MORE_DATA);
            CloseHandle(hEvent);
            return true;
        }
        if (gorErr != 0 && gorErr != ERROR_MORE_DATA) {
            if (isBrokenError(gorErr)) mark_broken(m_broken, m_throw_on_broken, gorErr);
        }
    }
    
    CloseHandle(hEvent);
    return false;
}

size_t client::available()
{
    if (!is_open(m_pipe)) {
        return m_read_buffer.size();
    }

    DWORD bytesAvailable = 0;
    if (PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
        return m_read_buffer.size() + bytesAvailable;
    }

    // The only reason this can fail is that the connection is gone, so even a plain look
    // records it.
    const DWORD error = GetLastError();
    if (isBrokenError(error)) {
        mark_broken(m_broken, m_throw_on_broken, error);
    }
    return m_read_buffer.size();
}

scl2::bytearray client::read(size_t bytes)
{
    if (!is_open(m_pipe)) {
        return scl2::bytearray();
    }

    // In Message mode, read() is intentionally disabled:
    // a partial read would silently discard the rest of the message.
    // Call readAll() to consume a complete message.
    if (m_mode == mode::Message) {
        return scl2::bytearray();
    }

    scl2::bytearray result;
    
    // First, read from internal buffer
    if (!m_read_buffer.empty()) {
        size_t fromBuffer = (std::min)(bytes, m_read_buffer.size());
        result.append(m_read_buffer.data(), fromBuffer);
        
        // Remove consumed bytes from buffer
        if (fromBuffer < m_read_buffer.size()) {
            scl2::bytearray remaining(m_read_buffer.data() + fromBuffer, m_read_buffer.size() - fromBuffer);
            m_read_buffer = remaining;
        } else {
            m_read_buffer.clear();
        }
        
        bytes -= fromBuffer;
        
        if (bytes == 0) {
            return result;
        }
    }

    // Then read from pipe
    DWORD bytesRead = 0;
    scl2::bytearray buffer;
    buffer.resize(bytes);

    if (!ReadFile(H(m_pipe), buffer.data(), static_cast<DWORD>(bytes), &bytesRead, nullptr)) {
        if (isBrokenError(GetLastError())) mark_broken(m_broken, m_throw_on_broken, GetLastError());
        return result;
    }

    if (bytesRead > 0) {
        buffer.resize(bytesRead);
        result.append(buffer.data(), bytesRead);
    }

    return result;
}

scl2::bytearray client::readAll()
{
    if (!is_open(m_pipe)) {
        scl2::bytearray result = m_read_buffer;
        m_read_buffer.clear();
        return result;
    }

    scl2::bytearray result;
    
    // First, get buffered data (applies to all modes)
    if (!m_read_buffer.empty()) {
        result = m_read_buffer;
        m_read_buffer.clear();
    }

    if (m_mode == mode::Message || m_mode == mode::MessageChunk) {
        // A wait may have pulled a whole message in already, and then there is nothing left to
        // read - reading on would block until the next message arrives.
        if (!result.empty() && !m_message_incomplete) {
            return result;
        }

        // Loop until one complete message is consumed
        while (true) {
            scl2::bytearray chunk;
            chunk.resize(m_buffer_size);
            DWORD bytesRead = 0;
            BOOL ok = ReadFile(H(m_pipe), chunk.data(), static_cast<DWORD>(m_buffer_size), &bytesRead, nullptr);
            if (bytesRead > 0) {
                chunk.resize(bytesRead);
                result.append(chunk.data(), bytesRead);
            }
            if (ok) {
                m_message_incomplete = false;
                break;
            }
            DWORD err = GetLastError();
            if (err == ERROR_MORE_DATA) {
                m_message_incomplete = true;
                continue;
            }
            if (isBrokenError(err)) mark_broken(m_broken, m_throw_on_broken, err);
            break;
        }
        return result;
    }

    // Byte mode: drain all currently available data
    DWORD bytesAvailable = 0;
    if (!PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
        if (isBrokenError(GetLastError())) mark_broken(m_broken, m_throw_on_broken, GetLastError());
        return result;
    }

    while (bytesAvailable > 0) {
        DWORD toRead = static_cast<DWORD>((bytesAvailable > m_buffer_size) ? m_buffer_size : bytesAvailable);
        scl2::bytearray buffer;
        buffer.resize(toRead);
        DWORD bytesRead = 0;
        
        if (!ReadFile(H(m_pipe), buffer.data(), toRead, &bytesRead, nullptr)) {
            if (isBrokenError(GetLastError())) mark_broken(m_broken, m_throw_on_broken, GetLastError());
            break;
        }

        if (bytesRead == 0) {
            // A blocking read that answers with nothing means the other end closed.
            mark_broken(m_broken, m_throw_on_broken, ERROR_BROKEN_PIPE);
            break;
        }

        buffer.resize(bytesRead);
        result.append(buffer.data(), bytesRead);

        if (!PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
            if (isBrokenError(GetLastError())) mark_broken(m_broken, m_throw_on_broken, GetLastError());
            break;
        }
    }

    return result;
}

size_t client::write(const scl2::bytearray& data)
{
    if (!is_open(m_pipe)) {
        return 0;
    }

    DWORD bytesWritten = 0;
    if (!WriteFile(H(m_pipe), data.data(), DWORD(data.size()), &bytesWritten, nullptr)) {
        if (isBrokenError(GetLastError())) mark_broken(m_broken, m_throw_on_broken, GetLastError());
        return 0;
    }

    return bytesWritten;
}

bool client::waitForFinished(std::chrono::milliseconds timeout)
{
    if (!is_open(m_pipe)) {
        return false;
    }

    DWORD error = 0;
    const bool finished = flush_pipe(m_pipe, timeout, m_cancel_event, nullptr, &error);
    if (isBrokenError(error)) {
        mark_broken(m_broken, m_throw_on_broken, error);
    }
    return finished;
}

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4996)  // the acknowledge helpers are deprecated
#endif

bool client::acknowledge()
{
    scl2::bytearray ack;
    ack.append(acknowledge_token);
    return write(ack) == 1;
}

bool client::waitForAcknowledged(std::chrono::milliseconds timeout)
{
    if (!waitForReadyRead(timeout)) {
        return false;
    }

    scl2::bytearray data = readAll();
    return data.size() == 1 && data.data()[0] == acknowledge_token;
}

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

bool client::close()
{
    const bool was_open = is_open(m_pipe);
    reset();
    return was_open;
}

bool client::reset()
{
    if (is_open(m_pipe)) {
        CloseHandle(H(m_pipe));
        m_pipe = nullptr;
    }

    // A cancel belongs to the connection, so dropping the connection drops it too. The event
    // itself stays: a client can connect again after a reset.
    if (is_open(m_cancel_event)) {
        ResetEvent(H(m_cancel_event));
    }

    m_message_incomplete = false;   // a released connection has no message in progress
    return true;
}

} // namespace scl2::pipe

#endif  // SCL2_PIPE_SUPPORTED