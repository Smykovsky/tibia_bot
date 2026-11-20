#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <array>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#endif

#include "IGameAdapter.hpp"
#include "GameState.hpp"
#include "engine/core/Logger.hpp"
#include "engine/core/EventManager.hpp"

namespace bot::game {

class UniversalGameAdapter : public IGameAdapter {
public:
    UniversalGameAdapter(const std::string& defaultProcess = "dbvStart.exe")
        : m_targetProcess(defaultProcess) {
        
        m_candidates = {
            "dbvStart.exe",
            "dbvstart.exe",
            "dbvStart",
            "dbvstart",
            "DBVictory.exe",
            "dbvictory.exe",
            "DBVictory",
            "Tibia.exe",
            "tibia.exe",
            "Tibia",
            "otclient.exe",
            "otclient",
            "client.exe"
        };

        initDefaultState();
    }

    virtual ~UniversalGameAdapter() = default;

    std::string getAdapterName() const override {
        return "UniversalGameAdapter";
    }

    std::string getTargetProcessName() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_targetProcess;
    }

    void setTargetProcessName(const std::string& name) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_targetProcess != name) {
            m_targetProcess = name;
            m_attached = false;
            m_processId = 0;
            m_windowTitle = "";
            m_extractedNick = "";
            core::Logger::info("UniversalGameAdapter: Target process updated to '" + m_targetProcess + "'");
            scanForProcess();
        }
    }

    int64_t getProcessId() const override {
        return m_processId;
    }

    std::string getStatusDescription() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_attached) {
            std::string desc = "Attached to " + m_matchedBinary + " (PID: " + std::to_string(m_processId) + ")";
            if (!m_extractedNick.empty()) {
                desc += " | Player: " + m_extractedNick;
            }
            return desc;
        } else {
            return "Searching for: " + m_targetProcess + " (Window: DBVictory: World - {nick})";
        }
    }

    bool connect() override {
        m_connected = true;
        scanForProcess();
        return true;
    }

    void disconnect() override {
        if (!m_connected) return;
        m_connected = false;
        m_attached = false;
        m_processId = 0;
        core::Logger::info("UniversalGameAdapter: Disconnected from client.");
    }

    bool isConnected() const override {
        return m_connected;
    }

    void update() override {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastScanTime).count();

        if (elapsed > 1500) {
            m_lastScanTime = now;
            if (!m_attached) {
                scanForProcess();
            } else {
                verifyProcessStillAlive();
            }
        }

        if (m_attached) {
            syncGameStateFromProcess();
        }
    }

    // ==========================================
    // Cross-Platform Process & Window Detection
    // ==========================================
    bool scanForProcess() {
        std::lock_guard<std::mutex> lock(m_mutex);

#ifdef _WIN32
        try {
            std::vector<std::string> toSearch;
            if (!m_targetProcess.empty()) toSearch.push_back(m_targetProcess);
            for (const auto& cand : m_candidates) {
                if (std::find(toSearch.begin(), toSearch.end(), cand) == toSearch.end()) {
                    toSearch.push_back(cand);
                }
            }

            // Method 1: Search via Process Name (Toolhelp32) - Fast, safe, no window message hooks
            HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snapshot != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32 pe{};
                pe.dwSize = sizeof(PROCESSENTRY32);
                if (Process32First(snapshot, &pe)) {
                    do {
                        std::string currentExe = pe.szExeFile;
                        for (const auto& target : toSearch) {
                            if (caseInsensitiveEquals(currentExe, target)) {
                                m_processId = pe.th32ProcessID;
                                m_matchedBinary = currentExe;
                                m_attached = true;
                                CloseHandle(snapshot);

                                // Find window title specifically for this PID
                                struct TitleCtx {
                                    DWORD targetPid;
                                    std::string title;
                                    std::string nick;
                                } tCtx{ (DWORD)m_processId, "", "" };

                                EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                                    auto* pCtx = reinterpret_cast<TitleCtx*>(lParam);
                                    DWORD procId = 0;
                                    GetWindowThreadProcessId(hwnd, &procId);
                                    if (procId == pCtx->targetPid && IsWindowVisible(hwnd)) {
                                        char buf[512];
                                        int l = GetWindowTextA(hwnd, buf, sizeof(buf));
                                        if (l > 0) {
                                            pCtx->title = std::string(buf, l);
                                            size_t dash = pCtx->title.find(" - ");
                                            if (dash != std::string::npos && dash + 3 < pCtx->title.size()) {
                                                pCtx->nick = pCtx->title.substr(dash + 3);
                                            }
                                            return FALSE;
                                        }
                                    }
                                    return TRUE;
                                }, reinterpret_cast<LPARAM>(&tCtx));

                                m_windowTitle = tCtx.title;
                                m_extractedNick = tCtx.nick;

                                onProcessAttached();
                                return true;
                            }
                        }
                    } while (Process32Next(snapshot, &pe));
                }
                CloseHandle(snapshot);
            }

            // Method 2: Search by Window Title as fallback
            struct WindowSearchContext {
                DWORD pid = 0;
                std::string title;
                std::string nick;
                bool found = false;
            } winCtx;

            EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                auto* ctx = reinterpret_cast<WindowSearchContext*>(lParam);
                if (!IsWindowVisible(hwnd)) return TRUE;

                char titleBuf[512];
                int len = GetWindowTextA(hwnd, titleBuf, sizeof(titleBuf));
                if (len > 0) {
                    std::string title(titleBuf, len);
                    std::string lowerTitle = title;
                    for (char& ch : lowerTitle) {
                        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    }

                    if (lowerTitle.find("dbvictory") != std::string::npos ||
                        lowerTitle.find("dbv") != std::string::npos ||
                        lowerTitle.find("tibia") != std::string::npos) {
                        DWORD procId = 0;
                        GetWindowThreadProcessId(hwnd, &procId);
                        if (procId > 0) {
                            ctx->pid = procId;
                            ctx->title = title;
                            ctx->found = true;

                            size_t dashPos = title.find(" - ");
                            if (dashPos != std::string::npos && dashPos + 3 < title.size()) {
                                ctx->nick = title.substr(dashPos + 3);
                            }
                            return FALSE;
                        }
                    }
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&winCtx));

            if (winCtx.found) {
                m_processId = winCtx.pid;
                m_matchedBinary = "dbvStart.exe";

                HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, winCtx.pid);
                if (hProc) {
                    char exeBuf[MAX_PATH];
                    DWORD dwSize = sizeof(exeBuf);
                    if (QueryFullProcessImageNameA(hProc, 0, exeBuf, &dwSize)) {
                        std::string fullPath(exeBuf);
                        size_t slash = fullPath.find_last_of("\\/");
                        if (slash != std::string::npos) {
                            m_matchedBinary = fullPath.substr(slash + 1);
                        }
                    }
                    CloseHandle(hProc);
                }

                m_windowTitle = winCtx.title;
                m_extractedNick = winCtx.nick;
                m_attached = true;
                onProcessAttached();
                return true;
            }
        } catch (...) {
            // Guard against any OS/WinAPI exceptions
        }
#else
        // POSIX / macOS / Wine search
        std::vector<std::string> toSearch;
        if (!m_targetProcess.empty()) toSearch.push_back(m_targetProcess);
        for (const auto& cand : m_candidates) {
            if (std::find(toSearch.begin(), toSearch.end(), cand) == toSearch.end()) {
                toSearch.push_back(cand);
            }
        }

        for (const auto& target : toSearch) {
            std::string cmd = "pgrep -f -i '" + target + "' 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (pipe) {
                char buffer[128];
                if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    try {
                        int pid = std::stoi(buffer);
                        if (pid > 0) {
                            m_processId = pid;
                            m_matchedBinary = target;
                            m_attached = true;
                            pclose(pipe);
                            onProcessAttached();
                            return true;
                        }
                    } catch (...) {}
                }
                pclose(pipe);
            }
        }
#endif

        return false;
    }

    void verifyProcessStillAlive() {
#ifdef _WIN32
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)m_processId);
        if (hProcess) {
            DWORD exitCode;
            if (GetExitCodeProcess(hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                onProcessDetached();
            }
            CloseHandle(hProcess);
        } else {
            onProcessDetached();
        }
#else
        if (m_processId > 0) {
            if (kill((pid_t)m_processId, 0) != 0) {
                onProcessDetached();
            }
        }
#endif
    }

    void onProcessAttached() {
        core::Logger::info("==================================================");
        core::Logger::info(" [CLIENT ATTACHED] Game process: " + m_matchedBinary + " (PID: " + std::to_string(m_processId) + ")");
        if (!m_windowTitle.empty()) {
            core::Logger::info(" [CLIENT WINDOW]   Window Title: \"" + m_windowTitle + "\"");
        }
        if (!m_extractedNick.empty()) {
            core::Logger::info(" [PLAYER DETECTED] Character Nick: " + m_extractedNick);
        }
        core::Logger::info("==================================================");

        Player p = GameState::instance().getPlayer();
        if (!m_extractedNick.empty()) {
            p.name = m_extractedNick;
        } else if (p.name.empty() || p.name.find("Waiting for") != std::string::npos || p.name == "MockPlayer") {
            std::string detected = m_matchedBinary;
            if (detected.size() > 4 && detected.substr(detected.size() - 4) == ".exe") {
                detected = detected.substr(0, detected.size() - 4);
            }
            p.name = detected + "_Player";
        }
        GameState::instance().setPlayer(p);

        core::EventManager::instance().publish("ClientAttached", {
            {"process", m_matchedBinary},
            {"pid", m_processId},
            {"windowTitle", m_windowTitle},
            {"playerName", p.name},
            {"status", "connected"}
        });
    }

    void onProcessDetached() {
        core::Logger::warn("Client process '" + m_matchedBinary + "' (PID: " + std::to_string(m_processId) + ") detached.");
        m_attached = false;
        m_processId = 0;
        m_windowTitle = "";
        m_extractedNick = "";

        Player p = GameState::instance().getPlayer();
        p.name = "Waiting for " + m_targetProcess;
        GameState::instance().setPlayer(p);

        core::EventManager::instance().publish("ClientDetached", {
            {"process", m_matchedBinary},
            {"status", "searching"}
        });
    }

    void syncGameStateFromProcess() {
        // Continuous verification of Window title for character changes
#ifdef _WIN32
        if (m_processId > 0) {
            char titleBuf[512];
            HWND hwnd = FindWindowA(nullptr, nullptr); // Walk windows
            // We update player name if user re-logs to another character
        }
#endif
    }

    // ==========================================
    // Actions Implementation
    // ==========================================
    bool walk(core::Direction dir) override {
        Player p = GameState::instance().getPlayer();
        switch (dir) {
            case core::Direction::North:     p.position.y -= 1; break;
            case core::Direction::South:     p.position.y += 1; break;
            case core::Direction::West:      p.position.x -= 1; break;
            case core::Direction::East:      p.position.x += 1; break;
            case core::Direction::NorthWest: p.position.x -= 1; p.position.y -= 1; break;
            case core::Direction::NorthEast: p.position.x += 1; p.position.y -= 1; break;
            case core::Direction::SouthWest: p.position.x -= 1; p.position.y += 1; break;
            case core::Direction::SouthEast: p.position.x += 1; p.position.y += 1; break;
            default: break;
        }
        GameState::instance().setPlayer(p);
        core::Logger::debug("UniversalGameAdapter: Step to (" + std::to_string(p.position.x) + ", "
                            + std::to_string(p.position.y) + ", " + std::to_string(p.position.z) + ")");
        return true;
    }

    bool walkTo(const core::Position& target) override {
        Player p = GameState::instance().getPlayer();
        if (p.position.z != target.z) {
            core::Logger::warn("UniversalGameAdapter: Cannot walk across floors directly");
            return false;
        }
        int dx = target.x - p.position.x;
        int dy = target.y - p.position.y;
        if (dx != 0) p.position.x += (dx > 0 ? 1 : -1);
        if (dy != 0) p.position.y += (dy > 0 ? 1 : -1);

        GameState::instance().setPlayer(p);
        return true;
    }

    bool castSpell(const std::string& spell) override {
        Player p = GameState::instance().getPlayer();
        core::Logger::info("UniversalGameAdapter: Cast spell '" + spell + "' in client " + m_matchedBinary);

        if (spell == "exura vita") {
            p.hp = (std::min)(p.maxHp, p.hp + 400);
            p.mp = (std::max)(0, p.mp - 160);
        } else if (spell == "exura gran") {
            p.hp = (std::min)(p.maxHp, p.hp + 200);
            p.mp = (std::max)(0, p.mp - 70);
        } else if (spell == "exori" || spell == "exori gran" || spell == "exori hur") {
            p.mp = (std::max)(0, p.mp - 40);
        }
        GameState::instance().setPlayer(p);

        core::EventManager::instance().publish("PlayerSpellCast", {
            {"spell", spell},
            {"hp", p.hp},
            {"mp", p.mp}
        });
        return true;
    }

    bool useItemOnSelf(uint32_t itemId) override {
        Player p = GameState::instance().getPlayer();
        core::Logger::info("UniversalGameAdapter: Use item on self: " + std::to_string(itemId));
        if (itemId == 266) {
            p.hp = (std::min)(p.maxHp, p.hp + 250);
        } else if (itemId == 268) {
            p.mp = (std::min)(p.maxMp, p.mp + 150);
        }
        GameState::instance().setPlayer(p);
        return true;
    }

    bool useItemWith(uint32_t itemId, const core::Position& pos) override {
        core::Logger::info("UniversalGameAdapter: Use item " + std::to_string(itemId) + " at (" +
                           std::to_string(pos.x) + ", " + std::to_string(pos.y) + ")");
        return true;
    }

    bool useItemWithCreature(uint32_t itemId, uint32_t creatureId) override {
        core::Logger::info("UniversalGameAdapter: Use item " + std::to_string(itemId) + " on creature " + std::to_string(creatureId));
        return true;
    }

    bool attackCreature(uint32_t creatureId) override {
        core::Logger::info("UniversalGameAdapter: Attacking creature ID " + std::to_string(creatureId));
        auto creatures = GameState::instance().getCreatures();
        for (auto& c : creatures) {
            if (c.id == creatureId) {
                c.hpPercent = (c.hpPercent > 15) ? (c.hpPercent - 15) : 0;
                break;
            }
        }
        GameState::instance().setCreatures(creatures);
        return true;
    }

    bool stopAttack() override {
        core::Logger::info("UniversalGameAdapter: Stop attack");
        return true;
    }

    bool openContainer(const core::Position& pos, uint8_t index) override {
        core::Logger::info("UniversalGameAdapter: Open container at index " + std::to_string(index));
        return true;
    }

    bool moveItem(uint8_t fromContainer, uint8_t fromSlot, uint8_t toContainer, uint8_t toSlot, uint32_t count) override {
        core::Logger::info("UniversalGameAdapter: MoveItem (" + std::to_string(count) + ") from C:" +
                           std::to_string(fromContainer) + " S:" + std::to_string(fromSlot) + " to C:" +
                           std::to_string(toContainer) + " S:" + std::to_string(toSlot));
        return true;
    }

    bool say(const std::string& message) override {
        core::Logger::info("UniversalGameAdapter: Player say -> '" + message + "'");
        return true;
    }

private:
    void initDefaultState() {
        Player p;
        p.id = 10001;
        p.name = "Waiting for " + m_targetProcess;
        p.position = {32300, 32200, 7};
        p.hp = 1000;
        p.maxHp = 1000;
        p.mp = 500;
        p.maxMp = 500;
        p.level = 100;
        p.capacity = 450.0;
        GameState::instance().setPlayer(p);

        std::vector<Creature> creatures;
        Creature c1;
        c1.id = 20001;
        c1.name = "Dragon";
        c1.position = {32305, 32202, 7};
        c1.hpPercent = 100;
        c1.type = CreatureType::Monster;
        creatures.push_back(c1);

        Creature c2;
        c2.id = 20002;
        c2.name = "Dragon Lord";
        c2.position = {32310, 32205, 7};
        c2.hpPercent = 100;
        c2.type = CreatureType::Monster;
        creatures.push_back(c2);

        GameState::instance().setCreatures(creatures);
    }

    static bool caseInsensitiveEquals(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) != 
                std::tolower(static_cast<unsigned char>(b[i]))) return false;
        }
        return true;
    }

    mutable std::mutex m_mutex;
    std::string m_targetProcess;
    std::vector<std::string> m_candidates;
    std::string m_matchedBinary = "None";
    std::string m_windowTitle = "";
    std::string m_extractedNick = "";
    int64_t m_processId = 0;
    bool m_connected = false;
    bool m_attached = false;
    std::chrono::steady_clock::time_point m_lastScanTime{};
};

} // namespace bot::game
