#include <cassert>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include "engine/core/Logger.hpp"
#include "engine/core/Types.hpp"
#include "engine/core/EventManager.hpp"
#include "engine/core/ModuleManager.hpp"
#include "engine/core/ConfigManager.hpp"
#include "engine/core/Engine.hpp"
#include "engine/game/GameState.hpp"
#include "engine/game/MockGameAdapter.hpp"
#include "engine/modules/HealingModule.hpp"
#include "engine/modules/CavebotModule.hpp"
#include "engine/modules/TargetingModule.hpp"
#include "engine/modules/SmartAoeModule.hpp"
#include "engine/script/LuaEngine.hpp"
#include "engine/ipc/IpcServer.hpp"

using namespace bot;

void testEventManager() {
    std::cout << "[TEST] Running EventManager test..." << std::endl;
    bool receivedSpecific = false;
    bool receivedWildcard = false;

    core::EventManager::instance().subscribe("TestEvent", [&](const core::Event& ev) {
        if (ev.data["val"] == 42) {
            receivedSpecific = true;
        }
    });

    core::EventManager::instance().subscribe("*", [&](const core::Event& ev) {
        if (ev.type == "TestEvent") {
            receivedWildcard = true;
        }
    });

    core::EventManager::instance().publish("TestEvent", {{"val", 42}});

    assert(receivedSpecific && "Specific event subscription failed");
    assert(receivedWildcard && "Wildcard event subscription failed");
    std::cout << "[TEST] EventManager passed!" << std::endl;
}

void testHealingModule() {
    std::cout << "[TEST] Running HealingModule test..." << std::endl;
    auto adapter = std::make_shared<game::MockGameAdapter>();
    adapter->connect();
    auto healing = std::make_shared<modules::HealingModule>(adapter);

    modules::HealRule vita;
    vita.name = "Vita";
    vita.thresholdPercent = 50.0;
    vita.spellWords = "exura vita";
    vita.minMana = 40;
    vita.priority = 1;
    healing->addRule(vita);
    healing->setEnabled(true);

    // Simulate player having 1000/1000 HP, then take 600 damage -> 400 HP (40%)
    adapter->simulatePlayerDamage(600);
    assert(game::GameState::instance().getPlayer().hpPercent() == 40.0);

    // Tick healing module
    healing->tick(0.05);

    // Exura vita heals 600 in mock adapter -> 400 + 600 = 1000 HP
    assert(game::GameState::instance().getPlayer().hp == 1000);
    std::cout << "[TEST] HealingModule passed!" << std::endl;
}

void testCavebotModule() {
    std::cout << "[TEST] Running CavebotModule test..." << std::endl;
    auto adapter = std::make_shared<game::MockGameAdapter>();
    adapter->connect();
    auto cavebot = std::make_shared<modules::CavebotModule>(adapter);

    // Set player position
    game::Player p = game::GameState::instance().getPlayer();
    p.position = {100, 100, 7};
    game::GameState::instance().setPlayer(p);

    modules::Waypoint wp1{modules::WaypointType::Walk, {101, 100, 7}, "wp1", "", 0};
    modules::Waypoint wp2{modules::WaypointType::Walk, {102, 100, 7}, "wp2", "", 0};
    cavebot->addWaypoint(wp1);
    cavebot->addWaypoint(wp2);
    cavebot->setEnabled(true);

    // First tick moves player towards wp1
    cavebot->tick(0.05);
    assert(game::GameState::instance().getPlayer().position.x == 101);

    // Next tick recognizes wp1 reached and moves index to wp2
    cavebot->tick(0.05);
    assert(cavebot->getCurrentIndex() == 1);

    std::cout << "[TEST] CavebotModule passed!" << std::endl;
}

void testTargetingModule() {
    std::cout << "[TEST] Running TargetingModule test..." << std::endl;
    auto adapter = std::make_shared<game::MockGameAdapter>();
    adapter->connect();
    auto targeting = std::make_shared<modules::TargetingModule>(adapter);

    modules::MonsterSetting ms;
    ms.name = "Dragon";
    ms.priority = 1;
    ms.stance = modules::Stance::Chase;
    targeting->addSetting(ms);
    targeting->setEnabled(true);

    // Clear any previous creatures and spawn one right next to player (at 32300, 32200, 7)
    game::GameState::instance().setCreatures({});
    adapter->spawnMonster(555, "Dragon", {32301, 32200, 7}, 100);

    // Tick targeting
    targeting->tick(0.05);

    // Player should now be attacking creature 555
    assert(game::GameState::instance().getPlayer().targetId == 555);
    std::cout << "[TEST] TargetingModule passed!" << std::endl;
}

void testLuaEngine() {
    std::cout << "[TEST] Running LuaEngine test..." << std::endl;
    auto adapter = std::make_shared<game::MockGameAdapter>();
    adapter->connect();
    script::LuaEngine::instance().init(adapter);

    std::string script = R"(
        local player = Game.getPlayer()
        Bot.log("Player name: " .. player.name .. ", HP: " .. player.hp)
        if player.hp > 0 then
            Bot.castSpell("exura gran")
        end
    )";

    std::string err;
    bool ok = script::LuaEngine::instance().executeString(script, &err);
    assert(ok && "Lua execution failed");
    std::cout << "[TEST] LuaEngine passed!" << std::endl;
}

void testIpcServerProtocol() {
    std::cout << "[TEST] Running IPC Protocol test..." << std::endl;
    nlohmann::json pingReq = {
        {"jsonrpc", "2.0"},
        {"method", "ping"},
        {"id", 1}
    };

    auto resp = ipc::IpcServer::instance().handleRequest(pingReq);
    assert(resp["result"] == "pong");
    assert(resp["id"] == 1);

    nlohmann::json stateReq = {
        {"jsonrpc", "2.0"},
        {"method", "getGameState"},
        {"id", 2}
    };
    auto stateResp = ipc::IpcServer::instance().handleRequest(stateReq);
    assert(stateResp["result"].contains("player"));
    assert(stateResp["id"] == 2);

    std::cout << "[TEST] IPC Protocol passed!" << std::endl;
}

int main() {
    core::Logger::info("Starting TibiaBot C++ Test Suite...");

    testEventManager();
    testHealingModule();
    testCavebotModule();
    testTargetingModule();
    testLuaEngine();
    testIpcServerProtocol();

    core::Logger::info("All TibiaBot Engine tests passed successfully!");
    std::cout << "\n>>> ALL TESTS PASSED (100%) <<<\n" << std::endl;
    return 0;
}
