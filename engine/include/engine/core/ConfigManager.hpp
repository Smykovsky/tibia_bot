#pragma once

#include <string>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <nlohmann/json.hpp>
#include "Logger.hpp"

namespace bot::core {

class ConfigManager {
public:
    static ConfigManager& instance() {
        static ConfigManager inst;
        return inst;
    }

    void setConfigDir(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_configDir = path;
        std::filesystem::create_directories(m_configDir);
    }

    bool loadProfile(const std::string& profileName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::filesystem::path filePath = std::filesystem::path(m_configDir) / (profileName + ".json");
        if (!std::filesystem::exists(filePath)) {
            Logger::warn("Profile file does not exist: " + filePath.string());
            m_currentConfig = nlohmann::json::object();
            m_currentProfile = profileName;
            return false;
        }

        try {
            std::ifstream file(filePath);
            m_currentConfig = nlohmann::json::parse(file);
            m_currentProfile = profileName;
            Logger::info("Loaded profile: " + profileName);
            return true;
        } catch (const std::exception& e) {
            Logger::error("Failed to parse config profile " + profileName + ": " + e.what());
            return false;
        }
    }

    bool saveProfile(const std::string& profileName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::filesystem::create_directories(m_configDir);
        std::filesystem::path filePath = std::filesystem::path(m_configDir) / (profileName + ".json");

        try {
            std::ofstream file(filePath);
            file << m_currentConfig.dump(4);
            Logger::info("Saved profile to " + filePath.string());
            return true;
        } catch (const std::exception& e) {
            Logger::error("Failed to save config profile " + profileName + ": " + e.what());
            return false;
        }
    }

    nlohmann::json getModuleConfig(const std::string& moduleName) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_currentConfig.contains(moduleName)) {
            return m_currentConfig[moduleName];
        }
        return nlohmann::json::object();
    }

    void setModuleConfig(const std::string& moduleName, const nlohmann::json& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentConfig[moduleName] = config;
    }

    nlohmann::json getAllConfig() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentConfig;
    }

    void setAllConfig(const nlohmann::json& fullConfig) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentConfig = fullConfig;
    }

    std::string getCurrentProfile() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentProfile;
    }

private:
    ConfigManager() : m_configDir("configs"), m_currentProfile("default") {}

    mutable std::mutex m_mutex;
    std::string m_configDir;
    std::string m_currentProfile;
    nlohmann::json m_currentConfig{nlohmann::json::object()};
};

} // namespace bot::core
