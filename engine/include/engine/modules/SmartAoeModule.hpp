#pragma once

#include <vector>
#include <string>
#include <chrono>
#include "engine/core/Module.hpp"
#include "engine/game/GameState.hpp"
#include "engine/game/IGameAdapter.hpp"

namespace bot::modules {

struct AoeSpellRule {
    std::string spellWords{"exori"};
    int minMonsters{3};
    int maxDistance{1}; // 1 = surrounding tiles (exori), 3-4 = GFB/Avalanche
    int minMana{115};
    int64_t cooldownMs{2000};
    int64_t lastExecutedMs{0};
};

class SmartAoeModule : public core::Module {
public:
    explicit SmartAoeModule(std::shared_ptr<game::IGameAdapter> adapter)
        : m_adapter(std::move(adapter)) {}

    std::string getName() const override { return "SmartAOE"; }
    std::string getDescription() const override { return "Area of Effect spell evaluation and casting"; }

    void addRule(const AoeSpellRule& rule) {
        m_rules.push_back(rule);
    }

    void clearRules() {
        m_rules.clear();
    }

    void tick(double /*deltaSeconds*/) override {
        if (!m_adapter || !m_adapter->isConnected()) return;

        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        game::Player player = game::GameState::instance().getPlayer();
        auto monsters = game::GameState::instance().getMonstersInRange(6);

        for (auto& rule : m_rules) {
            if (nowMs - rule.lastExecutedMs < rule.cooldownMs) continue;
            if (player.mp < rule.minMana) continue;

            int countInRange = 0;
            for (const auto& m : monsters) {
                if (m.position.chebyshevDistance(player.position) <= rule.maxDistance) {
                    countInRange++;
                }
            }

            if (countInRange >= rule.minMonsters) {
                rule.lastExecutedMs = nowMs;
                m_adapter->castSpell(rule.spellWords);
                break;
            }
        }
    }

    void loadConfig(const nlohmann::json& config) override {
        if (!config.contains("rules") || !config["rules"].is_array()) return;
        m_rules.clear();
        for (const auto& jRule : config["rules"]) {
            AoeSpellRule r;
            r.spellWords = jRule.value("spell", "exori");
            r.minMonsters = jRule.value("minMonsters", 3);
            r.maxDistance = jRule.value("maxDistance", 1);
            r.minMana = jRule.value("minMana", 115);
            r.cooldownMs = jRule.value("cooldownMs", 2000);
            m_rules.push_back(r);
        }
    }

    nlohmann::json saveConfig() const override {
        nlohmann::json j;
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& r : m_rules) {
            arr.push_back({
                {"spell", r.spellWords},
                {"minMonsters", r.minMonsters},
                {"maxDistance", r.maxDistance},
                {"minMana", r.minMana},
                {"cooldownMs", r.cooldownMs}
            });
        }
        j["rules"] = arr;
        return j;
    }

private:
    std::shared_ptr<game::IGameAdapter> m_adapter;
    std::vector<AoeSpellRule> m_rules;
};

} // namespace bot::modules
