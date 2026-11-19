#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include "engine/core/Module.hpp"
#include "engine/game/GameState.hpp"
#include "engine/game/IGameAdapter.hpp"

namespace bot::modules {

struct LootItem {
    uint32_t id{0};
    std::string name;
    uint8_t destinationContainer{0};
};

class LootingModule : public core::Module {
public:
    explicit LootingModule(std::shared_ptr<game::IGameAdapter> adapter)
        : m_adapter(std::move(adapter)) {}

    std::string getName() const override { return "Looting"; }
    std::string getDescription() const override { return "Automatic corpse opening and item collection"; }

    void addLootItem(const LootItem& item) {
        m_lootList.push_back(item);
        m_lootItemIds.insert(item.id);
    }

    void clearLootItems() {
        m_lootList.clear();
        m_lootItemIds.clear();
    }

    void tick(double /*deltaSeconds*/) override {
        if (!m_adapter || !m_adapter->isConnected()) return;

        // Check open containers for loot items and move them
        auto containers = game::GameState::instance().getContainers();
        for (const auto& cont : containers) {
            // Container index 1+ is typically an opened corpse / loot backpack
            if (cont.index > 0) {
                for (size_t slot = 0; slot < cont.items.size(); ++slot) {
                    const auto& item = cont.items[slot];
                    if (m_lootItemIds.count(item.id) > 0) {
                        // Move to main backpack (container 0)
                        m_adapter->moveItem(cont.index, static_cast<uint8_t>(slot), 0, 0, item.count);
                    }
                }
            }
        }
    }

    void loadConfig(const nlohmann::json& config) override {
        if (!config.contains("items") || !config["items"].is_array()) return;
        clearLootItems();
        for (const auto& jItem : config["items"]) {
            LootItem it;
            it.id = jItem.value("id", 0);
            it.name = jItem.value("name", "");
            it.destinationContainer = jItem.value("destContainer", 0);
            addLootItem(it);
        }
    }

    nlohmann::json saveConfig() const override {
        nlohmann::json j;
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& it : m_lootList) {
            arr.push_back({
                {"id", it.id},
                {"name", it.name},
                {"destContainer", it.destinationContainer}
            });
        }
        j["items"] = arr;
        return j;
    }

private:
    std::shared_ptr<game::IGameAdapter> m_adapter;
    std::vector<LootItem> m_lootList;
    std::unordered_set<uint32_t> m_lootItemIds;
};

} // namespace bot::modules
