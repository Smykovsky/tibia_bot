#pragma once

#include <vector>
#include <string>
#include "engine/core/Module.hpp"
#include "engine/game/GameState.hpp"
#include "engine/game/IGameAdapter.hpp"

namespace bot::modules {

enum class WaypointType {
    Walk,
    Node,
    Stand,
    Rope,
    Shovel,
    Ladder,
    Action
};

struct Waypoint {
    WaypointType type{WaypointType::Walk};
    core::Position position{0, 0, 0};
    std::string label;
    std::string script; // For Action waypoints
    int tolerance{1};
};

class CavebotModule : public core::Module {
public:
    explicit CavebotModule(std::shared_ptr<game::IGameAdapter> adapter)
        : m_adapter(std::move(adapter)) {}

    std::string getName() const override { return "Cavebot"; }
    std::string getDescription() const override { return "Autonomous waypoint path navigation and routing"; }

    void addWaypoint(const Waypoint& wp) {
        m_waypoints.push_back(wp);
    }

    void clearWaypoints() {
        m_waypoints.clear();
        m_currentIndex = 0;
    }

    size_t getCurrentIndex() const { return m_currentIndex; }
    void setCurrentIndex(size_t index) {
        if (index < m_waypoints.size()) {
            m_currentIndex = index;
        }
    }

    const std::vector<Waypoint>& getWaypoints() const { return m_waypoints; }

    void tick(double /*deltaSeconds*/) override {
        if (!m_adapter || !m_adapter->isConnected()) return;
        if (m_waypoints.empty()) return;

        if (m_currentIndex >= m_waypoints.size()) {
            m_currentIndex = 0; // Loop back
        }

        const auto& currentWp = m_waypoints[m_currentIndex];
        game::Player player = game::GameState::instance().getPlayer();

        // Check if reached waypoint
        int dist = currentWp.position.chebyshevDistance(player.position);
        int tolerance = (currentWp.type == WaypointType::Stand) ? 0 : currentWp.tolerance;

        if (dist <= tolerance) {
            handleWaypointArrival(currentWp);
            m_currentIndex = (m_currentIndex + 1) % m_waypoints.size();
            core::EventManager::instance().publish("WaypointReached", {
                {"index", m_currentIndex},
                {"label", currentWp.label}
            });
            return;
        }

        // Move towards waypoint
        m_adapter->walkTo(currentWp.position);
    }

    void loadConfig(const nlohmann::json& config) override {
        if (!config.contains("waypoints") || !config["waypoints"].is_array()) return;
        m_waypoints.clear();
        m_currentIndex = 0;
        for (const auto& jWp : config["waypoints"]) {
            Waypoint wp;
            std::string t = jWp.value("type", "walk");
            if (t == "stand") wp.type = WaypointType::Stand;
            else if (t == "node") wp.type = WaypointType::Node;
            else if (t == "rope") wp.type = WaypointType::Rope;
            else if (t == "ladder") wp.type = WaypointType::Ladder;
            else if (t == "shovel") wp.type = WaypointType::Shovel;
            else if (t == "action") wp.type = WaypointType::Action;
            else wp.type = WaypointType::Walk;

            wp.position.x = jWp.value("x", 0);
            wp.position.y = jWp.value("y", 0);
            wp.position.z = jWp.value("z", 7);
            wp.label = jWp.value("label", "");
            wp.script = jWp.value("script", "");
            wp.tolerance = jWp.value("tolerance", 1);
            m_waypoints.push_back(wp);
        }
    }

    nlohmann::json saveConfig() const override {
        nlohmann::json j;
        nlohmann::json wpArray = nlohmann::json::array();
        for (const auto& wp : m_waypoints) {
            std::string typeStr = "walk";
            switch (wp.type) {
                case WaypointType::Stand:  typeStr = "stand"; break;
                case WaypointType::Node:   typeStr = "node"; break;
                case WaypointType::Rope:   typeStr = "rope"; break;
                case WaypointType::Ladder: typeStr = "ladder"; break;
                case WaypointType::Shovel: typeStr = "shovel"; break;
                case WaypointType::Action: typeStr = "action"; break;
                default: break;
            }
            wpArray.push_back({
                {"type", typeStr},
                {"x", wp.position.x},
                {"y", wp.position.y},
                {"z", wp.position.z},
                {"label", wp.label},
                {"script", wp.script},
                {"tolerance", wp.tolerance}
            });
        }
        j["waypoints"] = wpArray;
        return j;
    }

private:
    void handleWaypointArrival(const Waypoint& wp) {
        if (wp.type == WaypointType::Rope) {
            // e.g. use rope item (id 3003) on tile
            m_adapter->useItemWith(3003, wp.position);
        } else if (wp.type == WaypointType::Shovel) {
            m_adapter->useItemWith(3457, wp.position);
        } else if (wp.type == WaypointType::Action && !wp.script.empty()) {
            core::EventManager::instance().publish("ExecuteScript", {{"script", wp.script}});
        }
    }

    std::shared_ptr<game::IGameAdapter> m_adapter;
    std::vector<Waypoint> m_waypoints;
    size_t m_currentIndex{0};
};

} // namespace bot::modules
