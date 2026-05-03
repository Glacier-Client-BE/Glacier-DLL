#pragma once
#include <string>
#include <string_view>
#include <mutex>

namespace Glacier {

enum class LogLevel { Trace, Info, Warn, Error };

class Logger {
public:
    static Logger& get();

    void open(const std::wstring& filePath);
    void close();

    void log(LogLevel lvl, std::string_view tag, std::string_view msg);

    // sugar
    template <class... Args> void info (std::string_view tag, Args&&... a) { logFmt(LogLevel::Info,  tag, std::forward<Args>(a)...); }
    template <class... Args> void warn (std::string_view tag, Args&&... a) { logFmt(LogLevel::Warn,  tag, std::forward<Args>(a)...); }
    template <class... Args> void error(std::string_view tag, Args&&... a) { logFmt(LogLevel::Error, tag, std::forward<Args>(a)...); }
    template <class... Args> void trace(std::string_view tag, Args&&... a) { logFmt(LogLevel::Trace, tag, std::forward<Args>(a)...); }

private:
    Logger() = default;
    ~Logger();

    template <class... Args>
    void logFmt(LogLevel lvl, std::string_view tag, Args&&... a) {
        std::string buf;
        ((buf += toStr(std::forward<Args>(a))), ...);
        log(lvl, tag, buf);
    }

    static std::string toStr(const char* s)        { return s ? s : ""; }
    static std::string toStr(std::string s)        { return s; }
    static std::string toStr(std::string_view s)   { return std::string(s); }
    static std::string toStr(int v)                { return std::to_string(v); }
    static std::string toStr(unsigned v)           { return std::to_string(v); }
    static std::string toStr(long v)               { return std::to_string(v); }
    static std::string toStr(long long v)          { return std::to_string(v); }
    static std::string toStr(unsigned long v)      { return std::to_string(v); }
    static std::string toStr(unsigned long long v) { return std::to_string(v); }
    static std::string toStr(float v)              { return std::to_string(v); }
    static std::string toStr(double v)             { return std::to_string(v); }
    static std::string toStr(bool v)               { return v ? "true" : "false"; }
    static std::string toStr(const void* p)        { char b[20]; std::snprintf(b, sizeof(b), "0x%p", p); return b; }

    std::mutex  m_;
    void*       file_  = nullptr; // FILE*
    bool        owned_ = false;
};

} // namespace Glacier
