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

        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();
        std::string timeStr = ss.str();

        std::string levelStr;
        switch (level) {
            case LogLevel::Debug:   levelStr = "[DEBUG]"; break;
            case LogLevel::Info:    levelStr = "[INFO] "; break;
            case LogLevel::Warning: levelStr = "[WARN] "; break;
            case LogLevel::Error:   levelStr = "[ERROR]"; break;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << timeStr << " " << levelStr << " " << message << std::endl;

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
