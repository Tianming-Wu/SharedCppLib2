#include "serial.hpp"

// Pulled in first for the same reason as in process.cpp: on Windows platform.hpp sets
// NOMINMAX before it includes windows.h, on Unix it gives us the OS_* macros.
#include "platform.hpp"
#include "string.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <utility>

#ifndef OS_WINDOWS
    #include <cerrno>
    #include <dirent.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <sys/ioctl.h>
    #include <sys/stat.h>
    #include <termios.h>
    #include <unistd.h>
#endif

namespace scl2 {

// ── helpers shared by both platforms ─────────────────────────────────

namespace {

using clock_type = std::chrono::steady_clock;
using ms = std::chrono::milliseconds;

/// How often a wait re-asks the driver. A serial port has no waitable handle, so waiting
/// means polling; 2 ms is well below the latency anyone notices on a serial line.
constexpr ms poll_interval{ 2 };

/// @brief Timeouts at or beyond this count as "wait forever".
constexpr auto infinite_timeout = std::chrono::hours(24 * 365);

inline bool is_infinite(ms timeout)
{
    return timeout.count() < 0 || timeout >= infinite_timeout;
}

inline clock_type::time_point deadline_from(ms timeout)
{
    if (is_infinite(timeout)) return clock_type::time_point::max();
    return clock_type::now() + timeout;
}

/// @brief What is left of the budget: -1 for "no limit", 0 once it is over.
inline ms time_left(clock_type::time_point deadline)
{
    if (deadline == clock_type::time_point::max()) return ms(-1);

    const auto left = std::chrono::duration_cast<ms>(deadline - clock_type::now());
    return left < ms::zero() ? ms(0) : left;
}

inline bool settings_valid(const serial::settings& config)
{
    return config.baud_rate > 0 && config.data_bits >= 5 && config.data_bits <= 8;
}

/// @brief "COM2" before "COM10": compare the name, then the number at its end.
bool port_less(const std::string& a, const std::string& b)
{
    const auto split = [](const std::string& name) {
        size_t digits = name.size();
        while (digits > 0 && std::isdigit(static_cast<unsigned char>(name[digits - 1]))) {
            --digits;
        }
        if (digits == name.size() || digits == 0) {
            return std::pair<std::string, long>{ name, -1 };
        }
        return std::pair<std::string, long>{ name.substr(0, digits),
                                             std::strtol(name.c_str() + digits, nullptr, 10) };
    };

    const auto ka = split(a);
    const auto kb = split(b);
    if (ka.first != kb.first) return ka.first < kb.first;
    return ka.second < kb.second;
}

} // namespace


// ════════════════════════════════════════════════════════════════════
//  Windows: the COMM API
// ════════════════════════════════════════════════════════════════════

#ifdef OS_WINDOWS

namespace {

using native_handle = serial::native_handle_type;

inline HANDLE as_handle(native_handle h) noexcept
{
    return reinterpret_cast<HANDLE>(h);
}

inline native_handle as_native(HANDLE h) noexcept
{
    return reinterpret_cast<native_handle>(h);
}

/// @brief "\\.\COM3" - the form CreateFile needs for every port, and the only one that
///        reaches COM10 and up.
std::wstring device_path(const std::string& name)
{
    if (name.rfind("\\\\.\\", 0) == 0 || name.rfind("\\\\?\\", 0) == 0) {
        return scl2::str_to_wstr(name);
    }
    return scl2::str_to_wstr("\\\\.\\" + name);
}

/// @brief The name without the \\.\ prefix, which is what the device APIs want.
std::wstring device_name(const std::string& name)
{
    if (name.rfind("\\\\.\\", 0) == 0 || name.rfind("\\\\?\\", 0) == 0) {
        return scl2::str_to_wstr(name.substr(4));
    }
    return scl2::str_to_wstr(name);
}

std::string error_text(DWORD code)
{
    return scl2::wstr_to_str(platform::windows::TranslateErrorW(code));
}

/// @brief Whether the error means the port itself is gone - an unplugged USB adapter, a
///        handle someone closed. A timeout is not one of these.
bool device_gone(DWORD code)
{
    switch (code) {
    case ERROR_ACCESS_DENIED:
    case ERROR_INVALID_HANDLE:
    case ERROR_OPERATION_ABORTED:
    case ERROR_DEVICE_REMOVED:
    case ERROR_GEN_FAILURE:
    case ERROR_IO_DEVICE:
    case ERROR_BAD_COMMAND:
        return true;
    default:
        return false;
    }
}

/// @brief The milliseconds as a DWORD, saturating instead of wrapping.
DWORD clamp_ms(ms value)
{
    const auto capped = std::min<long long>(value.count(),
                                            static_cast<long long>(MAXDWORD) - 1);
    return capped > 0 ? static_cast<DWORD>(capped) : 0;
}

/// @brief COMMTIMEOUTS for one read: 0 takes what is already there, negative waits without
///        a limit, anything else is the total time the driver may spend on this read.
COMMTIMEOUTS read_timeouts(ms timeout)
{
    COMMTIMEOUTS ct{};
    if (timeout.count() == 0) {
        // The documented combination that returns at once with the bytes already received.
        // Plain zeroes are not it: those mean "no timeouts", i.e. block until it is all here.
        ct.ReadIntervalTimeout = MAXDWORD;
        ct.ReadTotalTimeoutMultiplier = MAXDWORD;
        ct.ReadTotalTimeoutConstant = 0;
    } else if (is_infinite(timeout)) {
        ct.ReadIntervalTimeout = 0;
        ct.ReadTotalTimeoutMultiplier = 0;
        ct.ReadTotalTimeoutConstant = 0;
    } else {
        ct.ReadIntervalTimeout = 0;
        ct.ReadTotalTimeoutMultiplier = 0;
        ct.ReadTotalTimeoutConstant = clamp_ms(timeout);
    }
    return ct;
}

/// @brief COMMTIMEOUTS for one write. The driver has no "return at once" for writes, so 0
///        and negative both mean no limit.
COMMTIMEOUTS write_timeouts(ms timeout)
{
    COMMTIMEOUTS ct{};
    ct.WriteTotalTimeoutMultiplier = 0;
    ct.WriteTotalTimeoutConstant = is_infinite(timeout) ? 0 : clamp_ms(timeout);
    return ct;
}

void fill_dcb(DCB& dcb, const serial::settings& config)
{
    dcb.BaudRate = static_cast<DWORD>(config.baud_rate);
    dcb.ByteSize = static_cast<BYTE>(config.data_bits);
    dcb.fParity = config.parity_bit != serial::parity::None ? TRUE : FALSE;

    switch (config.parity_bit) {
    case serial::parity::Odd:   dcb.Parity = ODDPARITY; break;
    case serial::parity::Even:  dcb.Parity = EVENPARITY; break;
    case serial::parity::Mark:  dcb.Parity = MARKPARITY; break;
    case serial::parity::Space: dcb.Parity = SPACEPARITY; break;
    default:                    dcb.Parity = NOPARITY; break;
    }

    switch (config.stop) {
    case serial::stop_bits::OnePointFive: dcb.StopBits = ONE5STOPBITS; break;
    case serial::stop_bits::Two:          dcb.StopBits = TWOSTOPBITS; break;
    default:                              dcb.StopBits = ONESTOPBIT; break;
    }

    dcb.fBinary = TRUE;
    dcb.fNull = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fAbortOnError = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;

    // Nothing flow controlled unless asked for; RTS is asserted by the driver the way it is
    // out of the box.
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fInX = FALSE;
    dcb.fOutX = FALSE;

    const bool hardware = config.flow == serial::flow_control::Hardware
                       || config.flow == serial::flow_control::Both;
    const bool software = config.flow == serial::flow_control::Software
                       || config.flow == serial::flow_control::Both;

    if (hardware) {
        dcb.fOutxCtsFlow = TRUE;
        dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
    }
    if (software) {
        dcb.fInX = TRUE;
        dcb.fOutX = TRUE;
        dcb.XonChar = 0x11;
        dcb.XoffChar = 0x13;
    }
}

serial::settings read_dcb(const DCB& dcb)
{
    serial::settings config;
    config.baud_rate = static_cast<int>(dcb.BaudRate);
    config.data_bits = dcb.ByteSize;

    switch (dcb.Parity) {
    case ODDPARITY:   config.parity_bit = serial::parity::Odd; break;
    case EVENPARITY:  config.parity_bit = serial::parity::Even; break;
    case MARKPARITY:  config.parity_bit = serial::parity::Mark; break;
    case SPACEPARITY: config.parity_bit = serial::parity::Space; break;
    default:          config.parity_bit = serial::parity::None; break;
    }

    switch (dcb.StopBits) {
    case ONE5STOPBITS: config.stop = serial::stop_bits::OnePointFive; break;
    case TWOSTOPBITS:  config.stop = serial::stop_bits::Two; break;
    default:           config.stop = serial::stop_bits::One; break;
    }

    const bool hardware = dcb.fOutxCtsFlow && dcb.fRtsControl == RTS_CONTROL_HANDSHAKE;
    const bool software = dcb.fInX && dcb.fOutX;
    config.flow = hardware ? (software ? serial::flow_control::Both
                                       : serial::flow_control::Hardware)
                           : (software ? serial::flow_control::Software
                                       : serial::flow_control::None);
    return config;
}

} // namespace


serial::serial() = default;

serial::~serial()
{
    close();
}

serial::serial(serial&& another) noexcept
    : m_handle(another.m_handle),
      m_name(std::move(another.m_name)),
      m_settings(another.m_settings),
      m_read_timeout(another.m_read_timeout),
      m_write_timeout(another.m_write_timeout),
      m_last_error(std::move(another.m_last_error)),
      m_last_error_code(another.m_last_error_code),
      m_broken(another.m_broken)
{
    another.m_handle = invalid_handle;
    another.m_broken = false;
    another.m_last_error_code = 0;
}

serial& serial::operator=(serial&& another) noexcept
{
    if (this == &another) return *this;

    close();

    m_handle = another.m_handle;
    m_name = std::move(another.m_name);
    m_settings = another.m_settings;
    m_read_timeout = another.m_read_timeout;
    m_write_timeout = another.m_write_timeout;
    m_last_error = std::move(another.m_last_error);
    m_last_error_code = another.m_last_error_code;
    m_broken = another.m_broken;

    another.m_handle = invalid_handle;
    another.m_broken = false;
    another.m_last_error_code = 0;
    return *this;
}

void serial::setError(std::uint32_t code, std::string text)
{
    m_last_error_code = code;
    m_last_error = std::move(text);
}

bool serial::open(const std::string& name, const settings& config)
{
    close();
    m_name = name;

    if (!settings_valid(config)) {
        setError(0, "invalid serial settings");
        return false;
    }

    // No sharing: a serial port is a device someone either has or does not have.
    HANDLE handle = ::CreateFileW(device_path(name).c_str(), GENERIC_READ | GENERIC_WRITE,
                                  0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD code = ::GetLastError();
        setError(code, error_text(code));
        return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!::GetCommState(handle, &dcb)) {
        const DWORD code = ::GetLastError();
        ::CloseHandle(handle);
        setError(code, error_text(code));
        return false;
    }

    fill_dcb(dcb, config);
    if (!::SetCommState(handle, &dcb)) {
        const DWORD code = ::GetLastError();
        ::CloseHandle(handle);
        setError(code, error_text(code));
        return false;
    }

    COMMTIMEOUTS timeouts = read_timeouts(m_read_timeout);
    timeouts.WriteTotalTimeoutConstant = clamp_ms(m_write_timeout);
    ::SetCommTimeouts(handle, &timeouts);

    m_handle = as_native(handle);
    m_settings = config;
    m_broken = false;
    m_last_error.clear();
    m_last_error_code = 0;
    return true;
}

bool serial::close()
{
    if (m_handle != invalid_handle) {
        ::CloseHandle(as_handle(m_handle));
        m_handle = invalid_handle;
    }
    m_broken = false;
    return true;
}

bool serial::isOpen() const
{
    return m_handle != invalid_handle;
}

bool serial::valid()
{
    return isOpen();
}

bool serial::reset()
{
    return close();
}

bool serial::applySettings(const settings& config)
{
    if (!isOpen()) {
        setError(0, "the port is not open");
        return false;
    }
    if (!settings_valid(config)) {
        setError(0, "invalid serial settings");
        return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!::GetCommState(as_handle(m_handle), &dcb)) {
        const DWORD code = ::GetLastError();
        setError(code, error_text(code));
        return false;
    }

    fill_dcb(dcb, config);
    if (!::SetCommState(as_handle(m_handle), &dcb)) {
        const DWORD code = ::GetLastError();
        setError(code, error_text(code));
        return false;
    }

    m_settings = config;
    return true;
}

serial::settings serial::currentSettings() const
{
    if (!isOpen()) return m_settings;

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!::GetCommState(as_handle(m_handle), &dcb)) return m_settings;
    return read_dcb(dcb);
}

std::string serial::portName() const
{
    return m_name;
}

void serial::setReadTimeout(std::chrono::milliseconds timeout)
{
    m_read_timeout = timeout;
}

std::chrono::milliseconds serial::readTimeout() const
{
    return m_read_timeout;
}

void serial::setWriteTimeout(std::chrono::milliseconds timeout)
{
    m_write_timeout = timeout;
}

std::chrono::milliseconds serial::writeTimeout() const
{
    return m_write_timeout;
}

bool serial::readable()
{
    if (!isOpen()) return false;
    if (m_broken) return true;
    // available() is what notices a device that has gone away.
    return available() > 0 || m_broken;
}

size_t serial::available()
{
    if (!isOpen()) return 0;

    COMSTAT status{};
    DWORD errors = 0;
    if (!::ClearCommError(as_handle(m_handle), &errors, &status)) {
        const DWORD code = ::GetLastError();
        setError(code, error_text(code));
        if (device_gone(code)) m_broken = true;
        return 0;
    }
    return status.cbInQue;
}

scl2::bytearray serial::read(size_t bytes)
{
    scl2::bytearray result;
    if (!isOpen() || bytes == 0) return result;

    scl2::bytearray buffer;
    buffer.resize(bytes);

    const auto deadline = deadline_from(m_read_timeout);
    size_t got = 0;
    while (got < bytes) {
        const ms left = time_left(deadline);
        // Nothing queued and nothing left of the budget: asking the driver for a read that
        // cannot take anything still costs a round trip - about 15 ms on a CH340, which is
        // what makes a zero budget read feel slow. Do not ask.
        if (left.count() == 0 && available() == 0) break;

        COMMTIMEOUTS timeouts = read_timeouts(left);
        if (!::SetCommTimeouts(as_handle(m_handle), &timeouts)) {
            const DWORD code = ::GetLastError();
            setError(code, error_text(code));
            break;
        }

        const DWORD want = static_cast<DWORD>(
            std::min<size_t>(bytes - got, static_cast<size_t>(MAXDWORD)));
        DWORD received = 0;
        if (!::ReadFile(as_handle(m_handle), buffer.data() + got, want, &received, nullptr)) {
            const DWORD code = ::GetLastError();
            setError(code, error_text(code));
            if (device_gone(code)) m_broken = true;
            break;
        }

        got += received;
        // Nothing came in during the budget, or the whole request is in: either way, done.
        if (received == 0 || got == bytes) break;
    }

    if (got > 0) {
        buffer.resize(got);
        result = std::move(buffer);
    }
    return result;
}

scl2::bytearray serial::readAll()
{
    const size_t queued = available();
    if (queued == 0) return scl2::bytearray();
    return read(queued);
}

size_t serial::write(const scl2::bytearray& data)
{
    if (!isOpen() || data.empty()) return 0;

    const auto deadline = deadline_from(m_write_timeout);
    size_t sent = 0;
    bool first = true;
    while (sent < data.size()) {
        const ms left = time_left(deadline);
        // Once the budget is gone, stop: a zero here would mean "no timeouts" to the driver,
        // which blocks until every byte is out.
        if (!first && left.count() == 0) break;
        first = false;

        COMMTIMEOUTS timeouts = write_timeouts(left);
        if (!::SetCommTimeouts(as_handle(m_handle), &timeouts)) {
            const DWORD code = ::GetLastError();
            setError(code, error_text(code));
            break;
        }

        const DWORD chunk = static_cast<DWORD>(
            std::min<size_t>(data.size() - sent, static_cast<size_t>(MAXDWORD)));
        DWORD written = 0;
        if (!::WriteFile(as_handle(m_handle), data.data() + sent, chunk, &written, nullptr)) {
            const DWORD code = ::GetLastError();
            setError(code, error_text(code));
            if (device_gone(code)) m_broken = true;
            break;
        }

        sent += written;
        if (written == 0) break;
    }
    return sent;
}

bool serial::waitForReadyRead(std::chrono::milliseconds timeout)
{
    if (!isOpen()) return false;
    if (m_broken) return true;
    if (available() > 0) return true;

    const auto deadline = deadline_from(timeout);
    for (;;) {
        const ms left = time_left(deadline);
        if (left.count() == 0) return available() > 0;

        const ms slice = left.count() < 0 ? poll_interval : std::min(left, poll_interval);
        std::this_thread::sleep_for(slice);

        if (m_broken) return true;
        if (available() > 0) return true;
    }
}

bool serial::purge(purge_target what)
{
    if (!isOpen()) {
        setError(0, "the port is not open");
        return false;
    }

    DWORD flags = 0;
    switch (what) {
    case purge_target::Receive:  flags = PURGE_RXCLEAR; break;
    case purge_target::Transmit: flags = PURGE_TXCLEAR; break;
    case purge_target::Both:     flags = PURGE_RXCLEAR | PURGE_TXCLEAR; break;
    }

    if (!::PurgeComm(as_handle(m_handle), flags)) {
        const DWORD code = ::GetLastError();
        setError(code, error_text(code));
        return false;
    }
    return true;
}

size_t serial::bytesToWrite() const
{
    if (!isOpen()) return 0;

    COMSTAT status{};
    DWORD errors = 0;
    if (!::ClearCommError(as_handle(m_handle), &errors, &status)) return 0;
    return status.cbOutQue;
}

serial::modem_lines serial::modem()
{
    modem_lines lines;
    if (!isOpen()) return lines;

    DWORD status = 0;
    if (!::GetCommModemStatus(as_handle(m_handle), &status)) {
        const DWORD code = ::GetLastError();
        setError(code, error_text(code));
        return lines;
    }

    lines.cts = (status & MS_CTS_ON) != 0;
    lines.dsr = (status & MS_DSR_ON) != 0;
    lines.ri = (status & MS_RING_ON) != 0;
    lines.dcd = (status & MS_RLSD_ON) != 0;
    return lines;
}

bool serial::setDtr(bool asserted)
{
    if (!isOpen()) {
        setError(0, "the port is not open");
        return false;
    }
    if (!::EscapeCommFunction(as_handle(m_handle), asserted ? SETDTR : CLRDTR)) {
        const DWORD code = ::GetLastError();
        setError(code, error_text(code));
        return false;
    }
    return true;
}

bool serial::setRts(bool asserted)
{
    if (!isOpen()) {
        setError(0, "the port is not open");
        return false;
    }
    if (!::EscapeCommFunction(as_handle(m_handle), asserted ? SETRTS : CLRRTS)) {
        const DWORD code = ::GetLastError();
        setError(code, error_text(code));
        return false;
    }
    return true;
}

bool serial::sendBreak(std::chrono::milliseconds duration)
{
    if (!isOpen()) {
        setError(0, "the port is not open");
        return false;
    }

    if (!::SetCommBreak(as_handle(m_handle))) {
        const DWORD code = ::GetLastError();
        setError(code, error_text(code));
        return false;
    }

    std::this_thread::sleep_for(duration.count() > 0 ? duration
                                                      : std::chrono::milliseconds(250));

    if (!::ClearCommBreak(as_handle(m_handle))) {
        const DWORD code = ::GetLastError();
        setError(code, error_text(code));
        return false;
    }
    return true;
}

bool serial::waitForBytesWritten(std::chrono::milliseconds timeout)
{
    if (!isOpen()) return false;

    const auto deadline = deadline_from(timeout);
    for (;;) {
        if (bytesToWrite() == 0) return true;
        if (m_broken) return false;

        const ms left = time_left(deadline);
        if (left.count() == 0) return false;

        std::this_thread::sleep_for(left.count() < 0 ? poll_interval
                                                      : std::min(left, poll_interval));
    }
}

bool serial::broken() const
{
    return m_broken;
}

std::string serial::lastError() const
{
    return m_last_error;
}

std::uint32_t serial::lastErrorCode() const
{
    return m_last_error_code;
}

serial::native_handle_type serial::nativeHandle() const
{
    return m_handle;
}

std::vector<std::string> serial::availablePorts()
{
    std::vector<std::string> ports;

    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0,
                        KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) {
        return ports;
    }

    for (DWORD index = 0;; ++index) {
        wchar_t value_name[256]{};
        DWORD value_name_length = 256;
        BYTE data[1024]{};
        DWORD data_length = sizeof(data);
        DWORD type = 0;

        const LONG rc = ::RegEnumValueW(key, index, value_name, &value_name_length, nullptr,
                                        &type, data, &data_length);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc != ERROR_SUCCESS) break;
        if (type != REG_SZ) continue;

        std::wstring name(reinterpret_cast<const wchar_t*>(data),
                          data_length / sizeof(wchar_t));
        while (!name.empty() && name.back() == L'\0') name.pop_back();
        if (!name.empty()) ports.push_back(scl2::wstr_to_str(name));
    }

    ::RegCloseKey(key);
    std::sort(ports.begin(), ports.end(), port_less);
    return ports;
}

bool serial::portExists(const std::string& name)
{
    wchar_t target[512]{};
    return ::QueryDosDeviceW(device_name(name).c_str(), target, 512) != 0;
}


// ════════════════════════════════════════════════════════════════════
//  POSIX: termios
// ════════════════════════════════════════════════════════════════════

#else

namespace {

using native_handle = serial::native_handle_type;

inline int as_fd(native_handle h) noexcept
{
    return static_cast<int>(h);
}

inline native_handle as_native(int fd) noexcept
{
    return static_cast<native_handle>(fd);
}

/// @brief The errors that mean the device itself is gone, not that we were too slow.
bool device_gone(int code)
{
    return code == EIO || code == ENXIO || code == EBADF || code == ENODEV || code == EPIPE;
}

/// @brief Whether read()/write() were told to try again rather than that something failed.
inline bool again(int code)
{
    return code == EINTR || code == EAGAIN
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
        || code == EWOULDBLOCK
#endif
        ;
}

struct baud_pair {
    int rate;
    speed_t speed;
};

const baud_pair baud_table[] = {
    { 50, B50 },       { 75, B75 },       { 110, B110 },     { 134, B134 },
    { 150, B150 },     { 200, B200 },     { 300, B300 },     { 600, B600 },
    { 1200, B1200 },   { 1800, B1800 },   { 2400, B2400 },   { 4800, B4800 },
    { 9600, B9600 },   { 19200, B19200 }, { 38400, B38400 },
#ifdef B57600
    { 57600, B57600 },
#endif
#ifdef B115200
    { 115200, B115200 },
#endif
#ifdef B230400
    { 230400, B230400 },
#endif
#ifdef B460800
    { 460800, B460800 },
#endif
#ifdef B500000
    { 500000, B500000 },
#endif
#ifdef B576000
    { 576000, B576000 },
#endif
#ifdef B921600
    { 921600, B921600 },
#endif
#ifdef B1000000
    { 1000000, B1000000 },
#endif
#ifdef B1152000
    { 1152000, B1152000 },
#endif
#ifdef B1500000
    { 1500000, B1500000 },
#endif
#ifdef B2000000
    { 2000000, B2000000 },
#endif
#ifdef B3000000
    { 3000000, B3000000 },
#endif
};

speed_t speed_of(int rate)
{
    for (const baud_pair& entry : baud_table) {
        if (entry.rate == rate) return entry.speed;
    }
    return static_cast<speed_t>(0);
}

int rate_of(speed_t speed)
{
    for (const baud_pair& entry : baud_table) {
        if (entry.speed == speed) return entry.rate;
    }
    return 0;
}

/// @brief Put @p config onto @p fd. The error text is filled in when a setting cannot be
///        expressed here (mark parity, 1.5 stop bits, hardware flow control); @p code is the
///        errno behind a failed system call, or 0 when the setting is simply unsupported.
bool apply_termios(int fd, const serial::settings& config, std::string& error,
                   std::uint32_t& code)
{
    code = 0;

    termios tio{};
    if (::tcgetattr(fd, &tio) != 0) {
        code = static_cast<std::uint32_t>(errno);
        error = std::strerror(errno);
        return false;
    }

    // Raw first: no echo, no line editing, no signal characters. This is cfmakeraw() written
    // out, because cfmakeraw() is a BSD extension that a strict ANSI build does not see.
    tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL
                     | IXON | IXOFF | IXANY);
    tio.c_oflag &= ~OPOST;
    tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tio.c_cflag &= ~(CSIZE | PARENB);

    switch (config.data_bits) {
    case 5: tio.c_cflag |= CS5; break;
    case 6: tio.c_cflag |= CS6; break;
    case 7: tio.c_cflag |= CS7; break;
    default: tio.c_cflag |= CS8; break;
    }

    switch (config.parity_bit) {
    case serial::parity::Odd:
        tio.c_cflag |= PARENB | PARODD;
        break;
    case serial::parity::Even:
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
        break;
    case serial::parity::Mark:
#ifdef CMSPAR
        tio.c_cflag |= PARENB | PARODD | CMSPAR;
        break;
#else
        error = "mark parity is not available on this platform";
        return false;
#endif
    case serial::parity::Space:
#ifdef CMSPAR
        tio.c_cflag |= PARENB | CMSPAR;
        tio.c_cflag &= ~PARODD;
        break;
#else
        error = "space parity is not available on this platform";
        return false;
#endif
    default:
        tio.c_cflag &= ~(PARENB | PARODD);
        break;
    }

    switch (config.stop) {
    case serial::stop_bits::Two:
        tio.c_cflag |= CSTOPB;
        break;
    case serial::stop_bits::OnePointFive:
        error = "one and a half stop bits are not available on this platform";
        return false;
    default:
        tio.c_cflag &= ~CSTOPB;
        break;
    }

    const bool hardware = config.flow == serial::flow_control::Hardware
                       || config.flow == serial::flow_control::Both;
    const bool software = config.flow == serial::flow_control::Software
                       || config.flow == serial::flow_control::Both;

#if defined(CRTSCTS)
    tio.c_cflag &= ~CRTSCTS;
#elif defined(CCTS_OFLOW) && defined(CRTS_IFLOW)
    tio.c_cflag &= ~(CCTS_OFLOW | CRTS_IFLOW);
#endif

    if (hardware) {
#if defined(CRTSCTS)
        tio.c_cflag |= CRTSCTS;
#elif defined(CCTS_OFLOW) && defined(CRTS_IFLOW)
        tio.c_cflag |= CCTS_OFLOW | CRTS_IFLOW;
#else
        error = "hardware flow control is not available on this platform";
        return false;
#endif
    }

    if (software) {
        tio.c_iflag |= IXON | IXOFF;
        tio.c_cc[VSTART] = 0x11;
        tio.c_cc[VSTOP] = 0x13;
    }

    // Ignore modem control lines and switch the receiver on: without CLOCAL an open() would
    // block until a carrier shows up, which is not what a plain serial link wants.
    tio.c_cflag |= CLOCAL | CREAD;

    // VTIME and VMIN are unused (the descriptor is non-blocking and waits go through poll),
    // but leaving them at whatever the last user of the port set would be misleading.
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    const speed_t speed = speed_of(config.baud_rate);
    if (speed == 0) {
        error = "unsupported baud rate: " + std::to_string(config.baud_rate);
        return false;
    }
    if (::cfsetispeed(&tio, speed) != 0 || ::cfsetospeed(&tio, speed) != 0) {
        code = static_cast<std::uint32_t>(errno);
        error = std::strerror(errno);
        return false;
    }

    if (::tcsetattr(fd, TCSANOW, &tio) != 0) {
        code = static_cast<std::uint32_t>(errno);
        error = std::strerror(errno);
        return false;
    }
    return true;
}

serial::settings read_termios(int fd, const serial::settings& fallback)
{
    termios tio{};
    if (::tcgetattr(fd, &tio) != 0) return fallback;

    serial::settings config = fallback;

    const int rate = rate_of(::cfgetospeed(&tio));
    if (rate != 0) config.baud_rate = rate;

    switch (tio.c_cflag & CSIZE) {
    case CS5: config.data_bits = 5; break;
    case CS6: config.data_bits = 6; break;
    case CS7: config.data_bits = 7; break;
    case CS8: config.data_bits = 8; break;
    default: break;
    }

    if ((tio.c_cflag & PARENB) == 0) {
        config.parity_bit = serial::parity::None;
    } else {
#ifdef CMSPAR
        if ((tio.c_cflag & CMSPAR) != 0) {
            config.parity_bit = (tio.c_cflag & PARODD) != 0 ? serial::parity::Mark
                                                            : serial::parity::Space;
        } else
#endif
        {
            config.parity_bit = (tio.c_cflag & PARODD) != 0 ? serial::parity::Odd
                                                            : serial::parity::Even;
        }
    }

    config.stop = (tio.c_cflag & CSTOPB) != 0 ? serial::stop_bits::Two
                                              : serial::stop_bits::One;

    bool hardware = false;
#if defined(CRTSCTS)
    hardware = (tio.c_cflag & CRTSCTS) != 0;
#elif defined(CCTS_OFLOW) && defined(CRTS_IFLOW)
    hardware = (tio.c_cflag & (CCTS_OFLOW | CRTS_IFLOW)) != 0;
#endif
    const bool software = (tio.c_iflag & (IXON | IXOFF)) != 0;

    config.flow = hardware ? (software ? serial::flow_control::Both
                                       : serial::flow_control::Hardware)
                           : (software ? serial::flow_control::Software
                                       : serial::flow_control::None);
    return config;
}

} // namespace


serial::serial() = default;

serial::~serial()
{
    close();
}

serial::serial(serial&& another) noexcept
    : m_handle(another.m_handle),
      m_name(std::move(another.m_name)),
      m_settings(another.m_settings),
      m_read_timeout(another.m_read_timeout),
      m_write_timeout(another.m_write_timeout),
      m_last_error(std::move(another.m_last_error)),
      m_last_error_code(another.m_last_error_code),
      m_broken(another.m_broken)
{
    another.m_handle = invalid_handle;
    another.m_broken = false;
    another.m_last_error_code = 0;
}

serial& serial::operator=(serial&& another) noexcept
{
    if (this == &another) return *this;

    close();

    m_handle = another.m_handle;
    m_name = std::move(another.m_name);
    m_settings = another.m_settings;
    m_read_timeout = another.m_read_timeout;
    m_write_timeout = another.m_write_timeout;
    m_last_error = std::move(another.m_last_error);
    m_last_error_code = another.m_last_error_code;
    m_broken = another.m_broken;

    another.m_handle = invalid_handle;
    another.m_broken = false;
    another.m_last_error_code = 0;
    return *this;
}

void serial::setError(std::uint32_t code, std::string text)
{
    m_last_error_code = code;
    m_last_error = std::move(text);
}

bool serial::open(const std::string& name, const settings& config)
{
    close();
    m_name = name;

    if (!settings_valid(config)) {
        setError(0, "invalid serial settings");
        return false;
    }

    // O_NONBLOCK so that waiting is ours to time: the port is opened without waiting for a
    // carrier, and every read/write goes through poll().
    int flags = O_RDWR | O_NOCTTY | O_NONBLOCK;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif

    const int fd = ::open(name.c_str(), flags);
    if (fd < 0) {
        const int code = errno;
        setError(static_cast<std::uint32_t>(code), std::strerror(code));
        return false;
    }

    if (::isatty(fd) == 0) {
        setError(ENOTTY, "'" + name + "' is not a terminal device");
        ::close(fd);
        return false;
    }

    std::string error;
    std::uint32_t code = 0;
    if (!apply_termios(fd, config, error, code)) {
        setError(code, error);
        ::close(fd);
        return false;
    }

    m_handle = as_native(fd);
    m_settings = config;
    m_broken = false;
    m_last_error.clear();
    m_last_error_code = 0;
    return true;
}

bool serial::close()
{
    if (m_handle != invalid_handle) {
        ::close(as_fd(m_handle));
        m_handle = invalid_handle;
    }
    m_broken = false;
    return true;
}

bool serial::isOpen() const
{
    return m_handle != invalid_handle;
}

bool serial::valid()
{
    return isOpen();
}

bool serial::reset()
{
    return close();
}

bool serial::applySettings(const settings& config)
{
    if (!isOpen()) {
        setError(0, "the port is not open");
        return false;
    }
    if (!settings_valid(config)) {
        setError(0, "invalid serial settings");
        return false;
    }

    std::string error;
    std::uint32_t code = 0;
    if (!apply_termios(as_fd(m_handle), config, error, code)) {
        setError(code, error);
        return false;
    }

    m_settings = config;
    return true;
}

serial::settings serial::currentSettings() const
{
    if (!isOpen()) return m_settings;
    return read_termios(as_fd(m_handle), m_settings);
}

std::string serial::portName() const
{
    return m_name;
}

void serial::setReadTimeout(std::chrono::milliseconds timeout)
{
    m_read_timeout = timeout;
}

std::chrono::milliseconds serial::readTimeout() const
{
    return m_read_timeout;
}

void serial::setWriteTimeout(std::chrono::milliseconds timeout)
{
    m_write_timeout = timeout;
}

std::chrono::milliseconds serial::writeTimeout() const
{
    return m_write_timeout;
}

bool serial::readable()
{
    if (!isOpen()) return false;
    if (m_broken) return true;
    // available() is what notices a device that has gone away.
    return available() > 0 || m_broken;
}

size_t serial::available()
{
    if (!isOpen()) return 0;

    int pending = 0;
    if (::ioctl(as_fd(m_handle), FIONREAD, &pending) < 0) {
        const int code = errno;
        setError(static_cast<std::uint32_t>(code), std::strerror(code));
        if (device_gone(code)) m_broken = true;
        return 0;
    }
    return pending > 0 ? static_cast<size_t>(pending) : 0;
}

scl2::bytearray serial::read(size_t bytes)
{
    scl2::bytearray result;
    if (!isOpen() || bytes == 0) return result;

    scl2::bytearray buffer;
    buffer.resize(bytes);

    const auto deadline = deadline_from(m_read_timeout);
    size_t got = 0;
    while (got < bytes) {
        const ms left = time_left(deadline);

        pollfd probe{};
        probe.fd = as_fd(m_handle);
        probe.events = POLLIN;

        const int ready = ::poll(&probe, 1, static_cast<int>(left.count()));
        if (ready < 0) {
            if (errno == EINTR) continue;
            const int code = errno;
            setError(static_cast<std::uint32_t>(code), std::strerror(code));
            if (device_gone(code)) m_broken = true;
            break;
        }
        if (ready == 0) break;   // nothing arrived within the budget

        // Data comes first: a hangup can arrive with bytes still buffered, and those belong
        // to the caller before the port is declared gone.
        if ((probe.revents & POLLIN) == 0) {
            if ((probe.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) m_broken = true;
            break;
        }

        const ssize_t received = ::read(as_fd(m_handle), buffer.data() + got, bytes - got);
        if (received < 0) {
            const int code = errno;
            if (again(code)) {
                if (left.count() == 0) break;
                continue;
            }
            setError(static_cast<std::uint32_t>(code), std::strerror(code));
            if (device_gone(code)) m_broken = true;
            break;
        }
        if (received == 0) break;

        got += static_cast<size_t>(received);
        if (left.count() == 0) break;   // the budget was 0: one look, then done
    }

    if (got > 0) {
        buffer.resize(got);
        result = std::move(buffer);
    }
    return result;
}

scl2::bytearray serial::readAll()
{
    const size_t queued = available();
    if (queued == 0) return scl2::bytearray();
    return read(queued);
}

size_t serial::write(const scl2::bytearray& data)
{
    if (!isOpen() || data.empty()) return 0;

    const auto deadline = deadline_from(m_write_timeout);
    size_t sent = 0;
    while (sent < data.size()) {
        const ms left = time_left(deadline);

        pollfd probe{};
        probe.fd = as_fd(m_handle);
        probe.events = POLLOUT;

        const int ready = ::poll(&probe, 1, static_cast<int>(left.count()));
        if (ready < 0) {
            if (errno == EINTR) continue;
            const int code = errno;
            setError(static_cast<std::uint32_t>(code), std::strerror(code));
            if (device_gone(code)) m_broken = true;
            break;
        }
        if (ready == 0) break;

        if ((probe.revents & POLLOUT) == 0) {
            if ((probe.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) m_broken = true;
            break;
        }

        const ssize_t written = ::write(as_fd(m_handle), data.data() + sent, data.size() - sent);
        if (written < 0) {
            const int code = errno;
            if (again(code)) {
                if (left.count() == 0) break;
                continue;
            }
            setError(static_cast<std::uint32_t>(code), std::strerror(code));
            if (device_gone(code)) m_broken = true;
            break;
        }

        sent += static_cast<size_t>(written);
        if (left.count() == 0) break;
    }
    return sent;
}

bool serial::waitForReadyRead(std::chrono::milliseconds timeout)
{
    if (!isOpen()) return false;
    if (m_broken) return true;
    if (available() > 0) return true;

    const auto deadline = deadline_from(timeout);
    for (;;) {
        const ms left = time_left(deadline);

        pollfd probe{};
        probe.fd = as_fd(m_handle);
        probe.events = POLLIN;

        const int ready = ::poll(&probe, 1, static_cast<int>(left.count()));
        if (ready < 0) {
            if (errno == EINTR) continue;
            const int code = errno;
            setError(static_cast<std::uint32_t>(code), std::strerror(code));
            return false;
        }
        if (ready == 0) return available() > 0;

        if ((probe.revents & POLLIN) != 0) return true;
        if ((probe.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            m_broken = true;
            return true;
        }

        if (left.count() == 0) return available() > 0;
    }
}

bool serial::purge(purge_target what)
{
    if (!isOpen()) {
        setError(0, "the port is not open");
        return false;
    }

    int queue = 0;
    switch (what) {
    case purge_target::Receive:  queue = TCIFLUSH; break;
    case purge_target::Transmit: queue = TCOFLUSH; break;
    case purge_target::Both:     queue = TCIOFLUSH; break;
    }

    if (::tcflush(as_fd(m_handle), queue) != 0) {
        const int code = errno;
        setError(static_cast<std::uint32_t>(code), std::strerror(code));
        return false;
    }
    return true;
}

size_t serial::bytesToWrite() const
{
    if (!isOpen()) return 0;

#ifdef TIOCOUTQ
    int queued = 0;
    if (::ioctl(as_fd(m_handle), TIOCOUTQ, &queued) < 0) return 0;
    return queued > 0 ? static_cast<size_t>(queued) : 0;
#else
    return 0;
#endif
}

serial::modem_lines serial::modem()
{
    modem_lines lines;
    if (!isOpen()) return lines;

    int status = 0;
    if (::ioctl(as_fd(m_handle), TIOCMGET, &status) < 0) {
        const int code = errno;
        setError(static_cast<std::uint32_t>(code), std::strerror(code));
        return lines;
    }

    lines.cts = (status & TIOCM_CTS) != 0;
    lines.dsr = (status & TIOCM_DSR) != 0;
    lines.ri = (status & TIOCM_RNG) != 0;
    lines.dcd = (status & TIOCM_CAR) != 0;
    return lines;
}

bool serial::setDtr(bool asserted)
{
    if (!isOpen()) {
        setError(0, "the port is not open");
        return false;
    }

    int bits = TIOCM_DTR;
    if (::ioctl(as_fd(m_handle), asserted ? TIOCMBIS : TIOCMBIC, &bits) < 0) {
        const int code = errno;
        setError(static_cast<std::uint32_t>(code), std::strerror(code));
        return false;
    }
    return true;
}

bool serial::setRts(bool asserted)
{
    if (!isOpen()) {
        setError(0, "the port is not open");
        return false;
    }

    int bits = TIOCM_RTS;
    if (::ioctl(as_fd(m_handle), asserted ? TIOCMBIS : TIOCMBIC, &bits) < 0) {
        const int code = errno;
        setError(static_cast<std::uint32_t>(code), std::strerror(code));
        return false;
    }
    return true;
}

bool serial::sendBreak(std::chrono::milliseconds duration)
{
    (void)duration;   // tcsendbreak sends the system's own break and takes tenths of a second

    if (!isOpen()) {
        setError(0, "the port is not open");
        return false;
    }
    if (::tcsendbreak(as_fd(m_handle), 0) != 0) {
        const int code = errno;
        setError(static_cast<std::uint32_t>(code), std::strerror(code));
        return false;
    }
    return true;
}

bool serial::waitForBytesWritten(std::chrono::milliseconds timeout)
{
    if (!isOpen()) return false;

#ifdef TIOCOUTQ
    const auto deadline = deadline_from(timeout);
    for (;;) {
        if (bytesToWrite() == 0) return true;
        if (m_broken) return false;

        const ms left = time_left(deadline);
        if (left.count() == 0) return false;

        std::this_thread::sleep_for(left.count() < 0 ? poll_interval
                                                      : std::min(left, poll_interval));
    }
#else
    (void)timeout;   // without TIOCOUTQ the only way to wait is tcdrain, which cannot time out
    if (::tcdrain(as_fd(m_handle)) != 0) {
        const int code = errno;
        setError(static_cast<std::uint32_t>(code), std::strerror(code));
        return false;
    }
    return true;
#endif
}

bool serial::broken() const
{
    return m_broken;
}

std::string serial::lastError() const
{
    return m_last_error;
}

std::uint32_t serial::lastErrorCode() const
{
    return m_last_error_code;
}

serial::native_handle_type serial::nativeHandle() const
{
    return m_handle;
}

std::vector<std::string> serial::availablePorts()
{
    std::vector<std::string> ports;

    static const char* const prefixes[] = {
        "ttyS", "ttyUSB", "ttyACM", "ttyAMA", "ttyXRUSB", "ttyCH343USB",
        "rfcomm", "cu.", "tty.usb", "ttymodem",
    };

    DIR* directory = ::opendir("/dev");
    if (directory == nullptr) return ports;

    while (const dirent* entry = ::readdir(directory)) {
        const std::string name = entry->d_name;
        for (const char* prefix : prefixes) {
            if (name.rfind(prefix, 0) == 0) {
                ports.push_back("/dev/" + name);
                break;
            }
        }
    }
    ::closedir(directory);

    std::sort(ports.begin(), ports.end(), port_less);
    return ports;
}

bool serial::portExists(const std::string& name)
{
    struct stat info{};
    if (::stat(name.c_str(), &info) != 0) return false;
    return S_ISCHR(info.st_mode) != 0;
}

#endif  // OS_WINDOWS

} // namespace scl2
