#include "Logger.h"
#include "ClientConfig.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

void Logger::init() {
    std::lock_guard<std::mutex> lk(s_mutex);
    std::string path = ClientConfig::get().resolvePath("glacier.log");
    s_file.open(path, std::ios::out | std::ios::trunc);
    s_ready = s_file.is_open();
    if (!s_ready)
        OutputDebugStringA("[Glacier] WARNING: Could not open glacier.log\n");
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lk(s_mutex);
    if (s_file.is_open()) s_file.close();
    s_ready = false;
}

void Logger::log(const char* level, const std::string& msg) {
    // Timestamp
    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &time);
    std::ostringstream ts;
    ts << std::put_time(&tm, "%H:%M:%S");

    std::string line = "[" + ts.str() + "] [" + level + "] " + msg + "\n";

    // Debug output (visible in any debugger / DebugView)
    OutputDebugStringA(line.c_str());

    std::lock_guard<std::mutex> lk(s_mutex);
    if (s_ready && s_file.is_open()) {
        s_file << line;
        s_file.flush();
    }
}
