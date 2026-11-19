#pragma once

#include <iostream>
#include <vector>
#include <mutex>
#include "IGameAdapter.hpp"
#include "GameState.hpp"
#include "engine/core/Logger.hpp"
#include "engine/core/EventManager.hpp"

namespace bot::game {

class MockGameAdapter : public IGameAdapter {
public:
    MockGameAdapter() {
        // Setup initial default mock player
        Player p;
        p.id = 10001;
        p.name = "MockPlayer";
        p.position = {32300, 32200, 7};
        p.hp = 1000;
        p.maxHp = 1000;
        p.mp = 500;
        p.maxMp = 500;
        p.level = 100;
        p.capacity = 450.0;
        GameState::instance().setPlayer(p);

        // Setup a mock container with potions and gold
        Container bp;
        bp.index = 0;
        bp.capacity = 20;
        bp.name = "Backpack";
        bp.items.push_back({266, 50, "Health Potion", 0, 0, {}, false});
        bp.items.push_back({268, 80, "Mana Potion", 0, 1, {}, false});
        std::vector<Container> containers = {bp};
        GameState::instance().setContainers(containers);

        // Setup some monsters
        spawnMonster(20001, "Dragon", {32305, 32202, 7}, 100);
        spawnMonster(20002, "Dragon Lord", {32310, 32205, 7}, 100);
    }

    bool connect() override {
        m_connected = true;
        core::Logger::info("MockGameAdapter connected to simulated game environment.");
        return true;
    }

    void disconnect() override {
        m_connected = false;
        core::Logger::info("MockGameAdapter disconnected.");
    }

    bool isConnected() const override {
        return m_connected;
    }

    void update() override {
        // Here we can simulate game progression (e.g., slight mana regeneration, monsters moving, etc.)
    }

    bool walk(core::Direction dir) override {
        Player p = GameState::instance().getPlayer();
        switch (dir) {
            case core::Direction::North:     p.position.y -= 1; break;
            case core::Direction::South:     p.position.y += 1; break;
            case core::Direction::West:      p.position.x -= 1; break;
            case core::Direction::East:      p.position.x += 1; break;
            case core::Direction::NorthWest: p.position.x -= 1; p.position.y -= 1; break;
            case core::Direction::NorthEast: p.position.x += 1; p.position.y -= 1; break;
            case core::Direction::SouthWest: p.position.x -= 1; p.position.y += 1; break;
            case core::Direction::SouthEast: p.position.x += 1; p.position.y += 1; break;
            default: break;
        }
        GameState::instance().setPlayer(p);
        core::Logger::debug("MockGameAdapter: Walked to (" + std::to_string(p.position.x) + ", "
                            + std::to_string(p.position.y) + ", " + std::to_string(p.position.z) + ")");
        return true;
    }

    bool walkTo(const core::Position& target) override {
        Player p = GameState::instance().getPlayer();
        if (p.position.z != target.z) {
            core::Logger::warn("MockGameAdapter: Cannot walk across floors directly");
            return false;
        }

        // Move 1 step towards target
        int dx = target.x - p.position.x;
        int dy = target.y - p.position.y;
        if (dx != 0) p.position.x += (dx > 0 ? 1 : -1);
        if (dy != 0) p.position.y += (dy > 0 ? 1 : -1);

        GameState::instance().setPlayer(p);
        core::Logger::debug("MockGameAdapter: WalkTo step -> (" + std::to_string(p.position.x) + ", "
                            + std::to_string(p.position.y) + ", " + std::to_string(p.position.z) + ")");
        return true;
    }

    bool castSpell(const std::string& spell) override {
        core::Logger::info("MockGameAdapter: Cast spell: \"" + spell + "\"");
        Player p = GameState::instance().getPlayer();
        if (spell == "exura" || spell == "exura gran" || spell == "exura vita") {
            int healAmount = (spell == "exura vita") ? 600 : ((spell == "exura gran") ? 250 : 100);
            p.hp = std::min(p.maxHp, p.hp + healAmount);
            p.mp = std::max(0, p.mp - 40);
            GameState::instance().setPlayer(p);
            core::EventManager::instance().publish("PlayerHealed", {{"spell", spell}, {"hp", p.hp}});
        }
        return true;
    }

    bool useItemOnSelf(uint32_t itemId) override {
        core::Logger::info("MockGameAdapter: Used item on self: " + std::to_string(itemId));
        Player p = GameState::instance().getPlayer();
        if (itemId == 266) { // Health Potion
            p.hp = std::min(p.maxHp, p.hp + 200);
            GameState::instance().setPlayer(p);
        } else if (itemId == 268) { // Mana Potion
            p.mp = std::min(p.maxMp, p.mp + 150);
            GameState::instance().setPlayer(p);
        }
        return true;
    }

    bool useItemWith(uint32_t itemId, const core::Position& pos) override {
        core::Logger::info("MockGameAdapter: Used item " + std::to_string(itemId) + " at ("
                            + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ")");
        return true;
    }

    bool useItemWithCreature(uint32_t itemId, uint32_t creatureId) override {
        core::Logger::info("MockGameAdapter: Used item " + std::to_string(itemId) + " on creature " + std::to_string(creatureId));
        return true;
    }

    bool attackCreature(uint32_t creatureId) override {
        core::Logger::info("MockGameAdapter: Attacking creature ID " + std::to_string(creatureId));
        Player p = GameState::instance().getPlayer();
        p.targetId = creatureId;
        GameState::instance().setPlayer(p);
        return true;
    }

    bool stopAttack() override {
        core::Logger::info("MockGameAdapter: Stopped attack");
        Player p = GameState::instance().getPlayer();
        p.targetId = 0;
        GameState::instance().setPlayer(p);
        return true;
    }

    bool openContainer(const core::Position& pos, uint8_t index) override {
        core::Logger::info("MockGameAdapter: Opened container at position (" + std::to_string(pos.x)
                            + ", " + std::to_string(pos.y) + ") into index " + std::to_string(index));
        return true;
    }

    bool moveItem(uint8_t fromContainer, uint8_t fromSlot, uint8_t toContainer, uint8_t toSlot, uint32_t count) override {
        core::Logger::info("MockGameAdapter: Moved item from [" + std::to_string(fromContainer) + ":" + std::to_string(fromSlot)
                            + "] to [" + std::to_string(toContainer) + ":" + std::to_string(toSlot) + "] count=" + std::to_string(count));
        return true;
    }

    bool say(const std::string& message) override {
        core::Logger::info("MockGameAdapter: Say: \"" + message + "\"");
        return true;
    }

    // Testing helpers
    void spawnMonster(uint32_t id, const std::string& name, const core::Position& pos, uint8_t hpPercent = 100) {
        std::vector<Creature> creatures = GameState::instance().getCreatures();
        Creature c;
        c.id = id;
        c.name = name;
        c.type = CreatureType::Monster;
        c.position = pos;
        c.hpPercent = hpPercent;
        c.isVisible = true;
        c.isDead = false;
        creatures.push_back(c);
        GameState::instance().setCreatures(creatures);
    }

    void simulatePlayerDamage(int damage) {
        Player p = GameState::instance().getPlayer();
        p.hp = std::max(0, p.hp - damage);
        GameState::instance().setPlayer(p);
        core::EventManager::instance().publish("PlayerDamageTaken", {{"damage", damage}, {"currentHp", p.hp}});
    }

private:
    bool m_connected{false};
};

} // namespace bot::game
