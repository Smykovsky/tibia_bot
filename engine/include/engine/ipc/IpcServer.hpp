#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
#define IS_VALID_SOCKET(s) ((s) != INVALID_SOCKET)
#define CLOSE_SOCKET(s) closesocket(s)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
using socket_t = int;
#define IS_VALID_SOCKET(s) ((s) >= 0)
#define CLOSE_SOCKET(s) close(s)
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

#include "Protocol.hpp"
#include "engine/core/Logger.hpp"
#include "engine/core/Engine.hpp"
#include "engine/core/EventManager.hpp"
#include "engine/core/ModuleManager.hpp"
#include "engine/core/ConfigManager.hpp"
#include "engine/game/GameState.hpp"
#include "engine/script/LuaEngine.hpp"

namespace bot::ipc {

class IpcServer {
public:
    static IpcServer& instance() {
        static IpcServer inst;
        return inst;
    }

    bool start(int port = 8765) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_running) return true;

#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            core::Logger::error("Failed to initialize Winsock");
            return false;
        }
#endif

        m_port = port;
        m_serverFd = socket(AF_INET, SOCK_STREAM, 0);
        if (!IS_VALID_SOCKET(m_serverFd)) {
            core::Logger::error("Failed to create IPC socket");
            return false;
        }

        int opt = 1;
#ifdef _WIN32
        setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = inet_addr("127.0.0.1");
        address.sin_port = htons(port);

        if (bind(m_serverFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            core::Logger::error("Failed to bind IPC socket to port " + std::to_string(port));
            CLOSE_SOCKET(m_serverFd);
            m_serverFd = INVALID_SOCKET;
            return false;
        }

        if (listen(m_serverFd, 5) < 0) {
            core::Logger::error("Failed to listen on IPC socket");
            CLOSE_SOCKET(m_serverFd);
            m_serverFd = INVALID_SOCKET;
            return false;
        }

        m_running = true;
        m_acceptThread = std::make_unique<std::thread>(&IpcServer::acceptLoop, this);

        // Hook logger to broadcast logs to Launcher
        core::Logger::instance().addCallback([this](core::LogLevel level, const std::string& msg, const std::string& ts) {
            std::string lvlStr = "info";
            if (level == core::LogLevel::Debug) lvlStr = "debug";
            else if (level == core::LogLevel::Warning) lvlStr = "warn";
            else if (level == core::LogLevel::Error) lvlStr = "error";

            nlohmann::json notif = makeNotification("onLog", {
                {"level", lvlStr},
                {"message", msg},
                {"timestamp", ts}
            });
            broadcast(notif.dump() + "\n");
        });

        // Hook event manager to broadcast events to Launcher
        core::EventManager::instance().subscribe("*", [this](const core::Event& ev) {
            nlohmann::json notif = makeNotification("onEvent", {
                {"type", ev.type},
                {"data", ev.data},
                {"timestamp", ev.timestamp}
            });
            broadcast(notif.dump() + "\n");
        });

        core::Logger::info("IPC Server listening on 127.0.0.1:" + std::to_string(port));
        return true;
    }

    void stop() {
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            if (!m_running) return;
            m_running = false;
            if (IS_VALID_SOCKET(m_serverFd)) {
                CLOSE_SOCKET(m_serverFd);
                m_serverFd = INVALID_SOCKET;
            }
            for (socket_t clientFd : m_clients) {
                CLOSE_SOCKET(clientFd);
            }
            m_clients.clear();
#ifdef _WIN32
            WSACleanup();
#endif
        }

        if (m_acceptThread && m_acceptThread->joinable()) {
            m_acceptThread->join();
            m_acceptThread.reset();
        }

        core::Logger::info("IPC Server stopped");
    }

    void broadcast(const std::string& message) {
        std::vector<socket_t> currentClients;
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            currentClients = m_clients;
        }

        std::vector<socket_t> deadClients;
        for (socket_t fd : currentClients) {
            int sent = send(fd, message.c_str(), static_cast<int>(message.size()), 0);
            if (sent < 0) {
                deadClients.push_back(fd);
            }
        }

        if (!deadClients.empty()) {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            for (int deadFd : deadClients) {
                close(deadFd);
                m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), deadFd), m_clients.end());
            }
        }
    }

    nlohmann::json handleRequest(const nlohmann::json& req) {
        auto id = req.value("id", nlohmann::json(nullptr));
        std::string method = req.value("method", "");
        nlohmann::json params = req.value("params", nlohmann::json::object());

        if (method == "ping") {
            return makeSuccessResponse(id, "pong");
        } else if (method == "getEngineState") {
            return makeSuccessResponse(id, {
                {"state", core::Engine::instance().getStateString()},
                {"tickRateMs", core::Engine::instance().getTickRateMs()}
            });
        } else if (method == "startEngine") {
            // Can resume if paused
            if (core::Engine::instance().getState() == core::EngineState::Paused) {
                core::Engine::instance().resume();
            }
            return makeSuccessResponse(id, {{"status", "ok"}});
        } else if (method == "pauseEngine") {
            core::Engine::instance().pause();
            return makeSuccessResponse(id, {{"status", "ok"}});
        } else if (method == "resumeEngine") {
            core::Engine::instance().resume();
            return makeSuccessResponse(id, {{"status", "ok"}});
        } else if (method == "stopEngine") {
            core::Engine::instance().stop();
            return makeSuccessResponse(id, {{"status", "ok"}});
        } else if (method == "getGameState") {
            return makeSuccessResponse(id, game::GameState::instance().toJson());
        } else if (method == "getModules") {
            return makeSuccessResponse(id, core::ModuleManager::instance().getStatusJson());
        } else if (method == "setModuleState") {
            std::string modName = params.value("name", "");
            bool enabled = params.value("enabled", false);
            auto mod = core::ModuleManager::instance().getModule(modName);
            if (mod) {
                mod->setEnabled(enabled);
                return makeSuccessResponse(id, {{"name", modName}, {"enabled", enabled}});
            }
            return makeErrorResponse(id, -32602, "Module not found: " + modName);
        } else if (method == "getModuleConfig") {
            std::string modName = params.value("name", "");
            auto mod = core::ModuleManager::instance().getModule(modName);
            if (mod) {
                return makeSuccessResponse(id, mod->saveConfig());
            }
            return makeErrorResponse(id, -32602, "Module not found: " + modName);
        } else if (method == "setModuleConfig") {
            std::string modName = params.value("name", "");
            nlohmann::json cfg = params.value("config", nlohmann::json::object());
            auto mod = core::ModuleManager::instance().getModule(modName);
            if (mod) {
                mod->loadConfig(cfg);
                core::ConfigManager::instance().setModuleConfig(modName, cfg);
                return makeSuccessResponse(id, {{"status", "ok"}});
            }
            return makeErrorResponse(id, -32602, "Module not found: " + modName);
        } else if (method == "loadProfile") {
            std::string profile = params.value("profile", "default");
            bool ok = core::ConfigManager::instance().loadProfile(profile);
            if (ok) {
                core::ModuleManager::instance().applyConfig(core::ConfigManager::instance().getAllConfig());
            }
            return makeSuccessResponse(id, {{"profile", profile}, {"success", ok}});
        } else if (method == "saveProfile") {
            std::string profile = params.value("profile", "default");
            core::ConfigManager::instance().setAllConfig(core::ModuleManager::instance().exportConfig());
            bool ok = core::ConfigManager::instance().saveProfile(profile);
            return makeSuccessResponse(id, {{"profile", profile}, {"success", ok}});
        } else if (method == "executeLua") {
            std::string code = params.value("code", "");
            std::string err;
            bool ok = script::LuaEngine::instance().executeString(code, &err);
            if (ok) {
                return makeSuccessResponse(id, {{"success", true}});
            } else {
                return makeErrorResponse(id, -32000, err);
            }
        } else if (method == "getClientStatus") {
            auto adapter = core::Engine::instance().getGameAdapter();
            if (adapter) {
                return makeSuccessResponse(id, {
                    {"adapterName", adapter->getAdapterName()},
                    {"targetProcess", adapter->getTargetProcessName()},
                    {"pid", adapter->getProcessId()},
                    {"attached", adapter->getProcessId() > 0},
                    {"status", adapter->getStatusDescription()}
                });
            }
            return makeSuccessResponse(id, {{"status", "no_adapter"}});
        } else if (method == "setTargetClient") {
            std::string proc = params.value("processName", "DBVictory.exe");
            auto adapter = core::Engine::instance().getGameAdapter();
            if (adapter) {
                adapter->setTargetProcessName(proc);
                return makeSuccessResponse(id, {{"targetProcess", proc}, {"status", "updated"}});
            }
            return makeErrorResponse(id, -32000, "Adapter not available");
        }

        return makeErrorResponse(id, -32601, "Method not found: " + method);
    }

private:
    IpcServer() = default;
    ~IpcServer() { stop(); }

    void acceptLoop() {
        while (m_running) {
            sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            socket_t clientFd = accept(m_serverFd, (struct sockaddr*)&clientAddr, &clientLen);
            if (!IS_VALID_SOCKET(clientFd)) {
                if (!m_running) break;
                continue;
            }

            core::Logger::info("New IPC client connected from " + std::string(inet_ntoa(clientAddr.sin_addr)));

            {
                std::lock_guard<std::recursive_mutex> lock(m_mutex);
                m_clients.push_back(clientFd);
            }

            // Spawn client handler thread
            std::thread(&IpcServer::clientHandler, this, clientFd).detach();
        }
    }

    void clientHandler(socket_t clientFd) {
        std::string buffer;
        char temp[4096];

        while (m_running) {
            int bytesRead = recv(clientFd, temp, sizeof(temp) - 1, 0);
            if (bytesRead <= 0) break;

            temp[bytesRead] = '\0';
            buffer.append(temp, bytesRead);

            size_t newlinePos;
            while ((newlinePos = buffer.find('\n')) != std::string::npos) {
                std::string line = buffer.substr(0, newlinePos);
                buffer.erase(0, newlinePos + 1);

                if (line.empty() || line == "\r") continue;

                try {
                    nlohmann::json req = nlohmann::json::parse(line);
                    nlohmann::json resp = handleRequest(req);
                    std::string respStr = resp.dump() + "\n";
                    send(clientFd, respStr.c_str(), static_cast<int>(respStr.size()), 0);
                } catch (const std::exception& e) {
                    nlohmann::json err = makeErrorResponse(nullptr, -32700, std::string("Parse error: ") + e.what());
                    std::string errStr = err.dump() + "\n";
                    send(clientFd, errStr.c_str(), static_cast<int>(errStr.size()), 0);
                }
            }
        }

        CLOSE_SOCKET(clientFd);
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), clientFd), m_clients.end());
        }
        core::Logger::info("IPC client disconnected");
    }

    mutable std::recursive_mutex m_mutex;
    std::atomic<bool> m_running{false};
    socket_t m_serverFd{INVALID_SOCKET};
    int m_port{8765};
    std::unique_ptr<std::thread> m_acceptThread;
    std::vector<socket_t> m_clients;
};

} // namespace bot::ipc
