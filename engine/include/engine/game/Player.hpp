#pragma once

#include <string>
#include <cstdint>
#include <unordered_map>
#include "engine/core/Types.hpp"
#include "Item.hpp"

namespace bot::game {

enum class Slot : uint8_t {
    Head = 1,
    Necklace = 2,
    Backpack = 3,
    Armor = 4,
    RightHand = 5,
    LeftHand = 6,
    Legs = 7,
    Feet = 8,
    Ring = 9,
    Ammo = 10
};

struct Player {
    uint32_t id{0};
    std::string name{"Hero"};
    core::Position position{0, 0, 0};
    core::Direction direction{core::Direction::South};

    int32_t hp{100};
    int32_t maxHp{100};
    int32_t mp{100};
    int32_t maxMp{100};

    uint32_t level{1};
    uint64_t experience{0};
    double capacity{100.0};
    double maxCapacity{100.0};
    uint32_t soul{100};
    uint32_t staminaMinutes{2520}; // 42 hours

    uint32_t targetId{0}; // Creature ID currently attacked
    bool inPz{false};
    bool isPoisoned{false};
    bool isBurning{false};
    bool isElectrified{false};

    std::unordered_map<uint8_t, Item> equipment;

    double hpPercent() const {
        if (maxHp <= 0) return 0.0;
        return (static_cast<double>(hp) / maxHp) * 100.0;
    }

    double mpPercent() const {
        if (maxMp <= 0) return 0.0;
        return (static_cast<double>(mp) / maxMp) * 100.0;
    }
};

} // namespace bot::game
