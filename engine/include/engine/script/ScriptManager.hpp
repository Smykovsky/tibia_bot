#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <mutex>
#include "LuaEngine.hpp"
#include "engine/core/Logger.hpp"

namespace bot::script {

struct UserScript {
    std::string name;
    std::string filePath;
    bool enabled{false};
    std::string code;
};

class ScriptManager {
public:
    static ScriptManager& instance() {
        static ScriptManager inst;
        return inst;
    }

    void setScriptsDir(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_scriptsDir = path;
        std::filesystem::create_directories(m_scriptsDir);
    }

    void scanScripts() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!std::filesystem::exists(m_scriptsDir)) return;

        for (const auto& entry : std::filesystem::directory_iterator(m_scriptsDir)) {
            if (entry.path().extension() == ".lua") {
                std::string name = entry.path().stem().string();
                if (m_scripts.find(name) == m_scripts.end()) {
                    UserScript s;
                    s.name = name;
                    s.filePath = entry.path().string();
                    s.enabled = false;
                    m_scripts[name] = s;
                }
            }
        }
    }

    bool runScript(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_scripts.find(name);
        if (it == m_scripts.end()) {
            core::Logger::error("Script not found: " + name);
            return false;
        }

        std::string err;
        return LuaEngine::instance().executeFile(it->second.filePath, &err);
    }

    std::vector<UserScript> getScripts() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<UserScript> list;
        for (const auto& [_, s] : m_scripts) {
            list.push_back(s);
        }
        return list;
    }

private:
    ScriptManager() : m_scriptsDir("scripts") {}

    mutable std::mutex m_mutex;
    std::string m_scriptsDir;
    std::unordered_map<std::string, UserScript> m_scripts;
};

} // namespace bot::script
