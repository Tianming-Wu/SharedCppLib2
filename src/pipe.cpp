#include "pipe.hpp"

#if SCL2_PIPE_SUPPORTED

#include <accctrl.h>
#include <aclapi.h>

#include <thread>
#include <chrono>
#include <algorithm>

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

server_client::server_client(winhandle_t hpipe, size_t buffer_size, mode pipe_mode)
    : m_pipe(hpipe), buffer_size(buffer_size), m_mode(pipe_mode)
{
}

server_client::~server_client()
{
    cleanup();
}

server_client::server_client(server_client&& another)
    : m_pipe(another.m_pipe), buffer_size(another.buffer_size)
{
    another.m_pipe = nullptr;
}

server_client& server_client::operator=(server_client&& another)
{
    if (this != &another) {
        cleanup();
        m_pipe = another.m_pipe;
        another.m_pipe = nullptr;
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
            sd = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
            if (sd) {
                if (InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION) &&
                    SetSecurityDescriptorDacl(sd, TRUE, acl, FALSE)) {
                    FreeSid(pEveryoneSid);
                    // Note: acl is now owned by sd; caller must LocalFree(sd) then LocalFree(acl)
                    // but since we embed acl into sd here, callers LocalFree(sd) is sufficient
                    // for the descriptor itself. acl is freed below only on failure paths.
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

size_t server_client::available()
{
    // Return buffered data + pipe data
    DWORD bytesAvailable = 0;
    if (PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
        return m_read_buffer.size() + bytesAvailable;
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
        if (isBrokenError(GetLastError())) m_broken = true;
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
        // In message mode, ReadFile returns ERROR_MORE_DATA when the buffer is smaller
        // than the message. Loop until one complete message is read (ReadFile returns TRUE)
        // or a real error occurs. Do NOT use PeekNamedPipe here; its bytesAvailable only
        // reflects the first chunk and would cause us to loop endlessly on large messages.
        while (true) {
            scl2::bytearray chunk;
            chunk.resize(buffer_size);
            DWORD bytesRead = 0;
            BOOL ok = ReadFile(H(m_pipe), chunk.data(), static_cast<DWORD>(buffer_size), &bytesRead, nullptr);
            if (bytesRead > 0) {
                chunk.resize(bytesRead);
                result.append(chunk.data(), bytesRead);
            }
            if (ok) break; // complete message consumed
            DWORD err = GetLastError();
            if (err == ERROR_MORE_DATA) continue; // more chunks of this message remain
            if (isBrokenError(err)) m_broken = true;
            break;
        }
        return result;
    }

    // Byte mode: drain all currently available data
    DWORD bytesAvailable = 0;
    if (!PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
        if (isBrokenError(GetLastError())) m_broken = true;
        return result;
    }

    while (bytesAvailable > 0) {
        DWORD toRead = static_cast<DWORD>((bytesAvailable > buffer_size) ? buffer_size : bytesAvailable);
        scl2::bytearray buffer;
        buffer.resize(toRead);
        DWORD bytesRead = 0;
        
        if (!ReadFile(H(m_pipe), buffer.data(), toRead, &bytesRead, nullptr)) {
            if (isBrokenError(GetLastError())) m_broken = true;
            break;
        }

        if (bytesRead == 0) {
            break;
        }

        buffer.resize(bytesRead);
        result.append(buffer.data(), bytesRead);

        if (!PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
            if (isBrokenError(GetLastError())) m_broken = true;
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
    }

    // In message-based modes, wait without consuming data to preserve strict
    // one-message-per-readAll semantics.
    if (m_mode == mode::Message || m_mode == mode::MessageChunk) {
        const bool infiniteWait = (timeout.count() < 0);
        auto deadline = std::chrono::steady_clock::now() + timeout;
        constexpr auto pollInterval = std::chrono::milliseconds(5);

        while (true) {
            bytesAvailable = 0;
            if (PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
                if (bytesAvailable > 0) {
                    return true;
                }
            } else {
                DWORD err = GetLastError();
                if (isBrokenError(err)) m_broken = true;
                return false;
            }

            if (!infiniteWait) {
                auto now = std::chrono::steady_clock::now();
                if (now >= deadline) {
                    return false;
                }
                auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
                std::this_thread::sleep_for((std::min)(pollInterval, remain));
            } else {
                std::this_thread::sleep_for(pollInterval);
            }
        }
    }

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
    tempBuffer.resize(buffer_size);
    DWORD bytesRead = 0;

    // Initiate overlapped read
    BOOL readResult = ReadFile(H(m_pipe), tempBuffer.data(), static_cast<DWORD>(buffer_size), &bytesRead, &overlapped);
    
    DWORD dwTimeout = (timeout.count() < 0) ? INFINITE : static_cast<DWORD>(timeout.count());
    
    if (!readResult) {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_IO_PENDING) {
            // Operation is pending, wait for it
            DWORD dwWaitResult = WaitForSingleObject(hEvent, dwTimeout);
            
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
                    CloseHandle(hEvent);
                    return true;
                }
                if (gorErr != 0 && gorErr != ERROR_MORE_DATA) {
                    if (isBrokenError(gorErr)) m_broken = true;
                }
            }
            
            // Timeout or error - cancel the operation
            CancelIoEx(H(m_pipe), &overlapped);
            CloseHandle(hEvent);
            return false;
        } else {
            // Immediate error
            if (isBrokenError(dwError)) m_broken = true;
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
            CloseHandle(hEvent);
            return true;
        }
        if (gorErr != 0 && gorErr != ERROR_MORE_DATA) {
            if (isBrokenError(gorErr)) m_broken = true;
        }
    }
    
    CloseHandle(hEvent);
    return false;
}

size_t server_client::write(const scl2::bytearray &data)
{
    DWORD bytesWritten;
    if (!WriteFile(H(m_pipe), data.data(), DWORD(data.size()), &bytesWritten, nullptr)) {
        if (isBrokenError(GetLastError())) m_broken = true;
        return 0;
    }
    return bytesWritten;
}

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

bool server_client::close()
{
    if(is_open(m_pipe)) {
        DisconnectNamedPipe(H(m_pipe));
        CloseHandle(H(m_pipe));
        m_pipe = nullptr;
        return true;
    }
    return false;
}

bool server_client::cleanup()
{
    if(is_open(m_pipe)) {
        DisconnectNamedPipe(H(m_pipe));
        CloseHandle(H(m_pipe));
        m_pipe = nullptr;
        return true;
    }
    return false;
}

size_t server_client::bufferSize() const
{
    return buffer_size;
}

server::server(const std::string &name, const permissions &permissions)
    : m_name(name), m_permissions(permissions), m_pipe(nullptr), m_completion_port(nullptr), 
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
    m_stopped = false; // We do allow restarting a stopped server, so reset the flag here.
    
    // Create stop event for clean shutdown
    m_stop_event = as_winhandle(CreateEventA(nullptr, TRUE, FALSE, nullptr));
    if (!is_open(m_stop_event)) {
        return false;
    }

    // Create completion port for async I/O notification
    m_completion_port = as_winhandle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1));
    if (!is_open(m_completion_port)) {
        CloseHandle(H(m_stop_event));
        m_stop_event = nullptr;
        return false;
    }

    // Create event for connect notification
    m_connect_event = as_winhandle(CreateEventA(nullptr, TRUE, FALSE, nullptr));
    if (!is_open(m_connect_event)) {
        CloseHandle(H(m_stop_event));
        m_stop_event = nullptr;
        CloseHandle(H(m_completion_port));
        m_completion_port = nullptr;
        return false;
    }

    // Allocate OVERLAPPED structure for async connect
    m_overlapped_connect = new OVERLAPPED();
    if (!m_overlapped_connect) {
        CloseHandle(H(m_stop_event));
        m_stop_event = nullptr;
        CloseHandle(H(m_connect_event));
        m_connect_event = nullptr;
        CloseHandle(H(m_completion_port));
        m_completion_port = nullptr;
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
        max_clients + 1,
        static_cast<DWORD>(buffer_size), static_cast<DWORD>(buffer_size),
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
        CloseHandle(H(m_completion_port));
        m_completion_port = nullptr;
        CloseHandle(H(m_stop_event));
        m_stop_event = nullptr;
        return false;
    }

    // Associate pipe with completion port
    HANDLE hResult = CreateIoCompletionPort(H(m_pipe), H(m_completion_port), (ULONG_PTR)H(m_pipe), 1);
    if (!hResult) {
        CloseHandle(H(m_pipe));
        m_pipe = nullptr;
        delete ov(m_overlapped_connect);
        m_overlapped_connect = nullptr;
        CloseHandle(H(m_connect_event));
        m_connect_event = nullptr;
        CloseHandle(H(m_completion_port));
        m_completion_port = nullptr;
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
            CloseHandle(H(m_completion_port));
            m_completion_port = nullptr;
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
        return server_client(as_winhandle(INVALID_HANDLE_VALUE), buffer_size, m_mode);
    }

    // Check if the connect event is signaled (connection completed)
    DWORD dwWaitResult = WaitForSingleObject(H(m_connect_event), 0);
    
    if (dwWaitResult != WAIT_OBJECT_0) {
        // No pending connection ready
        return server_client(nullptr, buffer_size, m_mode);
    }

    // Connection is ready - prepare to return it to caller
    winhandle_t hConnectedPipe = m_pipe;

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
        max_clients + 1,
        static_cast<DWORD>(buffer_size), static_cast<DWORD>(buffer_size),
        NMPWAIT_USE_DEFAULT_WAIT,
        sd ? &sa : nullptr
    ));

    if (sd) {
        LocalFree(sd);
    }

    if (!is_open(m_pipe)) {
        return server_client(hConnectedPipe, buffer_size, m_mode);
    }

    // Associate new pipe with completion port
    HANDLE hResult = CreateIoCompletionPort(H(m_pipe), H(m_completion_port), (ULONG_PTR)H(m_pipe), 1);
    if (!hResult) {
        CloseHandle(H(m_pipe));
        m_pipe = nullptr;
        return server_client(hConnectedPipe, buffer_size, m_mode);
    }

    // Initiate async connect on new pipe instance
    BOOL connectResult = ConnectNamedPipe(H(m_pipe), ov(m_overlapped_connect));
    if (!connectResult) {
        DWORD dwError = GetLastError();
        if (dwError != ERROR_IO_PENDING && dwError != ERROR_PIPE_CONNECTED) {
            // Connection initiation failed, but return the established connection anyway
        }
    }

    return server_client(hConnectedPipe, buffer_size, m_mode);
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
    buffer_size = size;
}

size_t server::bufferSize() const
{
    return buffer_size;
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

void server::setMaxClients(int maxClients)
{
    if(is_open(m_pipe)) {
        throw std::runtime_error("Cannot change max clients while server is active");
    }

    // Clamp to [1, 254]. The extra +1 slot is reserved for the brief overlap during
    // queryNextConnection(), where both the just-accepted handle and the new listening
    // handle exist simultaneously. unlimitedClients (255) maps to the full Windows limit.
    if (maxClients <= 0 || maxClients >= unlimitedClients) {
        max_clients = unlimitedClients;
    } else {
        max_clients = maxClients;
    }
}

int server::getMaxClients() const
{
    return max_clients;
}

bool server::stop()
{
    m_stopped = true;

    // Signal stop event to unblock waiting threads
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
    // Close all client connections
    for (auto& client : m_clients) {
        client.cleanup();
    }
    m_clients.clear();

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

    // Close completion port
    if (is_open(m_completion_port)) {
        CloseHandle(H(m_completion_port));
        m_completion_port = nullptr;
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

int server::clientCount() const
{
    return static_cast<int>(m_clients.size());
}



// Client

client::client(const std::string& name)
    : m_name(name), m_pipe(nullptr)
{
}

client::~client()
{
    cleanup();
}

client::client(client&& another)
    : m_name(another.m_name), m_pipe(another.m_pipe), m_mode(another.m_mode)
{
    another.m_pipe = nullptr;
}

client& client::operator=(client&& another)
{
    if (this != &another) {
        cleanup();
        m_name = another.m_name;
        m_pipe = another.m_pipe;
        m_mode = another.m_mode;
        another.m_pipe = nullptr;
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

bool client::readyRead()
{
    return available() > 0;
}

bool client::waitForReadyRead(std::chrono::milliseconds timeout)
{
    if (!is_open(m_pipe)) {
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
    }

    // In message-based modes, wait without consuming data to preserve strict
    // one-message-per-readAll semantics.
    if (m_mode == mode::Message || m_mode == mode::MessageChunk) {
        const bool infiniteWait = (timeout.count() < 0);
        auto deadline = std::chrono::steady_clock::now() + timeout;
        constexpr auto pollInterval = std::chrono::milliseconds(5);

        while (true) {
            bytesAvailable = 0;
            if (PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
                if (bytesAvailable > 0) {
                    return true;
                }
            } else {
                DWORD err = GetLastError();
                if (isBrokenError(err)) m_broken = true;
                return false;
            }

            if (!infiniteWait) {
                auto now = std::chrono::steady_clock::now();
                if (now >= deadline) {
                    return false;
                }
                auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
                std::this_thread::sleep_for((std::min)(pollInterval, remain));
            } else {
                std::this_thread::sleep_for(pollInterval);
            }
        }
    }

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
    tempBuffer.resize(buffer_size);
    DWORD bytesRead = 0;

    // Initiate overlapped read
    BOOL readResult = ReadFile(H(m_pipe), tempBuffer.data(), static_cast<DWORD>(buffer_size), &bytesRead, &overlapped);
    
    DWORD dwTimeout = (timeout.count() < 0) ? INFINITE : static_cast<DWORD>(timeout.count());
    
    if (!readResult) {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_IO_PENDING) {
            // Operation is pending, wait for it
            DWORD dwWaitResult = WaitForSingleObject(hEvent, dwTimeout);
            
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
                    CloseHandle(hEvent);
                    return true;
                }
                if (gorErr != 0 && gorErr != ERROR_MORE_DATA) {
                    if (isBrokenError(gorErr)) m_broken = true;
                }
            }
            
            // Timeout or error - cancel the operation
            CancelIoEx(H(m_pipe), &overlapped);
            CloseHandle(hEvent);
            return false;
        } else {
            // Immediate error
            if (isBrokenError(dwError)) m_broken = true;
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
            CloseHandle(hEvent);
            return true;
        }
        if (gorErr != 0 && gorErr != ERROR_MORE_DATA) {
            if (isBrokenError(gorErr)) m_broken = true;
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
        if (isBrokenError(GetLastError())) m_broken = true;
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
        // Loop until one complete message is consumed
        while (true) {
            scl2::bytearray chunk;
            chunk.resize(buffer_size);
            DWORD bytesRead = 0;
            BOOL ok = ReadFile(H(m_pipe), chunk.data(), static_cast<DWORD>(buffer_size), &bytesRead, nullptr);
            if (bytesRead > 0) {
                chunk.resize(bytesRead);
                result.append(chunk.data(), bytesRead);
            }
            if (ok) break;
            DWORD err = GetLastError();
            if (err == ERROR_MORE_DATA) continue;
            if (isBrokenError(err)) m_broken = true;
            break;
        }
        return result;
    }

    // Byte mode: drain all currently available data
    DWORD bytesAvailable = 0;
    if (!PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
        if (isBrokenError(GetLastError())) m_broken = true;
        return result;
    }

    while (bytesAvailable > 0) {
        DWORD toRead = static_cast<DWORD>((bytesAvailable > buffer_size) ? buffer_size : bytesAvailable);
        scl2::bytearray buffer;
        buffer.resize(toRead);
        DWORD bytesRead = 0;
        
        if (!ReadFile(H(m_pipe), buffer.data(), toRead, &bytesRead, nullptr)) {
            if (isBrokenError(GetLastError())) m_broken = true;
            break;
        }

        if (bytesRead == 0) {
            break;
        }

        buffer.resize(bytesRead);
        result.append(buffer.data(), bytesRead);

        if (!PeekNamedPipe(H(m_pipe), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
            if (isBrokenError(GetLastError())) m_broken = true;
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
        if (isBrokenError(GetLastError())) m_broken = true;
        return 0;
    }

    return bytesWritten;
}

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

bool client::close()
{
    if (is_open(m_pipe)) {
        CloseHandle(H(m_pipe));
        m_pipe = nullptr;
        return true;
    }
    return false;
}

bool client::cleanup()
{
    if (is_open(m_pipe)) {
        CloseHandle(H(m_pipe));
        m_pipe = nullptr;
        return true;
    }
    return false;
}

} // namespace scl2::pipe

#endif  // SCL2_PIPE_SUPPORTED