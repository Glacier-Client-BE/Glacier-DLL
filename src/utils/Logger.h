#pragma once
#include <string>
#include <format>
#include <fstream>
#include <mutex>
#include <Windows.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Logger — thread-safe file + debug output logger.
//  Uses std::format (C++20). Falls back gracefully if the log file can't be opened.
// ─────────────────────────────────────────────────────────────────────────────
class Logger {
public:
    static void init();
    static void shutdown();

    template<typename... Args>
    static void info(std::format_string<Args...> fmt, Args&&... args) {
        log("INFO ", std::format(fmt, std::forward<Args>(args)...));
    }
    template<typename... Args>
    static void warn(std::format_string<Args...> fmt, Args&&... args) {
        log("WARN ", std::format(fmt, std::forward<Args>(args)...));
    }
    template<typename... Args>
    static void error(std::format_string<Args...> fmt, Args&&... args) {
        log("ERROR", std::format(fmt, std::forward<Args>(args)...));
    }

    // Convenience overloads for plain string
    static void info (const std::string& msg) { log("INFO ", msg); }
    static void warn (const std::string& msg) { log("WARN ", msg); }
    static void error(const std::string& msg) { log("ERROR", msg); }

private:
    static void log(const char* level, const std::string& msg);

    static inline std::ofstream s_file;
    static inline std::mutex    s_mutex;
    static inline bool          s_ready = false;
};
