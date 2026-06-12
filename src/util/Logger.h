#pragma once

#include <cstdio>
#include <format>
#include <mutex>
#include <string_view>
#include <Windows.h>

// Lightweight, thread-safe logger that writes to a debug console (allocated by
// the client on attach) and to OutputDebugString so logs survive even when the
// console is closed. Compiled out of Release-noconsole builds via GLACIER_LOG.
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
        m_console = true;
    }

    void detachConsole() {
        std::scoped_lock lock(m_mutex);
        if (!m_console) return;
        FreeConsole();
        m_console = false;
    }

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

    std::mutex m_mutex;
    bool m_console = false;
};

} // namespace glacier

#define LOG_TRACE(...) ::glacier::Logger::get().log(::glacier::LogLevel::Trace, __VA_ARGS__)
#define LOG_INFO(...)  ::glacier::Logger::get().log(::glacier::LogLevel::Info,  __VA_ARGS__)
#define LOG_WARN(...)  ::glacier::Logger::get().log(::glacier::LogLevel::Warn,  __VA_ARGS__)
#define LOG_ERROR(...) ::glacier::Logger::get().log(::glacier::LogLevel::Error, __VA_ARGS__)
