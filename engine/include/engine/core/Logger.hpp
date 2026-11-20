#pragma once

#include <string>
#include <iostream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <functional>
#include <vector>

namespace bot::core {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    using LogCallback = std::function<void(LogLevel level, const std::string& message, const std::string& timestamp)>;

    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void setLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_minLevel = level;
    }

    void addCallback(LogCallback cb) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callbacks.push_back(std::move(cb));
    }

    void log(LogLevel level, const std::string& message) {
        if (level < m_minLevel) return;

        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &in_time_t);
#else
        localtime_r(&in_time_t, &tm_buf);
#endif

        char timeBuf[64];
        std::snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                      tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                      tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                      static_cast<int>(ms.count()));
        std::string timeStr(timeBuf);

        std::string levelStr;
        switch (level) {
            case LogLevel::Debug:   levelStr = "[DEBUG]"; break;
            case LogLevel::Info:    levelStr = "[INFO] "; break;
            case LogLevel::Warning: levelStr = "[WARN] "; break;
            case LogLevel::Error:   levelStr = "[ERROR]"; break;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        std::fprintf(stdout, "%s %s %s\n", timeStr.c_str(), levelStr.c_str(), message.c_str());
        std::fflush(stdout);

        for (auto& cb : m_callbacks) {
            cb(level, message, timeStr);
        }
    }

    static void debug(const std::string& msg) { instance().log(LogLevel::Debug, msg); }
    static void info(const std::string& msg)  { instance().log(LogLevel::Info, msg); }
    static void warn(const std::string& msg)  { instance().log(LogLevel::Warning, msg); }
    static void error(const std::string& msg) { instance().log(LogLevel::Error, msg); }

private:
    Logger() = default;
    LogLevel m_minLevel{LogLevel::Info};
    std::mutex m_mutex;
    std::vector<LogCallback> m_callbacks;
};

} // namespace bot::core
