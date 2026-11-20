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
#include <cmath>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
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

    virtual ~UniversalGameAdapter() {
#ifdef _WIN32
        if (m_hProcess && m_hProcess != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hProcess);
            m_hProcess = NULL;
        }
#endif
    }

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
#ifdef _WIN32
            if (m_hProcess && m_hProcess != INVALID_HANDLE_VALUE) {
                CloseHandle(m_hProcess);
                m_hProcess = NULL;
            }
            m_hwnd = NULL;
            m_mem = {};
#endif
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
#ifdef _WIN32
        if (m_hProcess && m_hProcess != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hProcess);
            m_hProcess = NULL;
        }
        m_hwnd = NULL;
        m_mem = {};
#endif
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

            // Method 1: Search via Process Name (Toolhelp32)
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
                                    HWND targetHwnd;
                                    std::string title;
                                    std::string nick;
                                } tCtx{ (DWORD)m_processId, NULL, "", "" };

                                EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                                    auto* pCtx = reinterpret_cast<TitleCtx*>(lParam);
                                    DWORD procId = 0;
                                    GetWindowThreadProcessId(hwnd, &procId);
                                    if (procId == pCtx->targetPid && IsWindowVisible(hwnd)) {
                                        char buf[512];
                                        int l = GetWindowTextA(hwnd, buf, sizeof(buf));
                                        if (l > 0) {
                                            pCtx->targetHwnd = hwnd;
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

                                m_hwnd = tCtx.targetHwnd;
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
                HWND hwnd = NULL;
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
                            ctx->hwnd = hwnd;
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
                m_hwnd = winCtx.hwnd;
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

#ifdef _WIN32
        if (m_hProcess && m_hProcess != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hProcess);
            m_hProcess = NULL;
        }
        m_hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, static_cast<DWORD>(m_processId));
        if (m_hProcess) {
            core::Logger::info("UniversalGameAdapter: Memory handle opened successfully for PID " + std::to_string(m_processId));
        } else {
            core::Logger::warn("UniversalGameAdapter: Could not open process memory (Error: " + std::to_string(GetLastError()) + "). Run as Administrator if needed.");
        }
        m_mem = {};
        scanMemoryForPlayer();
#endif

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
#ifdef _WIN32
        if (m_hProcess && m_hProcess != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hProcess);
            m_hProcess = NULL;
        }
        m_hwnd = NULL;
        m_mem = {};
#endif

        Player p = GameState::instance().getPlayer();
        p.name = "Waiting for " + m_targetProcess;
        GameState::instance().setPlayer(p);

        core::EventManager::instance().publish("ClientDetached", {
            {"process", m_matchedBinary},
            {"status", "searching"}
        });
    }

#ifdef _WIN32
    void scanMemoryForPlayer() {
        if (!m_hProcess || m_hProcess == INVALID_HANDLE_VALUE) return;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastMemScanTime).count();
        if (elapsed < 2000 && m_mem.scanAttempts > 0) return;
        m_lastMemScanTime = now;
        m_mem.scanAttempts++;

        std::string nick = m_extractedNick;
        if (nick.empty()) return;

        uintptr_t currentAddr = 0;
        MEMORY_BASIC_INFORMATION mbi;
        std::vector<uint8_t> buffer;

        core::Logger::info("UniversalGameAdapter: Scanning process memory for player '" + nick + "'...");

        while (VirtualQueryEx(m_hProcess, reinterpret_cast<LPCVOID>(currentAddr), &mbi, sizeof(mbi))) {
            if (mbi.State == MEM_COMMIT && 
                (mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_EXECUTE_READWRITE) &&
                !(mbi.Protect & PAGE_GUARD)) {

                size_t regionSize = mbi.RegionSize;
                if (regionSize > 4 * 1024 * 1024) regionSize = 4 * 1024 * 1024;

                buffer.resize(regionSize);
                SIZE_T bytesRead = 0;
                if (ReadProcessMemory(m_hProcess, mbi.BaseAddress, buffer.data(), regionSize, &bytesRead) && bytesRead > nick.size()) {
                    
                    const uint8_t* pBuf = buffer.data();
                    size_t limit = bytesRead - nick.size();

                    for (size_t i = 0; i < limit; ++i) {
                        if (std::memcmp(pBuf + i, nick.data(), nick.size()) == 0) {
                            uintptr_t matchAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + i;

                            size_t windowStart = (i >= 512) ? (i - 512) : 0;
                            size_t windowEnd = (i + 512 + sizeof(uint16_t) * 2 + 1 <= bytesRead) ? (i + 512) : (bytesRead - 5);

                            uintptr_t foundPosAddr = 0;
                            bool isUint16 = true;

                            for (size_t j = windowStart; j < windowEnd; ++j) {
                                uint16_t x = *reinterpret_cast<const uint16_t*>(pBuf + j);
                                uint16_t y = *reinterpret_cast<const uint16_t*>(pBuf + j + 2);
                                uint8_t z = *(pBuf + j + 4);

                                if (x >= 500 && x <= 65000 && y >= 500 && y <= 65000 && z <= 15) {
                                    foundPosAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + j;
                                    isUint16 = true;
                                    break;
                                }

                                if (j + 12 <= bytesRead) {
                                    int32_t x32 = *reinterpret_cast<const int32_t*>(pBuf + j);
                                    int32_t y32 = *reinterpret_cast<const int32_t*>(pBuf + j + 4);
                                    int32_t z32 = *reinterpret_cast<const int32_t*>(pBuf + j + 8);
                                    if (x32 >= 500 && x32 <= 65000 && y32 >= 500 && y32 <= 65000 && z32 >= 0 && z32 <= 15) {
                                        foundPosAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + j;
                                        isUint16 = false;
                                        break;
                                    }
                                }
                            }

                            if (foundPosAddr != 0) {
                                m_mem.posAddr = foundPosAddr;
                                m_mem.posIsUint16 = isUint16;

                                size_t statStart = i;
                                size_t statEnd = (i + 1024 <= bytesRead) ? (i + 1024) : bytesRead;

                                for (size_t k = statStart; k + 16 <= statEnd; k += 4) {
                                    double d1 = *reinterpret_cast<const double*>(pBuf + k);
                                    double d2 = *reinterpret_cast<const double*>(pBuf + k + 8);
                                    if (!m_mem.hpAddr && d1 > 0.0 && d1 <= d2 && d2 < 100000000.0 && d2 >= 50.0) {
                                        m_mem.hpAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + k;
                                        m_mem.maxHpAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + k + 8;
                                        m_mem.hpIsDouble = true;
                                    }

                                    if (m_mem.hpAddr && !m_mem.mpAddr && k >= (m_mem.hpAddr - reinterpret_cast<uintptr_t>(mbi.BaseAddress) + 16)) {
                                        if (d1 >= 0.0 && d1 <= d2 && d2 < 100000000.0 && d2 >= 10.0) {
                                            m_mem.mpAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + k;
                                            m_mem.maxMpAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + k + 8;
                                            m_mem.mpIsDouble = true;
                                        }
                                    }

                                    if (!m_mem.hpAddr) {
                                        uint32_t u1 = *reinterpret_cast<const uint32_t*>(pBuf + k);
                                        uint32_t u2 = *reinterpret_cast<const uint32_t*>(pBuf + k + 4);
                                        if (u1 > 0 && u1 <= u2 && u2 < 100000000 && u2 >= 50) {
                                            m_mem.hpAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + k;
                                            m_mem.maxHpAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + k + 4;
                                            m_mem.hpIsDouble = false;
                                        }
                                    }

                                    if (!m_mem.lvlAddr) {
                                        int32_t lvl = *reinterpret_cast<const int32_t*>(pBuf + k);
                                        if (lvl >= 1 && lvl <= 5000) {
                                            m_mem.lvlAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + k;
                                            m_mem.lvlIsDouble = false;
                                        }
                                    }

                                    if (!m_mem.capAddr && d1 >= 0.0 && d1 <= 500000.0) {
                                        m_mem.capAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + k;
                                        m_mem.capIsDouble = true;
                                    }
                                }

                                m_mem.resolved = true;
                                core::Logger::info("==================================================");
                                core::Logger::info(" [MEMORY RESOLVED] Player entity hooked in RAM!");
                                core::Logger::info("  - Position Addr: 0x" + toHex(m_mem.posAddr) + " (" + (m_mem.posIsUint16 ? "16-bit" : "32-bit") + ")");
                                if (m_mem.hpAddr) core::Logger::info("  - HP Addr:       0x" + toHex(m_mem.hpAddr) + " (" + (m_mem.hpIsDouble ? "double" : "uint32") + ")");
                                if (m_mem.mpAddr) core::Logger::info("  - MP Addr:       0x" + toHex(m_mem.mpAddr) + " (" + (m_mem.mpIsDouble ? "double" : "uint32") + ")");
                                if (m_mem.lvlAddr) core::Logger::info("  - Level Addr:    0x" + toHex(m_mem.lvlAddr));
                                if (m_mem.capAddr) core::Logger::info("  - Cap Addr:      0x" + toHex(m_mem.capAddr));
                                core::Logger::info("==================================================");
                                return;
                            }
                        }
                    }
                }
            }
            currentAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        }
    }

    void scanMemoryForCreatures(const core::Position& playerPos) {
        if (!m_hProcess || m_hProcess == INVALID_HANDLE_VALUE) return;
        if (!m_mem.resolved || !m_mem.posAddr) return;

        uintptr_t scanBase = (m_mem.posAddr > 0x100000) ? (m_mem.posAddr - 0x100000) : 0;
        size_t scanSize = 0x200000;

        std::vector<uint8_t> buf(scanSize);
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(scanBase), buf.data(), scanSize, &bytesRead) || bytesRead < 64) {
            return;
        }

        std::vector<Creature> creatures;
        uint32_t generatedId = 20000;

        for (size_t i = 0; i + 8 <= bytesRead; i += 2) {
            uint16_t x = *reinterpret_cast<const uint16_t*>(buf.data() + i);
            uint16_t y = *reinterpret_cast<const uint16_t*>(buf.data() + i + 2);
            uint8_t z = *(buf.data() + i + 4);

            if (z == playerPos.z && 
                std::abs(static_cast<int>(x) - static_cast<int>(playerPos.x)) <= 8 &&
                std::abs(static_cast<int>(y) - static_cast<int>(playerPos.y)) <= 8 &&
                (x != playerPos.x || y != playerPos.y)) {

                std::string creatureName = "";
                size_t nStart = (i >= 128) ? (i - 128) : 0;
                size_t nEnd = (i + 128 <= bytesRead) ? (i + 128) : bytesRead;

                for (size_t j = nStart; j + 3 < nEnd; ++j) {
                    if (std::isalpha(buf[j])) {
                        size_t len = 0;
                        while (j + len < nEnd && (std::isalnum(buf[j + len]) || buf[j + len] == ' ' || buf[j + len] == '-')) {
                            len++;
                        }
                        if (len >= 3 && len <= 24) {
                            std::string candidate(reinterpret_cast<const char*>(buf.data() + j), len);
                            if (candidate != m_extractedNick && candidate.find("DBVictory") == std::string::npos && candidate.find("World") == std::string::npos) {
                                creatureName = candidate;
                                break;
                            }
                        }
                    }
                }

                if (!creatureName.empty()) {
                    bool alreadyExists = false;
                    for (const auto& ec : creatures) {
                        if (ec.position.x == x && ec.position.y == y && ec.position.z == z) {
                            alreadyExists = true;
                            break;
                        }
                    }
                    if (!alreadyExists && creatures.size() < 20) {
                        Creature c;
                        c.id = ++generatedId;
                        c.name = creatureName;
                        c.position = {x, y, z};
                        c.hpPercent = 100;
                        c.type = CreatureType::Monster;
                        creatures.push_back(c);
                    }
                }
            }
        }

        if (!creatures.empty()) {
            GameState::instance().setCreatures(creatures);
        }
    }
#endif

    void syncGameStateFromProcess() {
#ifdef _WIN32
        if (!m_attached || m_processId == 0) return;

        if (!m_hProcess || m_hProcess == INVALID_HANDLE_VALUE) {
            m_hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, static_cast<DWORD>(m_processId));
            if (!m_hProcess) return;
        }

        if (m_hwnd && IsWindow(m_hwnd)) {
            char titleBuf[512];
            int len = GetWindowTextA(m_hwnd, titleBuf, sizeof(titleBuf));
            if (len > 0) {
                std::string curTitle(titleBuf, len);
                if (curTitle != m_windowTitle) {
                    m_windowTitle = curTitle;
                    size_t dash = curTitle.find(" - ");
                    if (dash != std::string::npos && dash + 3 < curTitle.size()) {
                        std::string newNick = curTitle.substr(dash + 3);
                        if (newNick != m_extractedNick) {
                            m_extractedNick = newNick;
                            m_mem = {};
                            core::Logger::info("UniversalGameAdapter: Character switched to '" + m_extractedNick + "'");
                        }
                    }
                }
            }
        }

        if (!m_mem.resolved) {
            scanMemoryForPlayer();
            return;
        }

        Player p = GameState::instance().getPlayer();
        bool changed = false;

        // Read Position
        if (m_mem.posAddr) {
            if (m_mem.posIsUint16) {
                struct { uint16_t x; uint16_t y; uint8_t z; } rawPos{};
                if (ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.posAddr), &rawPos, sizeof(rawPos), nullptr)) {
                    if (rawPos.x >= 100 && rawPos.x <= 65000 && rawPos.y >= 100 && rawPos.y <= 65000 && rawPos.z <= 15) {
                        if (p.position.x != rawPos.x || p.position.y != rawPos.y || p.position.z != rawPos.z) {
                            p.position.x = rawPos.x;
                            p.position.y = rawPos.y;
                            p.position.z = rawPos.z;
                            changed = true;
                        }
                    } else {
                        m_mem.resolved = false;
                        return;
                    }
                }
            } else {
                struct { int32_t x; int32_t y; int32_t z; } rawPos{};
                if (ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.posAddr), &rawPos, sizeof(rawPos), nullptr)) {
                    if (rawPos.x >= 100 && rawPos.x <= 65000 && rawPos.y >= 100 && rawPos.y <= 65000 && rawPos.z >= 0 && rawPos.z <= 15) {
                        if (p.position.x != rawPos.x || p.position.y != rawPos.y || p.position.z != rawPos.z) {
                            p.position.x = rawPos.x;
                            p.position.y = rawPos.y;
                            p.position.z = rawPos.z;
                            changed = true;
                        }
                    } else {
                        m_mem.resolved = false;
                        return;
                    }
                }
            }
        }

        // Read HP
        if (m_mem.hpAddr) {
            if (m_mem.hpIsDouble) {
                double curHp = 0, maxHp = 0;
                ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.hpAddr), &curHp, sizeof(double), nullptr);
                if (m_mem.maxHpAddr) {
                    ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.maxHpAddr), &maxHp, sizeof(double), nullptr);
                }
                if (curHp > 0.0 && curHp <= 100000000.0) {
                    p.hp = static_cast<int32_t>(curHp);
                    if (maxHp > 0.0) p.maxHp = static_cast<int32_t>(maxHp);
                    changed = true;
                }
            } else {
                uint32_t curHp = 0, maxHp = 0;
                ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.hpAddr), &curHp, sizeof(uint32_t), nullptr);
                if (m_mem.maxHpAddr) {
                    ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.maxHpAddr), &maxHp, sizeof(uint32_t), nullptr);
                }
                if (curHp > 0 && curHp <= 100000000) {
                    p.hp = static_cast<int32_t>(curHp);
                    if (maxHp > 0) p.maxHp = static_cast<int32_t>(maxHp);
                    changed = true;
                }
            }
        }

        // Read Mana
        if (m_mem.mpAddr) {
            if (m_mem.mpIsDouble) {
                double curMp = 0, maxMp = 0;
                ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.mpAddr), &curMp, sizeof(double), nullptr);
                if (m_mem.maxMpAddr) {
                    ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.maxMpAddr), &maxMp, sizeof(double), nullptr);
                }
                if (curMp >= 0.0 && curMp <= 100000000.0) {
                    p.mp = static_cast<int32_t>(curMp);
                    if (maxMp > 0.0) p.maxMp = static_cast<int32_t>(maxMp);
                    changed = true;
                }
            } else {
                uint32_t curMp = 0, maxMp = 0;
                ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.mpAddr), &curMp, sizeof(uint32_t), nullptr);
                if (m_mem.maxMpAddr) {
                    ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.maxMpAddr), &maxMp, sizeof(uint32_t), nullptr);
                }
                if (curMp <= 100000000) {
                    p.mp = static_cast<int32_t>(curMp);
                    if (maxMp > 0) p.maxMp = static_cast<int32_t>(maxMp);
                    changed = true;
                }
            }
        }

        // Read Level
        if (m_mem.lvlAddr) {
            if (m_mem.lvlIsDouble) {
                double lvl = 0;
                ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.lvlAddr), &lvl, sizeof(double), nullptr);
                if (lvl >= 1.0 && lvl <= 10000.0) {
                    p.level = static_cast<int32_t>(lvl);
                    changed = true;
                }
            } else {
                int32_t lvl = 0;
                ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.lvlAddr), &lvl, sizeof(int32_t), nullptr);
                if (lvl >= 1 && lvl <= 10000) {
                    p.level = lvl;
                    changed = true;
                }
            }
        }

        // Read Capacity
        if (m_mem.capAddr) {
            if (m_mem.capIsDouble) {
                double cap = 0;
                ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.capAddr), &cap, sizeof(double), nullptr);
                if (cap >= 0.0 && cap <= 10000000.0) {
                    p.capacity = cap;
                    changed = true;
                }
            } else {
                uint32_t cap = 0;
                ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.capAddr), &cap, sizeof(uint32_t), nullptr);
                if (cap <= 10000000) {
                    p.capacity = static_cast<double>(cap);
                    changed = true;
                }
            }
        }

        if (!m_extractedNick.empty() && p.name != m_extractedNick) {
            p.name = m_extractedNick;
            changed = true;
        }

        if (changed) {
            GameState::instance().setPlayer(p);
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastCreatureScanTime).count() > 2500) {
            m_lastCreatureScanTime = now;
            scanMemoryForCreatures(p.position);
        }
#endif
    }

    // ==========================================
    // Actions Implementation
    // ==========================================
#ifdef _WIN32
    void sendKey(WORD vk) {
        if (!m_hwnd || !IsWindow(m_hwnd)) return;
        UINT scanCode = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
        LPARAM lParamDown = 1 | (scanCode << 16);
        LPARAM lParamUp = 1 | (scanCode << 16) | (1 << 30) | (1 << 31);
        PostMessageA(m_hwnd, WM_KEYDOWN, vk, lParamDown);
        PostMessageA(m_hwnd, WM_KEYUP, vk, lParamUp);
    }

    void sendText(const std::string& text) {
        if (!m_hwnd || !IsWindow(m_hwnd)) return;
        for (char c : text) {
            PostMessageA(m_hwnd, WM_CHAR, static_cast<WPARAM>(static_cast<unsigned char>(c)), 1);
        }
        sendKey(VK_RETURN);
    }
#endif

    bool walk(core::Direction dir) override {
#ifdef _WIN32
        if (m_hwnd && IsWindow(m_hwnd)) {
            WORD vk = 0;
            switch (dir) {
                case core::Direction::North:     vk = VK_UP; break;
                case core::Direction::South:     vk = VK_DOWN; break;
                case core::Direction::West:      vk = VK_LEFT; break;
                case core::Direction::East:      vk = VK_RIGHT; break;
                case core::Direction::NorthWest: vk = VK_NUMPAD7; break;
                case core::Direction::NorthEast: vk = VK_NUMPAD9; break;
                case core::Direction::SouthWest: vk = VK_NUMPAD1; break;
                case core::Direction::SouthEast: vk = VK_NUMPAD3; break;
                default: break;
            }
            if (vk != 0) {
                sendKey(vk);
            }
        }
#endif
        return true;
    }

    bool walkTo(const core::Position& target) override {
        auto now = std::chrono::steady_clock::now();
        auto diffMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastWalkTime).count();
        if (diffMs < 180) {
            return true;
        }
        m_lastWalkTime = now;

        Player p = GameState::instance().getPlayer();
        if (p.position.z != target.z) {
            core::Logger::warn("UniversalGameAdapter: Cannot walk across floors directly");
            return false;
        }

        int dx = target.x - p.position.x;
        int dy = target.y - p.position.y;
        if (dx == 0 && dy == 0) return true;

        core::Direction dir;
        if (dx > 0 && dy < 0) dir = core::Direction::NorthEast;
        else if (dx < 0 && dy < 0) dir = core::Direction::NorthWest;
        else if (dx > 0 && dy > 0) dir = core::Direction::SouthEast;
        else if (dx < 0 && dy > 0) dir = core::Direction::SouthWest;
        else if (dx > 0) dir = core::Direction::East;
        else if (dx < 0) dir = core::Direction::West;
        else if (dy > 0) dir = core::Direction::South;
        else dir = core::Direction::North;

        return walk(dir);
    }

    bool castSpell(const std::string& spell) override {
        core::Logger::info("UniversalGameAdapter: Cast spell '" + spell + "'");
#ifdef _WIN32
        if (m_hwnd && IsWindow(m_hwnd)) {
            sendText(spell);
        }
#endif
        core::EventManager::instance().publish("PlayerSpellCast", {
            {"spell", spell}
        });
        return true;
    }

    bool useItemOnSelf(uint32_t itemId) override {
        core::Logger::info("UniversalGameAdapter: Use item on self: " + std::to_string(itemId));
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
#ifdef _WIN32
        if (m_hwnd && IsWindow(m_hwnd)) {
            sendKey(VK_SPACE);
        }
#endif
        Player p = GameState::instance().getPlayer();
        p.targetId = creatureId;
        GameState::instance().setPlayer(p);
        return true;
    }

    bool stopAttack() override {
        core::Logger::info("UniversalGameAdapter: Stop attack");
#ifdef _WIN32
        if (m_hwnd && IsWindow(m_hwnd)) {
            sendKey(VK_ESCAPE);
        }
#endif
        Player p = GameState::instance().getPlayer();
        p.targetId = 0;
        GameState::instance().setPlayer(p);
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
#ifdef _WIN32
        if (m_hwnd && IsWindow(m_hwnd)) {
            sendText(message);
        }
#endif
        return true;
    }

private:
    void initDefaultState() {
        Player p;
        p.id = 10001;
        p.name = "Waiting for " + m_targetProcess;
        p.position = {0, 0, 7};
        p.hp = 0;
        p.maxHp = 0;
        p.mp = 0;
        p.maxMp = 0;
        p.level = 0;
        p.capacity = 0.0;
        GameState::instance().setPlayer(p);

        std::vector<Creature> creatures;
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

    static std::string toHex(uintptr_t val) {
        std::stringstream ss;
        ss << std::hex << std::uppercase << val;
        return ss.str();
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
    std::chrono::steady_clock::time_point m_lastMemScanTime{};
    std::chrono::steady_clock::time_point m_lastWalkTime{};
    std::chrono::steady_clock::time_point m_lastCreatureScanTime{};

#ifdef _WIN32
    HWND m_hwnd = NULL;
    HANDLE m_hProcess = NULL;
#endif

    struct MemoryOffsets {
        uintptr_t posAddr = 0;
        bool posIsUint16 = true;
        uintptr_t hpAddr = 0;
        uintptr_t maxHpAddr = 0;
        bool hpIsDouble = true;
        uintptr_t mpAddr = 0;
        uintptr_t maxMpAddr = 0;
        bool mpIsDouble = true;
        uintptr_t lvlAddr = 0;
        bool lvlIsDouble = false;
        uintptr_t capAddr = 0;
        bool capIsDouble = true;
        bool resolved = false;
        int scanAttempts = 0;
    } m_mem;
};

} // namespace bot::game
