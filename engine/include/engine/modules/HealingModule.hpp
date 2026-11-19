#pragma once

#include <vector>
#include <string>
#include <chrono>
#include "engine/core/Module.hpp"
#include "engine/game/GameState.hpp"
#include "engine/game/IGameAdapter.hpp"

namespace bot::modules {

enum class TriggerType {
    HpBelow,
    MpBelow,
    StatusPoison,
    StatusParalyze
};

enum class ActionType {
    Spell,
    Item
};

struct HealRule {
    std::string name;
    bool enabled{true};
    TriggerType trigger{TriggerType::HpBelow};
    double thresholdPercent{70.0};
    ActionType action{ActionType::Spell};
    std::string spellWords{"exura"};
    uint32_t itemId{266};
    int32_t minMana{20};
    int64_t cooldownMs{1000};
    int priority{1}; // Lower number = higher priority

    int64_t lastExecutedMs{0};
};

class HealingModule : public core::Module {
public:
    explicit HealingModule(std::shared_ptr<game::IGameAdapter> adapter)
        : m_adapter(std::move(adapter)) {}

    std::string getName() const override { return "Healing"; }
    std::string getDescription() const override { return "Automated HP/MP healing and status curing"; }

    void addRule(const HealRule& rule) {
        m_rules.push_back(rule);
        sortRules();
    }

    void clearRules() {
        m_rules.clear();
    }

    void tick(double /*deltaSeconds*/) override {
        if (!m_adapter || !m_adapter->isConnected()) return;

        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        game::Player player = game::GameState::instance().getPlayer();
        double hpPct = player.hpPercent();
        double mpPct = player.mpPercent();

        for (auto& rule : m_rules) {
            if (!rule.enabled) continue;
            if (nowMs - rule.lastExecutedMs < rule.cooldownMs) continue;
            if (player.mp < rule.minMana) continue;

            bool shouldTrigger = false;
            if (rule.trigger == TriggerType::HpBelow && hpPct <= rule.thresholdPercent) {
                shouldTrigger = true;
            } else if (rule.trigger == TriggerType::MpBelow && mpPct <= rule.thresholdPercent) {
                shouldTrigger = true;
            } else if (rule.trigger == TriggerType::StatusPoison && player.isPoisoned) {
                shouldTrigger = true;
            }

            if (shouldTrigger) {
                rule.lastExecutedMs = nowMs;
                if (rule.action == ActionType::Spell) {
                    m_adapter->castSpell(rule.spellWords);
                } else if (rule.action == ActionType::Item) {
                    m_adapter->useItemOnSelf(rule.itemId);
                }
                break; // Execute the highest priority rule per tick
            }
        }
    }

    void loadConfig(const nlohmann::json& config) override {
        if (!config.contains("rules") || !config["rules"].is_array()) return;
        m_rules.clear();
        for (const auto& jRule : config["rules"]) {
            HealRule r;
            r.name = jRule.value("name", "Heal");
            r.enabled = jRule.value("enabled", true);
            r.thresholdPercent = jRule.value("thresholdPercent", 70.0);
            r.spellWords = jRule.value("spellWords", "exura");
            r.itemId = jRule.value("itemId", 266);
            r.minMana = jRule.value("minMana", 20);
            r.cooldownMs = jRule.value("cooldownMs", 1000);
            r.priority = jRule.value("priority", 1);

            std::string trig = jRule.value("trigger", "hp_below");
            if (trig == "mp_below") r.trigger = TriggerType::MpBelow;
            else if (trig == "poison") r.trigger = TriggerType::StatusPoison;
            else r.trigger = TriggerType::HpBelow;

            std::string act = jRule.value("action", "spell");
            if (act == "item") r.action = ActionType::Item;
            else r.action = ActionType::Spell;

            m_rules.push_back(r);
        }
        sortRules();
    }

    nlohmann::json saveConfig() const override {
        nlohmann::json j;
        nlohmann::json rulesJson = nlohmann::json::array();
        for (const auto& r : m_rules) {
            rulesJson.push_back({
                {"name", r.name},
                {"enabled", r.enabled},
                {"thresholdPercent", r.thresholdPercent},
                {"spellWords", r.spellWords},
                {"itemId", r.itemId},
                {"minMana", r.minMana},
                {"cooldownMs", r.cooldownMs},
                {"priority", r.priority},
                {"trigger", r.trigger == TriggerType::MpBelow ? "mp_below" : (r.trigger == TriggerType::StatusPoison ? "poison" : "hp_below")},
                {"action", r.action == ActionType::Item ? "item" : "spell"}
            });
        }
        j["rules"] = rulesJson;
        return j;
    }

private:
    void sortRules() {
        std::sort(m_rules.begin(), m_rules.end(), [](const HealRule& a, const HealRule& b) {
            return a.priority < b.priority;
        });
    }

    std::shared_ptr<game::IGameAdapter> m_adapter;
    std::vector<HealRule> m_rules;
};

} // namespace bot::modules
