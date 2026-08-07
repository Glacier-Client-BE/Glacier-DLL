#pragma once

#include <atomic>
#include <cstdio>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <Windows.h>

// Lightweight, thread-safe logger that writes to a debug console (allocated by
// the client on attach), to OutputDebugString, and to a file.
//
// The file is the one that matters when something goes wrong. A console dies
// with the process, so the exact case worth diagnosing — the game crashing or
// hanging — is the case where the output is gone before it can be read. Every
// line is flushed as it is written, so whatever reached the log survives even
// an abrupt termination: the last line in the file is the last thing Glacier
// did.
//
// The previous session is kept alongside it, because after a crash the user's
// next action is to relaunch, and that would otherwise overwrite the only
// record of what happened.
namespace glacier {

enum class LogLevel { Trace, Info, Warn, Error };

class Logger {
public:
    static Logger& get() {
        static Logger instance;
        return instance;
    }

    void attachConsole() {
        std::scoped_lock lock(m_mutex);
        if (m_console) return;
        AllocConsole();
        SetConsoleTitleW(L"Glacier // debug");
        freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);
        freopen_s(reinterpret_cast<FILE**>(stderr), "CONOUT$", "w", stderr);

        // The console defaults to the system OEM codepage, which mangles the
        // UTF-8 our log strings are encoded in — em dashes arrive as "ÔÇö".
        SetConsoleOutputCP(CP_UTF8);

        // ANSI colour codes are inert unless virtual-terminal processing is
        // turned on; without this the escapes print literally as "<-[36m".
        // Not available on very old Windows, so failure just means no colour.
        if (const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
            out && out != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (GetConsoleMode(out, &mode)) {
                m_color = SetConsoleMode(
                    out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
            }
        }

        m_console = true;
    }

    void detachConsole() {
        std::scoped_lock lock(m_mutex);
        if (!m_console) return;
        FreeConsole();
        m_console = false;
    }

    // Starts writing to `path`. The caller owns creating the directory.
    //
    // Any existing log is moved aside to "<path>.prev" first: after a crash the
    // next thing that happens is a relaunch, and without this the relaunch
    // would destroy the only record of the crash.
    void openFile(const std::string& path) {
        std::scoped_lock lock(m_mutex);
        if (m_file) return;

        const std::string previous = path + ".prev";
        DeleteFileA(previous.c_str());
        MoveFileA(path.c_str(), previous.c_str());   // fails harmlessly on first run

        if (fopen_s(&m_file, path.c_str(), "w") != 0 || !m_file) {
            m_file = nullptr;
            // Written directly rather than through LOG_WARN: the mutex is held
            // here and it is not recursive, so logging normally would deadlock
            // on the very path meant to report a problem.
            if (m_console) {
                std::fputs("[Glacier][warn] could not open the log file — diagnostics "
                           "will be lost if the game crashes\n", stdout);
                std::fflush(stdout);
            }
            return;
        }
        m_path = path;
    }

    void closeFile() {
        std::scoped_lock lock(m_mutex);
        if (!m_file) return;
        std::fclose(m_file);
        m_file = nullptr;
    }

    const std::string& filePath() const { return m_path; }

    template <typename... Args>
    void log(LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
        const std::string body = std::format(fmt, std::forward<Args>(args)...);
        const std::string line = std::format("[Glacier][{}] {}\n", levelTag(level), body);

        std::scoped_lock lock(m_mutex);
        if (m_console) {
            std::fputs(colorFor(level), stdout);
            std::fputs(line.c_str(), stdout);
            std::fputs("\033[0m", stdout);
            std::fflush(stdout);
        }
        if (m_file) {
            // Timestamped, so a line can be lined up against the game's own
            // crash report, and un-coloured so the file stays readable in a
            // text editor.
            SYSTEMTIME t{};
            GetLocalTime(&t);
            std::fprintf(m_file, "%02d:%02d:%02d.%03d [%s] %s\n",
                         t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
                         levelTag(level), body.c_str());

            // Flushed per line, deliberately. Buffering would lose exactly the
            // lines written just before a crash — the only ones that matter.
            // fflush hands the bytes to the OS, which survives the process
            // dying, so this is enough without the cost of a real fsync.
            std::fflush(m_file);
        }
        OutputDebugStringA(line.c_str());
    }

private:
    Logger() = default;

    static constexpr const char* levelTag(LogLevel l) {
        switch (l) {
            case LogLevel::Trace: return "trace";
            case LogLevel::Info:  return "info";
            case LogLevel::Warn:  return "warn";
            case LogLevel::Error: return "error";
        }
        return "?";
    }

    static constexpr const char* colorFor(LogLevel l) {
        switch (l) {
            case LogLevel::Trace: return "\033[90m";
            case LogLevel::Info:  return "\033[36m";
            case LogLevel::Warn:  return "\033[33m";
            case LogLevel::Error: return "\033[31m";
        }
        return "\033[0m";
    }

    std::mutex  m_mutex;
    std::FILE*  m_file = nullptr;
    std::string m_path;
    bool m_console = false;
    bool m_color   = false;
};

} // namespace glacier

// Logs the first time this line is reached and never again.
//
// For per-frame paths, where the useful question is "did execution ever get
// this far" rather than "what happened this frame". A plain LOG_INFO in a
// render callback emits thousands of lines a second and buries the answer;
// these mark the stages a frame passes through, so the last one in the file
// says how far the last frame got before the process died.
#define LOG_ONCE(...)                                             \
    do {                                                          \
        static std::atomic<bool> glacierLoggedOnce_{ false };      \
        if (!glacierLoggedOnce_.exchange(true,                     \
                std::memory_order_relaxed)) {                      \
            ::glacier::Logger::get().log(                          \
                ::glacier::LogLevel::Trace, __VA_ARGS__);          \
        }                                                          \
    } while (0)

#define LOG_TRACE(...) ::glacier::Logger::get().log(::glacier::LogLevel::Trace, __VA_ARGS__)
#define LOG_INFO(...)  ::glacier::Logger::get().log(::glacier::LogLevel::Info,  __VA_ARGS__)
#define LOG_WARN(...)  ::glacier::Logger::get().log(::glacier::LogLevel::Warn,  __VA_ARGS__)
#define LOG_ERROR(...) ::glacier::Logger::get().log(::glacier::LogLevel::Error, __VA_ARGS__)
