#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "Types.hpp"
#include "EventManager.hpp"

namespace bot::core {

class Module {
public:
    virtual ~Module() = default;

    virtual std::string getName() const = 0;
    virtual std::string getDescription() const { return ""; }

    virtual bool init() { return true; }
    virtual void start() { m_enabled = true; }
    virtual void stop() { m_enabled = false; }
    virtual void tick(double deltaSeconds) = 0;

    virtual void onEvent(const Event& /*event*/) {}

    virtual void loadConfig(const nlohmann::json& /*config*/) {}
    virtual nlohmann::json saveConfig() const { return nlohmann::json::object(); }

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) {
        if (m_enabled != enabled) {
            m_enabled = enabled;
            if (m_enabled) {
                start();
            } else {
                stop();
            }
            EventManager::instance().publish("ModuleStateChanged", {
                {"module", getName()},
                {"enabled", m_enabled}
            });
        }
    }

protected:
    bool m_enabled{false};
};

} // namespace bot::core
