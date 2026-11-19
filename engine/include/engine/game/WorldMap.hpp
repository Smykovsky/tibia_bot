#pragma once

#include <unordered_map>
#include <vector>
#include "engine/core/Types.hpp"
#include "Item.hpp"

namespace bot::game {

struct Tile {
    core::Position position;
    bool walkable{true};
    int moveCost{100}; // Standard ground cost
    bool isRopeSpot{false};
    bool isLadder{false};
    bool isSewerGrate{false};
    bool isHole{false};
    bool isProtectionZone{false};
    std::vector<Item> items;
};

// Custom hash for core::Position
struct PositionHash {
    std::size_t operator()(const core::Position& pos) const {
        std::size_t h1 = std::hash<int32_t>{}(pos.x);
        std::size_t h2 = std::hash<int32_t>{}(pos.y);
        std::size_t h3 = std::hash<int32_t>{}(pos.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class WorldMap {
public:
    void setTile(const core::Position& pos, const Tile& tile) {
        m_tiles[pos] = tile;
    }

    bool isWalkable(const core::Position& pos) const {
        auto it = m_tiles.find(pos);
        if (it != m_tiles.end()) {
            return it->second.walkable;
        }
        return true; // Default assumption if unseen or open ground
    }

    const Tile* getTile(const core::Position& pos) const {
        auto it = m_tiles.find(pos);
        if (it != m_tiles.end()) {
            return &(it->second);
        }
        return nullptr;
    }

    void clear() {
        m_tiles.clear();
    }

private:
    std::unordered_map<core::Position, Tile, PositionHash> m_tiles;
};

} // namespace bot::game
