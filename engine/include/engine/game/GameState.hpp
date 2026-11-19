#pragma once

#include <vector>
#include <mutex>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "Player.hpp"
#include "Creature.hpp"
#include "Item.hpp"
#include "WorldMap.hpp"

namespace bot::game {

class GameState {
public:
    static GameState& instance() {
        static GameState inst;
        return inst;
    }

    void setPlayer(const Player& player) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_player = player;
    }

    Player getPlayer() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_player;
    }

    void setCreatures(const std::vector<Creature>& creatures) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_creatures = creatures;
    }

    std::vector<Creature> getCreatures() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_creatures;
    }

    std::vector<Creature> getMonstersInRange(int maxDistance = 8) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<Creature> result;
        for (const auto& c : m_creatures) {
            if (c.type == CreatureType::Monster && !c.isDead && c.isVisible) {
                if (c.position.chebyshevDistance(m_player.position) <= maxDistance) {
                    result.push_back(c);
                }
            }
        }
        return result;
    }

    Creature* findCreatureById(uint32_t id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& c : m_creatures) {
            if (c.id == id) return &c;
        }
        return nullptr;
    }

    void setContainers(const std::vector<Container>& containers) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_containers = containers;
    }

    std::vector<Container> getContainers() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_containers;
    }

    WorldMap& getWorldMap() {
        return m_worldMap;
    }

    const WorldMap& getWorldMap() const {
        return m_worldMap;
    }

    nlohmann::json toJson() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        nlohmann::json j;
        j["player"] = {
            {"id", m_player.id},
            {"name", m_player.name},
            {"position", {{"x", m_player.position.x}, {"y", m_player.position.y}, {"z", m_player.position.z}}},
            {"hp", m_player.hp},
            {"maxHp", m_player.maxHp},
            {"hpPercent", m_player.hpPercent()},
            {"mp", m_player.mp},
            {"maxMp", m_player.maxMp},
            {"mpPercent", m_player.mpPercent()},
            {"level", m_player.level},
            {"capacity", m_player.capacity},
            {"targetId", m_player.targetId}
        };

        nlohmann::json creaturesJson = nlohmann::json::array();
        for (const auto& c : m_creatures) {
            if (!c.isDead && c.isVisible) {
                creaturesJson.push_back({
                    {"id", c.id},
                    {"name", c.name},
                    {"hpPercent", c.hpPercent},
                    {"distance", c.position.chebyshevDistance(m_player.position)},
                    {"position", {{"x", c.position.x}, {"y", c.position.y}, {"z", c.position.z}}}
                });
            }
        }
        j["creatures"] = creaturesJson;

        return j;
    }

private:
    GameState() = default;

    mutable std::mutex m_mutex;
    Player m_player;
    std::vector<Creature> m_creatures;
    std::vector<Container> m_containers;
    WorldMap m_worldMap;
};

} // namespace bot::game
