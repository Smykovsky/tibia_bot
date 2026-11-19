#pragma once

#include <string>
#include <cstdint>
#include "engine/core/Types.hpp"

namespace bot::game {

class IGameAdapter {
public:
    virtual ~IGameAdapter() = default;

    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual void update() = 0;

    // Client discovery & status
    virtual std::string getAdapterName() const { return "IGameAdapter"; }
    virtual std::string getTargetProcessName() const { return ""; }
    virtual void setTargetProcessName(const std::string& /*name*/) {}
    virtual int64_t getProcessId() const { return 0; }
    virtual std::string getStatusDescription() const { return ""; }

    // Actions
    virtual bool walk(core::Direction dir) = 0;
    virtual bool walkTo(const core::Position& pos) = 0;
    virtual bool castSpell(const std::string& spell) = 0;
    virtual bool useItemOnSelf(uint32_t itemId) = 0;
    virtual bool useItemWith(uint32_t itemId, const core::Position& pos) = 0;
    virtual bool useItemWithCreature(uint32_t itemId, uint32_t creatureId) = 0;
    virtual bool attackCreature(uint32_t creatureId) = 0;
    virtual bool stopAttack() = 0;
    virtual bool openContainer(const core::Position& pos, uint8_t index) = 0;
    virtual bool moveItem(uint8_t fromContainer, uint8_t fromSlot, uint8_t toContainer, uint8_t toSlot, uint32_t count) = 0;
    virtual bool say(const std::string& message) = 0;
};

} // namespace bot::game
