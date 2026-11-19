#include <iostream>
#include <csignal>
#include <atomic>
#include <condition_variable>
#include "engine/core/Logger.hpp"
#include "engine/core/Engine.hpp"
#include "engine/core/ModuleManager.hpp"
#include "engine/core/ConfigManager.hpp"
#include "engine/game/UniversalGameAdapter.hpp"
#include "engine/game/MockGameAdapter.hpp"
#include "engine/modules/HealingModule.hpp"
#include "engine/modules/CavebotModule.hpp"
#include "engine/modules/TargetingModule.hpp"
#include "engine/modules/LootingModule.hpp"
#include "engine/modules/SmartAoeModule.hpp"
#include "engine/script/LuaEngine.hpp"
#include "engine/script/ScriptManager.hpp"
#include "engine/ipc/IpcServer.hpp"

namespace {
std::atomic<bool> g_shutdownRequested{false};
std::mutex g_cvMutex;
std::condition_variable g_cv;

void signalHandler(int signum) {
    bot::core::Logger::info("Received signal " + std::to_string(signum) + ", initiating graceful shutdown...");
    g_shutdownRequested = true;
    g_cv.notify_all();
}
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    int port = 8765;
    std::string targetClient = "DBVictory.exe";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--client" && i + 1 < argc) {
            targetClient = argv[++i];
        }
    }

    bot::core::Logger::info("=========================================");
    bot::core::Logger::info("   TibiaBot C++ Engine (Shrouded Model)  ");
    bot::core::Logger::info("   Target Client: " + targetClient);
    bot::core::Logger::info("=========================================");

    // 1. Setup Config
    bot::core::ConfigManager::instance().setConfigDir("configs");

    // 2. Setup Universal Game Adapter (DBVictory.exe / Tibia.exe / etc.)
    auto gameAdapter = std::make_shared<bot::game::UniversalGameAdapter>(targetClient);

    // 3. Register Modules
    auto healing = std::make_shared<bot::modules::HealingModule>(gameAdapter);
    auto cavebot = std::make_shared<bot::modules::CavebotModule>(gameAdapter);
    auto targeting = std::make_shared<bot::modules::TargetingModule>(gameAdapter);
    auto looting = std::make_shared<bot::modules::LootingModule>(gameAdapter);
    auto smartAoe = std::make_shared<bot::modules::SmartAoeModule>(gameAdapter);

    // Setup some sensible default rules
    bot::modules::HealRule rExuraVita;
    rExuraVita.name = "Emergency Vita";
    rExuraVita.thresholdPercent = 50.0;
    rExuraVita.spellWords = "exura vita";
    rExuraVita.minMana = 160;
    rExuraVita.priority = 1;
    healing->addRule(rExuraVita);

    bot::modules::HealRule rExuraGran;
    rExuraGran.name = "Medium Gran";
    rExuraGran.thresholdPercent = 75.0;
    rExuraGran.spellWords = "exura gran";
    rExuraGran.minMana = 70;
    rExuraGran.priority = 2;
    healing->addRule(rExuraGran);

    bot::modules::HealRule rPotion;
    rPotion.name = "Health Potion";
    rPotion.thresholdPercent = 35.0;
    rPotion.action = bot::modules::ActionType::Item;
    rPotion.itemId = 266;
    rPotion.priority = 0; // Highest priority emergency
    healing->addRule(rPotion);

    bot::core::ModuleManager::instance().registerModule(healing);
    bot::core::ModuleManager::instance().registerModule(cavebot);
    bot::core::ModuleManager::instance().registerModule(targeting);
    bot::core::ModuleManager::instance().registerModule(looting);
    bot::core::ModuleManager::instance().registerModule(smartAoe);

    // 4. Initialize Lua Engine & Scripting
    bot::script::LuaEngine::instance().init(gameAdapter);
    bot::script::ScriptManager::instance().setScriptsDir("scripts");
    bot::script::ScriptManager::instance().scanScripts();

    // 5. Initialize Engine Core (50ms tick rate)
    if (!bot::core::Engine::instance().initialize(gameAdapter, 50)) {
        bot::core::Logger::error("Failed to start Engine");
        return 1;
    }

    // 6. Start IPC Server for Launcher communication
    if (!bot::ipc::IpcServer::instance().start(port)) {
        bot::core::Logger::error("Failed to start IPC Server");
        bot::core::Engine::instance().stop();
        return 1;
    }

    bot::core::Logger::info("TibiaBot Engine is up and running on port " + std::to_string(port) + ".");

    // Wait until shutdown signal
    {
        std::unique_lock<std::mutex> lock(g_cvMutex);
        g_cv.wait(lock, [] { return g_shutdownRequested.load(); });
    }

    bot::core::Logger::info("Shutting down TibiaBot Engine...");
    bot::ipc::IpcServer::instance().stop();
    bot::core::Engine::instance().stop();
    bot::script::LuaEngine::instance().shutdown();

    bot::core::Logger::info("Engine terminated successfully.");
    return 0;
}
