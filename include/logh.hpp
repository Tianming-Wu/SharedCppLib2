/*
    Log Header-Only (logh) - A simple header-only logging library for C++23

    You can include this header as #include <SharedCppLib2/logh.hpp> or copy it to your project
    and include it as #include "logh.hpp".

    By default, logh outputs to console.
    You can enable file output by calling `logh::setFileOutput("log.txt")` to output to a file,
    this would not disable console output automatically.
    To disable console output, call `logh::setConsoleOutput(false)`.
    You can only output to one file at a time, and the file will be opened in append mode.
    `setFileOutput()` answers whether the file is open now; a file that will not open is not an
    exception, it is reported to `logh::setErrorHandler()` (stderr by default) and the file that
    was already open keeps being written to.

    Available compile-control macros:
        - LOGH_DISABLE : Completely remove logh from your source.
                         The `logh_*` macros will be redirected to empty implementation, so your
                         debug log can automatically disappears in release build.
                         Note that the arguments are not evaluated (and the `logh` class does not
                         exist), so a side effect inside a log call disappears with it.
        - LOGH_AUTOSIGNATURE: Enable automatic signature feature. It will generate the function name
                              of the log call site, and replace the signature argument with it.
                              Note that if you enabled this, you cannot input signature manually.
                              It changes the signature of `logh::info()` and friends, so it has to be
                              defined the same way in every translation unit of a program.
                              It's recommended to use a central header file.

    The output channel is a process-wide static state, and is not synchronized: logging from
    several threads at once needs the caller to serialize.

    [SCL_STANDALONE_MODULE]
    version: 2.0.1
    cpp_generation: cxx20 - cxx23
*/

#pragma once

#ifndef LOGH_DISABLE

#include <string>
#include <string_view>
#include <fstream>
#include <iostream>
#include <iterator> // for std::back_inserter
#include <chrono>   // for timestamp features
#include <ctime>    // for std::localtime / std::time_t
#include <format>

// Receives the one-line reason of a failure (a log file that will not open, for now).
using logh_error_handler = void (*)(const std::string&);

// output control helper object for logh
// not for user.
class logh_channel
{
    friend class logh;
private:
    logh_channel() = default;

    logh_channel(const logh_channel&) = delete;
    logh_channel& operator=(const logh_channel&) = delete;
    logh_channel(logh_channel&&) = delete;
    logh_channel& operator=(logh_channel&&) = delete;

    inline void submit(const std::string_view message) noexcept {
        if (enable_console) {
            std::cout << message << std::endl;
        }
        if (enable_file && ofs.is_open()) {
            ofs << message << std::endl;
        }
    }

    // Returns false when the file cannot be opened. Opening a log file cannot decide whether
    // the program survives, so nothing is thrown: the reason goes to the error handler
    // (stderr by default). The file that is already open keeps working, because the new one
    // is opened first and only swapped in once it succeeded.
    inline bool setFileOutput(const std::string& filename) {
        if (filename.empty()) {
            // disable file output
            enable_file = false;
            if (ofs.is_open()) ofs.close();
            return true;
        }

        std::ofstream candidate(filename, std::ios::app);
        if (!candidate.is_open()) {
            reportError("failed to open log file: " + filename);
            return false;
        }

        if (ofs.is_open()) ofs.close();
        ofs = std::move(candidate);
        enable_file = true;
        return true;
    }

    inline void setConsoleOutput(bool enable) {
        enable_console = enable;
    }

    inline static void reportError(const std::string& message) {
        (handler ? handler : defaultHandler)(message);
    }

    inline static void defaultHandler(const std::string& message) {
        std::cerr << "[logh] " << message << std::endl;
    }

    bool enable_console = true;
    bool enable_file = false;

    // for file output.
    // user can only select a sinle file output at a time.
    std::ofstream ofs;

    inline static logh_error_handler handler = nullptr;
};

#ifdef LOGH_AUTOSIGNATURE
namespace plogh {
    constexpr std::string_view function_name(std::string_view signature) {
        constexpr auto is_word_start = [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '_'
                || c == '`' || c == '{'; // `anonymous-namespace', {anonymous}
        };
        // true if the identifier right in front of `pos` (spaces ignored) is `word`.
        constexpr auto word_before = [](std::string_view s, std::size_t pos, std::string_view word) {
            while (pos > 0 && s[pos - 1] == ' ') --pos;
            return pos >= word.size() && s.substr(pos - word.size(), word.size()) == word;
        };
        constexpr std::size_t npos = std::string_view::npos;

        // The parameter list is the first '(' outside <...> that is neither preceded by a
        // space — a parameter list is glued to the name, while the parentheses a return type
        // may carry sit after one — nor the empty pair of an `operator ()`.
        std::size_t angle = 0, paren = npos;
        for (std::size_t i = 0; i < signature.size() && paren == npos; ++i) {
            const char c = signature[i];
            if (c == '<') ++angle;
            else if (c == '>') { if (angle) --angle; }
            else if (c == '(' && angle == 0 && i > 0 && signature[i - 1] != ' '
                    && !word_before(signature, i, "operator"))
                paren = i;
        }

        // `operator<`, `operator<<` and `operator<=` spell a '<' that reads as the start of
        // template arguments above, and MSVC writes `operator <(…)` with a space before the
        // parentheses, so neither is found; behind an `operator` keyword the next '(' is the
        // parameter list, less the empty pair that `operator ()` owns.
        if (paren == npos) {
            for (std::size_t i = 0; i + 8 <= signature.size(); ++i) {
                if (signature.compare(i, 8, "operator") != 0) continue;
                if (i > 0 && is_word_start(signature[i - 1])) continue;
                if (i + 8 < signature.size() && is_word_start(signature[i + 8])) continue;
                std::size_t j = i + 8;
                while (j < signature.size() && signature[j] == ' ') ++j;
                if (j + 1 < signature.size() && signature[j] == '(' && signature[j + 1] == ')')
                    j += 2;
                paren = signature.find('(', j);
                break;
            }
        }
        if (paren == npos) return signature;

        // In front of it the name starts after the last space outside <...> that a word
        // follows. The return type and the calling convention are before that space; the
        // spaces inside `operator +` and `operator new` are inside the name.
        std::size_t start = 0;
        angle = 0;
        for (std::size_t i = 0; i + 1 < paren; ++i) {
            const char c = signature[i];
            if (c == '<') ++angle;
            else if (c == '>') { if (angle) --angle; }
            else if (c == ' ' && angle == 0 && is_word_start(signature[i + 1])) {
                std::size_t word = i, begin = word;
                while (begin > 0 && is_word_start(signature[begin - 1])) --begin;
                if (signature.substr(begin, word - begin) != "operator")
                    start = i + 1;
            }
        }

        std::string_view name = signature.substr(start, paren - start);

        // A calling convention says nothing about where the line came from. An anonymous
        // namespace does, so it stays — and it stays spelled the way the compiler writes it:
        // `anonymous-namespace'::helper on MSVC, {anonymous}::helper on GCC / Clang.
        for (std::string_view prefix : { "__cdecl ", "__stdcall ", "__fastcall ",
                                         "__vectorcall ", "__thiscall ", "__clrcall " }) {
            if (name.size() > prefix.size() && name.substr(0, prefix.size()) == prefix) {
                name.remove_prefix(prefix.size());
                break;
            }
        }
        return name;
    }
} // namespace plogh

#if defined(_MSC_VER)
    #define LOGH_FUNCSIG __FUNCSIG__
#elif defined(__GNUC__) || defined(__clang__)
    #define LOGH_FUNCSIG __PRETTY_FUNCTION__
#else
    #define LOGH_FUNCSIG __func__
#endif

    #define PLOGH_AUTOSIGNATURE ::plogh::function_name(LOGH_FUNCSIG)

    #define plogh_generate(LNAME, UNAME) \
    template <typename... Args> \
    static void LNAME(const std::string_view signature, const std::string_view format_str, Args&&... args) noexcept { \
        try { \
            std::string line; \
            line.reserve(kLineReserve); \
            line.append("[" UNAME "] "); \
            appendTimestamp(line, std::chrono::system_clock::now()); \
            line.append(" ["); \
            line.append(signature); \
            line.append("] "); \
            appendFormatted(line, format_str, std::forward<Args>(args)...); \
            p_out.submit(line); \
        } catch (const std::bad_alloc&) { \
            p_out.submit(kOutOfMemory); \
        } \
    }
#else
    #define plogh_generate(LNAME, UNAME) \
    template <typename... Args> \
    static void LNAME(const std::string_view format_str, Args&&... args) noexcept { \
        try { \
            std::string line; \
            line.reserve(kLineReserve); \
            line.append("[" UNAME "] "); \
            appendTimestamp(line, std::chrono::system_clock::now()); \
            line.push_back(' '); \
            appendFormatted(line, format_str, std::forward<Args>(args)...); \
            p_out.submit(line); \
        } catch (const std::bad_alloc&) { \
            p_out.submit(kOutOfMemory); \
        } \
    }
#endif


// basic logh object
// the overall control logic is all static
class logh
{
protected:
    // Formats the arguments straight into `out`, so a log line costs one buffer instead of
    // several temporaries. A broken format string is reported in place, never thrown, and
    // whatever the format wrote before it failed is rolled back.
    template <typename... Args>
    inline static void appendFormatted(std::string& out, const std::string_view format_str, Args&&... args) {
        const std::size_t mark = out.size();
        try {
            std::vformat_to(std::back_inserter(out), format_str, std::make_format_args(args...));
        } catch (const std::format_error& e) {
            out.resize(mark);
            out.append("<format error: ").append(e.what())
                .append(" | Original format string: ").append(format_str).append(">");
        }
    }

    // Appends "[2026-10-21 16:00:00]".
    // std::put_time is not formattable by std::format, so use strftime here.
    inline static void appendTimestamp(std::string& out, const std::chrono::system_clock::time_point tp) {
        const std::time_t time = std::chrono::system_clock::to_time_t(tp);
        std::tm local{};
#if defined(_MSC_VER)
        localtime_s(&local, &time);
#else
        localtime_r(&time, &local);
#endif
        char buffer[32]{};
        std::strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", &local);
        out.append(buffer);
    }

public:
    // raw log without any logh formatting.
    template <typename... Args>
    static void log(const std::string_view format_str, Args&&... args) noexcept {
        try {
            std::string line;
            line.reserve(kLineReserve);
            appendFormatted(line, format_str, std::forward<Args>(args)...);
            p_out.submit(line);
        } catch (const std::bad_alloc&) {
            p_out.submit(kOutOfMemory);
        }
    }

    plogh_generate(debug, "DEBUG")
    plogh_generate(info, "INFO")
    plogh_generate(warn, "WARN")
    plogh_generate(error, "ERROR")
    plogh_generate(fatal, "FATAL")

    inline static bool setFileOutput(const std::string& filename) { return p_out.setFileOutput(filename); }
    inline static void setConsoleOutput(bool enable) { p_out.setConsoleOutput(enable); }

    // Receives the one-line reason of a failure; pass nullptr to go back to stderr.
    inline static void setErrorHandler(const logh_error_handler error_handler) {
        logh_channel::handler = error_handler;
    }

private:
    // header-only: must be `inline` so that every TU shares one definition.
    inline static logh_channel p_out{};

    // initial capacity of a log line, enough for a short line without growing.
    static constexpr std::size_t kLineReserve = 96;

    // Said when a line cannot be built at all. Submitted without allocating anything, so it
    // cannot fail the same way; a log call must never take the program down.
    static constexpr std::string_view kOutOfMemory = "[logh] out of memory, a log line was dropped";
};

// output macros
//
// The signature (when LOGH_AUTOSIGNATURE is on) is the *first* parameter of logh::info() and
// friends, so the macro only has to prepend an argument: no `##__VA_ARGS__` (a GNU extension
// that deletes the comma in front of empty __VA_ARGS__) and no __VA_OPT__ (which the classic
// MSVC preprocessor does not implement) is needed.

#ifdef LOGH_AUTOSIGNATURE
    #define logh_debug(...) logh::debug(PLOGH_AUTOSIGNATURE, __VA_ARGS__)
    #define logh_info(...)  logh::info(PLOGH_AUTOSIGNATURE, __VA_ARGS__)
    #define logh_warn(...)  logh::warn(PLOGH_AUTOSIGNATURE, __VA_ARGS__)
    #define logh_error(...) logh::error(PLOGH_AUTOSIGNATURE, __VA_ARGS__)
    #define logh_fatal(...) logh::fatal(PLOGH_AUTOSIGNATURE, __VA_ARGS__)
#else
    #define logh_debug(...) logh::debug(__VA_ARGS__)
    #define logh_info(...)  logh::info(__VA_ARGS__)
    #define logh_warn(...)  logh::warn(__VA_ARGS__)
    #define logh_error(...) logh::error(__VA_ARGS__)
    #define logh_fatal(...) logh::fatal(__VA_ARGS__)
#endif

#else // ifdef LOGH_DISABLE

// we keep these macros, but redirect them to empty implementation,
// so existing logh calls in the code will simply disappear in the build.
#define logh_debug(...)
#define logh_info(...)
#define logh_warn(...)
#define logh_error(...)
#define logh_fatal(...)

#endif // LOGH_DISABLE