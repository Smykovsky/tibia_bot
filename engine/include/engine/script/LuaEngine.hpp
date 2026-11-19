#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <iostream>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include "engine/core/Logger.hpp"
#include "engine/core/EventManager.hpp"
#include "engine/core/ModuleManager.hpp"
#include "engine/game/GameState.hpp"
#include "engine/game/IGameAdapter.hpp"

namespace bot::script {

class LuaEngine {
public:
    static LuaEngine& instance() {
        static LuaEngine inst;
        return inst;
    }

    bool init(std::shared_ptr<game::IGameAdapter> adapter) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_adapter = adapter;

        if (m_L) {
            lua_close(m_L);
            m_L = nullptr;
        }

        m_L = luaL_newstate();
        if (!m_L) {
            core::Logger::error("Failed to create Lua state");
            return false;
        }

        // Open safe standard libraries
        luaL_requiref(m_L, "_G", luaopen_base, 1);
        lua_pop(m_L, 1);
        luaL_requiref(m_L, "table", luaopen_table, 1);
        lua_pop(m_L, 1);
        luaL_requiref(m_L, "string", luaopen_string, 1);
        lua_pop(m_L, 1);
        luaL_requiref(m_L, "math", luaopen_math, 1);
        lua_pop(m_L, 1);

        registerBindings();
        core::Logger::info("LuaEngine initialized successfully");
        return true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_L) {
            lua_close(m_L);
            m_L = nullptr;
        }
    }

    bool executeString(const std::string& code, std::string* outError = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_L) return false;

        if (luaL_dostring(m_L, code.c_str()) != LUA_OK) {
            std::string err = lua_tostring(m_L, -1);
            lua_pop(m_L, 1);
            core::Logger::error("Lua Execution Error: " + err);
            if (outError) *outError = err;
            return false;
        }
        return true;
    }

    bool executeFile(const std::string& path, std::string* outError = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_L) return false;

        if (luaL_dofile(m_L, path.c_str()) != LUA_OK) {
            std::string err = lua_tostring(m_L, -1);
            lua_pop(m_L, 1);
            core::Logger::error("Lua File Error (" + path + "): " + err);
            if (outError) *outError = err;
            return false;
        }
        return true;
    }

private:
    LuaEngine() = default;
    ~LuaEngine() { shutdown(); }

    void registerBindings() {
        // Register "Game" table
        lua_newtable(m_L);

        lua_pushcfunction(m_L, [](lua_State* L) -> int {
            auto player = game::GameState::instance().getPlayer();
            lua_newtable(L);
            lua_pushinteger(L, player.id); lua_setfield(L, -2, "id");
            lua_pushstring(L, player.name.c_str()); lua_setfield(L, -2, "name");
            lua_pushinteger(L, player.hp); lua_setfield(L, -2, "hp");
            lua_pushinteger(L, player.maxHp); lua_setfield(L, -2, "maxHp");
            lua_pushnumber(L, player.hpPercent()); lua_setfield(L, -2, "hpPercent");
            lua_pushinteger(L, player.mp); lua_setfield(L, -2, "mp");
            lua_pushinteger(L, player.maxMp); lua_setfield(L, -2, "maxMp");
            lua_pushnumber(L, player.mpPercent()); lua_setfield(L, -2, "mpPercent");
            lua_pushinteger(L, player.position.x); lua_setfield(L, -2, "x");
            lua_pushinteger(L, player.position.y); lua_setfield(L, -2, "y");
            lua_pushinteger(L, player.position.z); lua_setfield(L, -2, "z");
            lua_pushinteger(L, player.level); lua_setfield(L, -2, "level");
            lua_pushnumber(L, player.capacity); lua_setfield(L, -2, "cap");
            return 1;
        });
        lua_setfield(m_L, -2, "getPlayer");

        lua_pushcfunction(m_L, [](lua_State* L) -> int {
            int maxDist = static_cast<int>(luaL_optinteger(L, 1, 8));
            auto monsters = game::GameState::instance().getMonstersInRange(maxDist);
            auto player = game::GameState::instance().getPlayer();

            lua_newtable(L);
            for (size_t i = 0; i < monsters.size(); ++i) {
                lua_newtable(L);
                lua_pushinteger(L, monsters[i].id); lua_setfield(L, -2, "id");
                lua_pushstring(L, monsters[i].name.c_str()); lua_setfield(L, -2, "name");
                lua_pushinteger(L, monsters[i].hpPercent); lua_setfield(L, -2, "hpPercent");
                lua_pushinteger(L, monsters[i].position.chebyshevDistance(player.position)); lua_setfield(L, -2, "distance");
                lua_pushinteger(L, monsters[i].position.x); lua_setfield(L, -2, "x");
                lua_pushinteger(L, monsters[i].position.y); lua_setfield(L, -2, "y");
                lua_pushinteger(L, monsters[i].position.z); lua_setfield(L, -2, "z");
                lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
            }
            return 1;
        });
        lua_setfield(m_L, -2, "getMonsters");

        lua_setglobal(m_L, "Game");

        // Register "Bot" table
        lua_newtable(m_L);

        lua_pushlightuserdata(m_L, this);
        lua_pushcclosure(m_L, [](lua_State* L) -> int {
            auto* self = static_cast<LuaEngine*>(lua_touserdata(L, lua_upvalueindex(1)));
            const char* spell = luaL_checkstring(L, 1);
            if (self->m_adapter) {
                bool res = self->m_adapter->castSpell(spell);
                lua_pushboolean(L, res);
                return 1;
            }
            lua_pushboolean(L, false);
            return 1;
        }, 1);
        lua_setfield(m_L, -2, "castSpell");

        lua_pushlightuserdata(m_L, this);
        lua_pushcclosure(m_L, [](lua_State* L) -> int {
            auto* self = static_cast<LuaEngine*>(lua_touserdata(L, lua_upvalueindex(1)));
            uint32_t itemId = static_cast<uint32_t>(luaL_checkinteger(L, 1));
            if (self->m_adapter) {
                bool res = self->m_adapter->useItemOnSelf(itemId);
                lua_pushboolean(L, res);
                return 1;
            }
            lua_pushboolean(L, false);
            return 1;
        }, 1);
        lua_setfield(m_L, -2, "useItem");

        lua_pushlightuserdata(m_L, this);
        lua_pushcclosure(m_L, [](lua_State* L) -> int {
            auto* self = static_cast<LuaEngine*>(lua_touserdata(L, lua_upvalueindex(1)));
            const char* msg = luaL_checkstring(L, 1);
            if (self->m_adapter) {
                bool res = self->m_adapter->say(msg);
                lua_pushboolean(L, res);
                return 1;
            }
            lua_pushboolean(L, false);
            return 1;
        }, 1);
        lua_setfield(m_L, -2, "say");

        lua_pushcfunction(m_L, [](lua_State* L) -> int {
            const char* msg = luaL_checkstring(L, 1);
            core::Logger::info(std::string("[Lua] ") + msg);
            return 0;
        });
        lua_setfield(m_L, -2, "log");

        lua_pushcfunction(m_L, [](lua_State* L) -> int {
            const char* modName = luaL_checkstring(L, 1);
            bool enable = lua_toboolean(L, 2);
            auto mod = core::ModuleManager::instance().getModule(modName);
            if (mod) {
                mod->setEnabled(enable);
                lua_pushboolean(L, true);
            } else {
                lua_pushboolean(L, false);
            }
            return 1;
        });
        lua_setfield(m_L, -2, "setModule");

        lua_setglobal(m_L, "Bot");
    }

    std::mutex m_mutex;
    lua_State* m_L{nullptr};
    std::shared_ptr<game::IGameAdapter> m_adapter;
};

} // namespace bot::script
