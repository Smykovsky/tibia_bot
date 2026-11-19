#pragma once

#include <string>
#include <cstdint>
#include "engine/core/Types.hpp"

namespace bot::game {

struct Item {
    uint32_t id{0};
    uint32_t count{1};
    std::string name;
    uint8_t containerIndex{0}; // if in container
    uint8_t slotIndex{0};
    core::Position groundPos{0, 0, 0};
    bool isGround{false};
};

struct Container {
    uint8_t index{0};
    uint32_t containerItemId{0};
    std::string name;
    uint8_t capacity{0};
    std::vector<Item> items;

    bool isFull() const {
        return items.size() >= capacity;
    }

    size_t getFreeSlots() const {
        return capacity > items.size() ? capacity - items.size() : 0;
    }
};

} // namespace bot::game
