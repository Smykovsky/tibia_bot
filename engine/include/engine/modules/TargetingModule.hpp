#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include "engine/core/Module.hpp"
#include "engine/game/GameState.hpp"
#include "engine/game/IGameAdapter.hpp"

namespace bot::modules {

enum class Stance {
    Chase,
    KeepDistance,
    Stand
};

struct MonsterSetting {
    std::string name{"*"}; // "*" matches any monster
    int priority{1};       // 1 = highest
    Stance stance{Stance::Chase};
    int desiredDistance{3};
    std::vector<std::string> attackSpells;
};

class TargetingModule : public core::Module {
public:
    explicit TargetingModule(std::shared_ptr<game::IGameAdapter> adapter)
        : m_adapter(std::move(adapter)) {}

    std::string getName() const override { return "Targeting"; }
    std::string getDescription() const override { return "Automated monster targeting, distance keeping, and combat"; }

    void addSetting(const MonsterSetting& setting) {
        m_settings.push_back(setting);
    }

    void clearSettings() {
        m_settings.clear();
    }

    void tick(double /*deltaSeconds*/) override {
        if (!m_adapter || !m_adapter->isConnected()) return;

        auto monsters = game::GameState::instance().getMonstersInRange(8);
        game::Player player = game::GameState::instance().getPlayer();

        if (monsters.empty()) {
            if (player.targetId != 0) {
                m_adapter->stopAttack();
            }
            return;
        }

        // Find best target based on rules
        const game::Creature* bestTarget = nullptr;
        int bestScore = -999999;
        const MonsterSetting* matchedSetting = nullptr;

        for (const auto& monster : monsters) {
            const MonsterSetting* setting = findSettingForMonster(monster.name);
            int priority = setting ? setting->priority : 10;
            int dist = monster.position.chebyshevDistance(player.position);

            // Score formula: higher priority first, closer monsters second, lower hp third
            int score = (100 - priority * 10) * 100 - dist * 10 + (100 - monster.hpPercent);

            if (score > bestScore) {
                bestScore = score;
                bestTarget = &monster;
                matchedSetting = setting;
            }
        }

        if (bestTarget) {
            // If not attacking this target yet, attack it
            if (player.targetId != bestTarget->id) {
                m_adapter->attackCreature(bestTarget->id);
            }

            // Handle stance
            if (matchedSetting && matchedSetting->stance == Stance::KeepDistance) {
                int curDist = bestTarget->position.chebyshevDistance(player.position);
                int desired = matchedSetting->desiredDistance;
                if (curDist < desired) {
                    // Step away from monster
                    core::Position runAwayPos = player.position;
                    if (player.position.x > bestTarget->position.x) runAwayPos.x += 1;
                    else if (player.position.x < bestTarget->position.x) runAwayPos.x -= 1;
                    if (player.position.y > bestTarget->position.y) runAwayPos.y += 1;
                    else if (player.position.y < bestTarget->position.y) runAwayPos.y -= 1;
                    m_adapter->walkTo(runAwayPos);
                } else if (curDist > desired) {
                    m_adapter->walkTo(bestTarget->position);
                }
            } else if (!matchedSetting || matchedSetting->stance == Stance::Chase) {
                if (bestTarget->position.chebyshevDistance(player.position) > 1) {
                    m_adapter->walkTo(bestTarget->position);
                }
            }

            // Handle attack spells with cooldown
            auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();

            if (matchedSetting && !matchedSetting->attackSpells.empty()) {
                if (nowMs - m_lastSpellMs >= 2000) { // 2s attack cooldown
                    m_lastSpellMs = nowMs;
                    m_adapter->castSpell(matchedSetting->attackSpells.front());
                }
            }
        }
    }

    void loadConfig(const nlohmann::json& config) override {
        if (!config.contains("targets") || !config["targets"].is_array()) return;
        m_settings.clear();
        for (const auto& jTarget : config["targets"]) {
            MonsterSetting s;
            s.name = jTarget.value("name", "*");
            s.priority = jTarget.value("priority", 1);
            std::string st = jTarget.value("stance", "chase");
            if (st == "keep_distance") s.stance = Stance::KeepDistance;
            else if (st == "stand") s.stance = Stance::Stand;
            else s.stance = Stance::Chase;
            s.desiredDistance = jTarget.value("desiredDistance", 3);
            if (jTarget.contains("spells") && jTarget["spells"].is_array()) {
                for (const auto& sp : jTarget["spells"]) {
                    s.attackSpells.push_back(sp.get<std::string>());
                }
            }
            m_settings.push_back(s);
        }
    }

    nlohmann::json saveConfig() const override {
        nlohmann::json j;
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& s : m_settings) {
            std::string st = "chase";
            if (s.stance == Stance::KeepDistance) st = "keep_distance";
            else if (s.stance == Stance::Stand) st = "stand";

            arr.push_back({
                {"name", s.name},
                {"priority", s.priority},
                {"stance", st},
                {"desiredDistance", s.desiredDistance},
                {"spells", s.attackSpells}
            });
        }
        j["targets"] = arr;
        return j;
    }

private:
    const MonsterSetting* findSettingForMonster(const std::string& monsterName) const {
        const MonsterSetting* wildcard = nullptr;
        for (const auto& s : m_settings) {
            if (s.name == monsterName) return &s;
            if (s.name == "*") wildcard = &s;
        }
        return wildcard;
    }

    std::shared_ptr<game::IGameAdapter> m_adapter;
    std::vector<MonsterSetting> m_settings;
    int64_t m_lastSpellMs{0};
};

} // namespace bot::modules
