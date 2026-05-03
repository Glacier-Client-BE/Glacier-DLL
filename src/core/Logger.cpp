#include "Logger.h"
#include <Windows.h>
#include <cstdio>
#include <chrono>
#include <ctime>

namespace Glacier {

Logger& Logger::get() {
    static Logger L;
    return L;
}

Logger::~Logger() { close(); }

void Logger::open(const std::wstring& path) {
    std::scoped_lock lk(m_);
    if (file_) return;
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"a") == 0 && f) {
        file_  = f;
        owned_ = true;
    }
}

void Logger::close() {
    std::scoped_lock lk(m_);
    if (file_ && owned_) std::fclose(static_cast<FILE*>(file_));
    file_  = nullptr;
    owned_ = false;
}

static const char* lvlTag(LogLevel l) {
    switch (l) {
        case LogLevel::Trace: return "TRC";
        case LogLevel::Info:  return "INF";
        case LogLevel::Warn:  return "WRN";
        case LogLevel::Error: return "ERR";
    }
    return "?  ";
}

void Logger::log(LogLevel lvl, std::string_view tag, std::string_view msg) {
    auto t  = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    char ts[24];
    std::snprintf(ts, sizeof(ts), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);

    std::scoped_lock lk(m_);
    char line[512];
    std::snprintf(line, sizeof(line), "[%s][%s][%.*s] %.*s\n",
        ts, lvlTag(lvl),
        static_cast<int>(tag.size()), tag.data(),
        static_cast<int>(msg.size()), msg.data());

#ifdef _DEBUG
    OutputDebugStringA(line);
#endif
    if (file_) {
        std::fputs(line, static_cast<FILE*>(file_));
        std::fflush(static_cast<FILE*>(file_));
    }
}

} // namespace Glacier
