/*
    Serial ports: one port, one byte stream.

    classes:
        scl2::serial, with the nested types serial::settings, parity, stop_bits,
        flow_control, purge_target and modem_lines
    link target:
        SharedCppLib2::serial

    A serial port is a stream like the others in this library: read() takes bytes, write()
    sends them, available() says how many are waiting. There are no message boundaries and no
    peer to connect to - the port is the connection - so one object owns one open port.

    The same header serves Windows (COMM API) and POSIX (termios), and names no platform type:
    the native handle travels as an opaque std::intptr_t, as it does in process.hpp.

    A port is named "COM3" on Windows - the \\.\ prefix that COM10 and up require is added for
    you - and "/dev/ttyUSB0" on POSIX. availablePorts() answers what the system reports.

    Every failure is recorded rather than thrown: the call answers false (or short reads and
    zero written bytes) and lastError() says what happened.
*/

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "basics.hpp"
#include "bytearray.hpp"
#include "stream.hpp"

namespace scl2 {

class serial : public scl2::basic_iostream {
public:
    /// @brief The parity bit a frame carries, if any.
    enum class parity {
        None,
        Odd,
        Even,
        Mark,   ///< always 1; Windows and Linux can ask for it, other POSIX systems cannot
        Space,  ///< always 0; same restriction as Mark
    };

    /// @brief How many stop bits close a frame.
    enum class stop_bits {
        One,
        OnePointFive,   ///< Windows only; the POSIX termios API has no way to express it
        Two,
    };

    /// @brief Which directions are flow controlled.
    enum class flow_control {
        None,
        Hardware,   ///< RTS / CTS
        Software,   ///< XON / XOFF
        Both,
    };

    /// @brief The line settings of a port. The defaults are 115200 8N1, no flow control.
    struct settings {
        int baud_rate = 115200;
        int data_bits = 8;
        parity parity_bit = parity::None;
        stop_bits stop = stop_bits::One;
        flow_control flow = flow_control::None;

        bool operator==(const settings&) const = default;
    };

    /// @brief Which driver queue purge() throws away.
    enum class purge_target {
        Receive,
        Transmit,
        Both,
    };

    /// @brief The input lines a modem raises, as read from the port.
    struct modem_lines {
        bool cts = false;   ///< clear to send
        bool dsr = false;   ///< data set ready
        bool ri = false;    ///< ring indicator
        bool dcd = false;   ///< data carrier detect
    };

    /// @brief Opaque native handle: a HANDLE on Windows, a file descriptor on POSIX.
    /// @note invalid_handle is INVALID_HANDLE_VALUE on Windows and -1 elsewhere.
    using native_handle_type = std::intptr_t;
    static constexpr native_handle_type invalid_handle = -1;

    /// @brief A port that holds nothing. valid() returns false.
    serial();
    ~serial() override;

    disable_copy(serial)

    /// @note Hand written rather than defaulted: a moved-from port has to give up its handle,
    ///       or both objects would end up closing it.
    serial(serial&& another) noexcept;
    serial& operator=(serial&& another) noexcept;

    // ── open / close ─────────────────────────────────────────────────

    /// @brief Open @p name with @p config, replacing whatever this object held.
    /// @note Windows opens with no sharing, so a port another program holds cannot be
    ///       opened. The name is kept even when opening fails, so portName() reports what
    ///       was tried.
    bool open(const std::string& name, const settings& config = settings());

    /// @brief Release the port. Idempotent, always answers true.
    bool close();

    bool isOpen() const;

    /// @brief The stream name for close(). Idempotent, answers true even when closed.
    bool reset() override;

    // ── settings ─────────────────────────────────────────────────────

    /// @brief Apply @p config to the open port, keeping the driver's other settings.
    bool applySettings(const settings& config);

    /// @brief The settings the driver reports. When the port is closed, or the driver will
    ///        not say, this is what was last applied successfully.
    settings currentSettings() const;

    /// @brief The name this port was opened with, in the form it was given.
    std::string portName() const;

    /// @brief How long read() waits for the bytes it was asked for.
    /// @note 0 means "take whatever is already there", a negative value means "wait without a
    ///       limit", and the default is one second.
    void setReadTimeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds readTimeout() const;

    /// @brief How long write() waits for the driver to take the bytes.
    /// @note 0 and negative both mean "no limit" on Windows, where the driver has no way to
    ///       make a write return at once. POSIX honours both.
    void setWriteTimeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds writeTimeout() const;

    // ── scl2::basic_iostream ─────────────────────────────────────────

    bool valid() override;

    /// @brief true while bytes are waiting, and also once the port has gone away, so that a
    ///        caller blocked on it learns about that instead of waiting for ever.
    bool readable() override;

    size_t available() override;

    /// @brief Read up to @p bytes, waiting no longer than the read timeout.
    /// @return the bytes that arrived; fewer than asked for means the timeout ran out.
    scl2::bytearray read(size_t bytes) override;

    /// @brief Take everything the port has queued right now, without waiting.
    scl2::bytearray readAll() override;

    /// @brief Write @p data, waiting no longer than the write timeout.
    /// @return the bytes the driver took, so a partial write says where to continue.
    size_t write(const scl2::bytearray& data) override;

    /// @brief Wait until bytes arrive, or the port goes away, or @p timeout runs out.
    /// @note The wait polls the driver, so it is only as precise as the polling interval;
    ///       a serial port has no waitable handle to block on.
    bool waitForReadyRead(std::chrono::milliseconds timeout = std::chrono::seconds(5)) override;

    // ── lines ────────────────────────────────────────────────────────

    /// @brief Throw away what is queued in the driver.
    bool purge(purge_target what);

    /// @brief How many bytes are still queued in the driver for transmission.
    /// @note 0 when the port is closed or the system cannot say.
    size_t bytesToWrite() const;

    /// @brief Read the modem input lines.
    modem_lines modem();

    /// @brief Assert or drop DTR.
    bool setDtr(bool asserted);

    /// @brief Assert or drop RTS.
    /// @note On Windows hardware flow control takes RTS over and undoes this.
    bool setRts(bool asserted);

    /// @brief Hold the line in the break condition for @p duration, then release it.
    /// @note POSIX sends the system's own break (at least 0.25 s on most systems) and
    ///       ignores the duration.
    bool sendBreak(std::chrono::milliseconds duration = std::chrono::milliseconds(250));

    /// @brief Wait until the driver's output queue is empty.
    /// @note This is the portable stand-in for flushing: it does not force the bytes out,
    ///       it waits until the driver has none left.
    bool waitForBytesWritten(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    // ── state ────────────────────────────────────────────────────────

    /// @brief Whether an operation has found this port gone. Cleared by open().
    bool broken() const;

    /// @brief What last went wrong, in the system's own words. Empty when nothing has.
    std::string lastError() const;

    /// @brief The platform error code behind lastError(): a Win32 error or errno. 0 when the
    ///        failure was this module's own (invalid settings, port not open).
    std::uint32_t lastErrorCode() const;

    native_handle_type nativeHandle() const;

    // ── the system's ports ───────────────────────────────────────────

    /// @brief The serial ports the system reports, sorted naturally (COM2 before COM10).
    /// @note Windows answers from the registry's device map, POSIX by looking for the usual
    ///       device names under /dev. A port that is listed may still refuse to open.
    static std::vector<std::string> availablePorts();

    /// @brief Whether a port with this name exists, without opening (and claiming) it.
    static bool portExists(const std::string& name);

private:
    void setError(std::uint32_t code, std::string text);

    native_handle_type m_handle = invalid_handle;
    std::string m_name;
    settings m_settings;
    std::chrono::milliseconds m_read_timeout{ 1000 };
    std::chrono::milliseconds m_write_timeout{ 1000 };
    std::string m_last_error;
    std::uint32_t m_last_error_code = 0;
    bool m_broken = false;
};

} // namespace scl2