#pragma once

#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include "Types.hpp"
#include "Logger.hpp"
#include "ModuleManager.hpp"
#include "EventManager.hpp"
#include "ConfigManager.hpp"
#include "engine/game/IGameAdapter.hpp"

namespace bot::core {

class Engine {
public:
    static Engine& instance() {
        static Engine inst;
        return inst;
    }

    bool initialize(std::shared_ptr<game::IGameAdapter> adapter, int tickRateMs = 50) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_state != EngineState::Stopped) {
            Logger::warn("Engine already initialized or running");
            return false;
        }

        m_gameAdapter = adapter;
        m_tickRateMs = tickRateMs;
        m_state = EngineState::Starting;

        Logger::info("Initializing Engine with tick rate " + std::to_string(m_tickRateMs) + "ms...");

        if (m_gameAdapter && !m_gameAdapter->connect()) {
            Logger::error("Failed to connect game adapter");
            m_state = EngineState::Error;
            return false;
        }

        if (!ModuleManager::instance().initAll()) {
            Logger::error("Failed to initialize all modules");
            m_state = EngineState::Error;
            return false;
        }

        m_state = EngineState::Running;
        m_running = true;
        m_workerThread = std::make_unique<std::thread>(&Engine::runLoop, this);

        Logger::info("Engine started successfully");
        EventManager::instance().publish("EngineStarted", {{"state", "running"}});
        return true;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (!m_running) return;
            Logger::info("Stopping Engine...");
            m_state = EngineState::Stopping;
            m_running = false;
        }

        if (m_workerThread && m_workerThread->joinable()) {
            m_workerThread->join();
            m_workerThread.reset();
        }

        ModuleManager::instance().stopAll();

        if (m_gameAdapter) {
            m_gameAdapter->disconnect();
        }

        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_state = EngineState::Stopped;
        }

        Logger::info("Engine stopped cleanly");
        EventManager::instance().publish("EngineStopped", {{"state", "stopped"}});
    }

    void pause() {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_state == EngineState::Running) {
            m_state = EngineState::Paused;
            Logger::info("Engine paused");
            EventManager::instance().publish("EnginePaused", {});
        }
    }

    void resume() {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_state == EngineState::Paused) {
            m_state = EngineState::Running;
            Logger::info("Engine resumed");
            EventManager::instance().publish("EngineResumed", {});
        }
    }

    EngineState getState() const {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        return m_state;
    }

    std::string getStateString() const {
        switch (getState()) {
            case EngineState::Stopped:  return "stopped";
            case EngineState::Starting: return "starting";
            case EngineState::Running:  return "running";
            case EngineState::Paused:   return "paused";
            case EngineState::Stopping: return "stopping";
            case EngineState::Error:    return "error";
        }
        return "unknown";
    }

    std::shared_ptr<game::IGameAdapter> getGameAdapter() const {
        return m_gameAdapter;
    }

    int getTickRateMs() const { return m_tickRateMs; }

private:
    Engine() = default;
    ~Engine() { stop(); }

    void runLoop() {
        auto lastTime = std::chrono::steady_clock::now();

        while (m_running) {
            auto currentTime = std::chrono::steady_clock::now();
            double deltaSeconds = std::chrono::duration<double>(currentTime - lastTime).count();
            lastTime = currentTime;

            if (getState() == EngineState::Running) {
                try {
                    // 1. Update Game State from Adapter
                    if (m_gameAdapter && m_gameAdapter->isConnected()) {
                        m_gameAdapter->update();
                    }

                    // 2. Tick all active modules
                    ModuleManager::instance().tickAll(deltaSeconds);

                } catch (const std::exception& e) {
                    Logger::error("Unhandled exception in main engine loop: " + std::string(e.what()));
                }
            }

            // Precise sleep to maintain target tick rate
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - currentTime).count();
            auto sleepTime = m_tickRateMs - elapsed;
            if (sleepTime > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
            }
        }
    }

    mutable std::mutex m_stateMutex;
    EngineState m_state{EngineState::Stopped};
    std::atomic<bool> m_running{false};
    int m_tickRateMs{50};
    std::unique_ptr<std::thread> m_workerThread;
    std::shared_ptr<game::IGameAdapter> m_gameAdapter;
};

} // namespace bot::core
