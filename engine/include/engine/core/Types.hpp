#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <cstdint>
#include <string>
#include <vector>
#include <cmath>

namespace bot::core {

struct Position {
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;

    bool operator==(const Position& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Position& other) const {
        return !(*this == other);
    }

    double distanceTo(const Position& other) const {
        if (z != other.z) return 999999.0;
        int dx = x - other.x;
        int dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    int chebyshevDistance(const Position& other) const {
        if (z != other.z) return 999999;
        return std::max(std::abs(x - other.x), std::abs(y - other.y));
    }

    int manhattanDistance(const Position& other) const {
        if (z != other.z) return 999999;
        return std::abs(x - other.x) + std::abs(y - other.y);
    }
};

enum class Direction : uint8_t {
    North = 0,
    East = 1,
    South = 2,
    West = 3,
    NorthEast = 4,
    SouthEast = 5,
    SouthWest = 6,
    NorthWest = 7,
    None = 8
};

enum class EngineState {
    Stopped,
    Starting,
    Running,
    Paused,
    Stopping,
    Error
};

} // namespace bot::core
