#pragma once

#include <string>
#include <cstdint>
#include "engine/core/Types.hpp"

namespace bot::game {

enum class CreatureType : uint8_t {
    Player,
    Monster,
    Npc
};

struct Creature {
    uint32_t id{0};
    std::string name;
    CreatureType type{CreatureType::Monster};
    core::Position position{0, 0, 0};
    core::Direction direction{core::Direction::South};

    uint8_t hpPercent{100};
    bool isVisible{true};
    bool isDead{false};
    bool isAttacking{false};

    double distanceTo(const core::Position& pos) const {
        return position.distanceTo(pos);
    }
};

} // namespace bot::game
