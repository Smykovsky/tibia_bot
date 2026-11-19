#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "Module.hpp"
#include "Logger.hpp"

namespace bot::core {

class ModuleManager {
public:
    static ModuleManager& instance() {
        static ModuleManager inst;
        return inst;
    }

    void registerModule(std::shared_ptr<Module> module) {
        if (!module) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string name = module->getName();
        m_modules[name] = module;
        m_moduleOrder.push_back(name);
        Logger::info("Registered module: " + name);
    }

    std::shared_ptr<Module> getModule(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_modules.find(name);
        if (it != m_modules.end()) {
            return it->second;
        }
        return nullptr;
    }

    template<typename T>
    std::shared_ptr<T> getModuleAs(const std::string& name) const {
        return std::dynamic_pointer_cast<T>(getModule(name));
    }

    bool initAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        bool allOk = true;
        for (const auto& name : m_moduleOrder) {
            auto& mod = m_modules[name];
            try {
                if (!mod->init()) {
                    Logger::error("Failed to init module: " + name);
                    allOk = false;
                }
            } catch (const std::exception& e) {
                Logger::error("Exception in init module " + name + ": " + e.what());
                allOk = false;
            }
        }
        return allOk;
    }

    void tickAll(double deltaSeconds) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& name : m_moduleOrder) {
            auto& mod = m_modules[name];
            if (mod->isEnabled()) {
                try {
                    mod->tick(deltaSeconds);
                } catch (const std::exception& e) {
                    Logger::error("Exception in module " + name + " tick: " + e.what());
                }
            }
        }
    }

    void stopAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& name : m_moduleOrder) {
            auto& mod = m_modules[name];
            if (mod->isEnabled()) {
                try {
                    mod->stop();
                } catch (const std::exception& e) {
                    Logger::error("Exception stopping module " + name + ": " + e.what());
                }
            }
        }
    }

    nlohmann::json getStatusJson() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        nlohmann::json res = nlohmann::json::array();
        for (const auto& name : m_moduleOrder) {
            const auto& mod = m_modules.at(name);
            res.push_back({
                {"name", mod->getName()},
                {"description", mod->getDescription()},
                {"enabled", mod->isEnabled()}
            });
        }
        return res;
    }

    void applyConfig(const nlohmann::json& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& name : m_moduleOrder) {
            auto& mod = m_modules[name];
            if (config.contains(name)) {
                try {
                    mod->loadConfig(config[name]);
                } catch (const std::exception& e) {
                    Logger::error("Error loading config for " + name + ": " + e.what());
                }
            }
        }
    }

    nlohmann::json exportConfig() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        nlohmann::json config = nlohmann::json::object();
        for (const auto& name : m_moduleOrder) {
            const auto& mod = m_modules.at(name);
            config[name] = mod->saveConfig();
        }
        return config;
    }

private:
    ModuleManager() = default;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<Module>> m_modules;
    std::vector<std::string> m_moduleOrder;
};

} // namespace bot::core
