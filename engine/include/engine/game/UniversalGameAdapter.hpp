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
#include <cstdio>

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

    static void enableDebugPrivileges() {
#ifdef _WIN32
        HANDLE hToken = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            LUID luid;
            if (LookupPrivilegeValueA(NULL, "SeDebugPrivilege", &luid)) {
                TOKEN_PRIVILEGES tp;
                tp.PrivilegeCount = 1;
                tp.Privileges[0].Luid = luid;
                tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
            }
            CloseHandle(hToken);
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
            enableDebugPrivileges();

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
                                    if (procId == pCtx->targetPid && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == NULL) {
                                        char buf[512];
                                        int l = GetWindowTextA(hwnd, buf, sizeof(buf));
                                        if (l > 0) {
                                            std::string t(buf, l);
                                            pCtx->targetHwnd = hwnd;
                                            pCtx->title = t;
                                            size_t dash = t.find(" - ");
                                            if (dash != std::string::npos && dash + 3 < t.size()) {
                                                pCtx->nick = t.substr(dash + 3);
                                                return FALSE; // Found primary game window
                                            }
                                        }
                                    }
                                    return TRUE;
                                }, reinterpret_cast<LPARAM>(&tCtx));

                                m_hwnd = tCtx.targetHwnd;
                                m_windowTitle = tCtx.title;
                                m_extractedNick = cleanNick(tCtx.nick);

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
                if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != NULL) return TRUE;

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
                m_windowTitle = winCtx.title;
                m_extractedNick = cleanNick(winCtx.nick);
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
        enableDebugPrivileges();

        if (m_hProcess && m_hProcess != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hProcess);
            m_hProcess = NULL;
        }

        // Try opening with broad rights first, fallback to read rights
        m_hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, static_cast<DWORD>(m_processId));
        if (!m_hProcess) {
            m_hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, static_cast<DWORD>(m_processId));
        }
        if (!m_hProcess) {
            m_hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(m_processId));
        }
        if (!m_hProcess) {
            m_hProcess = OpenProcess(PROCESS_VM_READ, FALSE, static_cast<DWORD>(m_processId));
        }

        if (m_hProcess) {
            core::Logger::info("UniversalGameAdapter: Memory handle opened successfully for PID " + std::to_string(m_processId));
            
            // Auto-deploy OTClient native bridge module if client directory is accessible
            char exePath[MAX_PATH];
            DWORD pathLen = sizeof(exePath);
            if (QueryFullProcessImageNameA(m_hProcess, 0, exePath, &pathLen)) {
                installOtclientBridge(exePath);
            }
        } else {
            core::Logger::warn("UniversalGameAdapter: Could not open process memory (Error: " + std::to_string(GetLastError()) + "). Run TibiaBot as Administrator!");
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
    void installOtclientBridge(const std::string& exeFullPath) {
        try {
            size_t lastSlash = exeFullPath.find_last_of("\\/");
            if (lastSlash == std::string::npos) return;
            std::string exeDir = exeFullPath.substr(0, lastSlash);

            std::string modulesDir = exeDir + "\\modules";
            DWORD attr = GetFileAttributesA(modulesDir.c_str());
            if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                size_t parentSlash = exeDir.find_last_of("\\/");
                if (parentSlash != std::string::npos) {
                    std::string parentModules = exeDir.substr(0, parentSlash) + "\\modules";
                    attr = GetFileAttributesA(parentModules.c_str());
                    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                        modulesDir = parentModules;
                    } else {
                        return;
                    }
                } else {
                    return;
                }
            }

            std::string botModDir = modulesDir + "\\tibia_bot";
            CreateDirectoryA(botModDir.c_str(), NULL);

            std::string otmodPath = botModDir + "\\tibia_bot.otmod";
            std::string luaPath = botModDir + "\\tibia_bot.lua";

            FILE* fMod = fopen(otmodPath.c_str(), "w");
            if (fMod) {
                fputs("Module\n"
                      "  name: tibia_bot\n"
                      "  description: TibiaBot Engine Native Bridge\n"
                      "  author: Antigravity\n"
                      "  website: https://github.com/Smykovsky/tibia_bot\n"
                      "  sandboxed: false\n"
                      "  scripts: [ tibia_bot.lua ]\n"
                      "  autoload: true\n"
                      "  autoload-priority: 99999\n"
                      "  @onLoad: init()\n"
                      "  @onUnload: terminate()\n", fMod);
                fclose(fMod);
            }

            FILE* fLua = fopen(luaPath.c_str(), "w");
            if (fLua) {
                const char* script =
                    "local bridgeTimer = nil\n"
                    "\n"
                    "function init()\n"
                    "  bridgeTimer = cycleEvent(updateBridge, 50)\n"
                    "  print('[TibiaBot] Native Engine Bridge loaded!')\n"
                    "end\n"
                    "\n"
                    "function terminate()\n"
                    "  if bridgeTimer then\n"
                    "    removeEvent(bridgeTimer)\n"
                    "    bridgeTimer = nil\n"
                    "  end\n"
                    "end\n"
                    "\n"
                    "function updateBridge()\n"
                    "  if not g_game.isOnline() then return end\n"
                    "  local player = g_game.getLocalPlayer()\n"
                    "  if not player then return end\n"
                    "\n"
                    "  local pos = player:getPosition()\n"
                    "  local health = player:getHealth()\n"
                    "  local maxHealth = player:getMaxHealth()\n"
                    "  local mana = player:getMana()\n"
                    "  local maxMana = player:getMaxMana()\n"
                    "  local level = player:getLevel()\n"
                    "  local cap = player:getFreeCapacity()\n"
                    "  local name = player:getName()\n"
                    "\n"
                    "  local temp = os.getenv('TEMP') or 'C:\\\\Windows\\\\Temp'\n"
                    "  local cmdPath = temp .. '\\\\tibia_bot_cmd.txt'\n"
                    "  local cf = io.open(cmdPath, 'r')\n"
                    "  if cf then\n"
                    "    local content = cf:read('*all')\n"
                    "    cf:close()\n"
                    "    os.remove(cmdPath)\n"
                    "    if content and #content > 0 then\n"
                    "      for line in string.gmatch(content, '[^\\r\\n]+') do\n"
                    "        if string.sub(line, 1, 5) == 'WALK:' then\n"
                    "          local d = tonumber(string.sub(line, 6))\n"
                    "          if d then g_game.walk(d) end\n"
                    "        elseif string.sub(line, 1, 4) == 'SAY:' then\n"
                    "          local s = string.sub(line, 5)\n"
                    "          if s and #s > 0 then g_game.talk(s) end\n"
                    "        elseif line == 'ATTACK' then\n"
                    "          local curTgt = g_game.getAttackingCreature()\n"
                    "          if not curTgt then\n"
                    "            local specs = g_map.getSpectators(pos, false)\n"
                    "            for _, c in ipairs(specs) do\n"
                    "              if c:isMonster() then g_game.attack(c) break end\n"
                    "            end\n"
                    "          end\n"
                    "        elseif line == 'STOP' then\n"
                    "          g_game.cancelAttack()\n"
                    "        end\n"
                    "      end\n"
                    "    end\n"
                    "  end\n"
                    "\n"
                    "  local str = string.format('POS:%d,%d,%d;HP:%d,%d;MP:%d,%d;LVL:%d;CAP:%.1f;NAME:%s\\n',\n"
                    "    pos.x, pos.y, pos.z,\n"
                    "    math.floor(health), math.floor(maxHealth),\n"
                    "    math.floor(mana), math.floor(maxMana),\n"
                    "    math.floor(level), cap, name)\n"
                    "\n"
                    "  local statePath = temp .. '\\\\tibia_bot_state.txt'\n"
                    "  local sf = io.open(statePath, 'w')\n"
                    "  if sf then\n"
                    "    sf:write(str)\n"
                    "    sf:close()\n"
                    "  end\n"
                    "end\n";
                fputs(script, fLua);
                fclose(fLua);
            }

            core::Logger::info("UniversalGameAdapter: [BRIDGE] Deployed native OTClient bridge module to: " + botModDir);
        } catch (...) {}
    }

    bool readFromBridgeFile(Player& p) {
        char tempPath[MAX_PATH];
        DWORD len = GetTempPathA(sizeof(tempPath), tempPath);
        if (len == 0) return false;
        std::string stateFile = std::string(tempPath) + "tibia_bot_state.txt";

        HANDLE hFile = CreateFileA(stateFile.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        FILETIME lastWrite;
        if (GetFileTime(hFile, NULL, NULL, &lastWrite)) {
            FILETIME curTime;
            GetSystemTimeAsFileTime(&curTime);
            ULARGE_INTEGER tLast, tCur;
            tLast.LowPart = lastWrite.dwLowDateTime; tLast.HighPart = lastWrite.dwHighDateTime;
            tCur.LowPart = curTime.dwLowDateTime; tCur.HighPart = curTime.dwHighDateTime;
            // File must be updated in last 2.5s (25,000,000 in 100ns units)
            if (tCur.QuadPart > tLast.QuadPart && (tCur.QuadPart - tLast.QuadPart) > 25000000ULL) {
                CloseHandle(hFile);
                return false;
            }
        }

        char buf[512]{};
        DWORD readBytes = 0;
        ReadFile(hFile, buf, sizeof(buf) - 1, &readBytes, NULL);
        CloseHandle(hFile);
        if (readBytes == 0) return false;

        std::string data(buf, readBytes);
        int x=0, y=0, z=7, hp=0, maxHp=0, mp=0, maxMp=0, lvl=0;
        float cap=0.0f;
        char nameBuf[128]{};

        if (sscanf(data.c_str(), "POS:%d,%d,%d;HP:%d,%d;MP:%d,%d;LVL:%d;CAP:%f;NAME:%127[^\r\n]",
                   &x, &y, &z, &hp, &maxHp, &mp, &maxMp, &lvl, &cap, nameBuf) >= 7) {
            p.position = {static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint8_t>(z)};
            p.hp = hp;
            p.maxHp = maxHp;
            p.mp = mp;
            p.maxMp = maxMp;
            p.level = lvl;
            p.capacity = cap;
            if (nameBuf[0] != '\0') p.name = std::string(nameBuf);
            return true;
        }
        return false;
    }

    void sendBridgeCommand(const std::string& cmd) {
        char tempPath[MAX_PATH];
        if (GetTempPathA(sizeof(tempPath), tempPath) == 0) return;
        std::string cmdPath = std::string(tempPath) + "tibia_bot_cmd.txt";
        FILE* f = fopen(cmdPath.c_str(), "a");
        if (f) {
            fputs((cmd + "\n").c_str(), f);
            fclose(f);
        }
    }

    void scanMemoryForPlayer() {
        if (!m_hProcess || m_hProcess == INVALID_HANDLE_VALUE) return;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastMemScanTime).count();
        if (elapsed < 2000 && m_mem.scanAttempts > 0) return;
        m_lastMemScanTime = now;
        m_mem.scanAttempts++;

        std::string nick = m_extractedNick;
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
                if (ReadProcessMemory(m_hProcess, mbi.BaseAddress, buffer.data(), regionSize, &bytesRead) && bytesRead > 16) {
                    
                    const uint8_t* pBuf = buffer.data();

                    // Strategy A: Scan for nickname occurrence if nick is known
                    if (!nick.empty() && bytesRead > nick.size()) {
                        size_t limit = bytesRead - nick.size();
                        for (size_t i = 0; i < limit; ++i) {
                            if (std::memcmp(pBuf + i, nick.data(), nick.size()) == 0) {
                                size_t windowStart = (i >= 512) ? (i - 512) : 0;
                                size_t windowEnd = (i + 512 + 8 <= bytesRead) ? (i + 512) : (bytesRead - 8);

                                for (size_t j = windowStart; j < windowEnd; ++j) {
                                    uint16_t x = *reinterpret_cast<const uint16_t*>(pBuf + j);
                                    uint16_t y = *reinterpret_cast<const uint16_t*>(pBuf + j + 2);
                                    uint8_t z = *(pBuf + j + 4);

                                    if (x >= 500 && x <= 65000 && y >= 500 && y <= 65000 && z <= 15) {
                                        m_mem.posAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + j;
                                        m_mem.posIsUint16 = true;

                                        // Scan nearby for health & mana
                                        for (size_t k = i; k + 16 <= (i + 1024 <= bytesRead ? i + 1024 : bytesRead); k += 4) {
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
                                        }

                                        m_mem.resolved = true;
                                        core::Logger::info("==================================================");
                                        core::Logger::info(" [MEMORY RESOLVED] Player entity hooked in RAM!");
                                        core::Logger::info("  - Position Addr: 0x" + toHex(m_mem.posAddr));
                                        if (m_mem.hpAddr) core::Logger::info("  - HP Addr:       0x" + toHex(m_mem.hpAddr));
                                        if (m_mem.mpAddr) core::Logger::info("  - MP Addr:       0x" + toHex(m_mem.mpAddr));
                                        core::Logger::info("==================================================");
                                        return;
                                    }
                                }
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

        Player p = GameState::instance().getPlayer();
        bool changed = false;

        // 1. Try reading from native OTClient bridge file first (highest reliability)
        if (readFromBridgeFile(p)) {
            GameState::instance().setPlayer(p);
            return;
        }

        // 2. Direct RAM memory reading fallback
        if (!m_hProcess || m_hProcess == INVALID_HANDLE_VALUE) {
            m_hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(m_processId));
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
                        std::string newNick = cleanNick(curTitle.substr(dash + 3));
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

        // Read Position
        if (m_mem.posAddr) {
            struct { uint16_t x; uint16_t y; uint8_t z; } rawPos{};
            if (ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_mem.posAddr), &rawPos, sizeof(rawPos), nullptr)) {
                if (rawPos.x >= 100 && rawPos.x <= 65000 && rawPos.y >= 100 && rawPos.y <= 65000 && rawPos.z <= 15) {
                    if (p.position.x != rawPos.x || p.position.y != rawPos.y || p.position.z != rawPos.z) {
                        p.position.x = rawPos.x;
                        p.position.y = rawPos.y;
                        p.position.z = rawPos.z;
                        changed = true;
                    }
                }
            }
        }

        // Read HP
        if (m_mem.hpAddr) {
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
        }

        // Read Mana
        if (m_mem.mpAddr) {
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
        Sleep(15);
        PostMessageA(m_hwnd, WM_KEYUP, vk, lParamUp);
    }

    void sendText(const std::string& text) {
        if (!m_hwnd || !IsWindow(m_hwnd)) return;
        for (char c : text) {
            PostMessageA(m_hwnd, WM_CHAR, static_cast<WPARAM>(static_cast<unsigned char>(c)), 1);
            Sleep(5);
        }
        sendKey(VK_RETURN);
    }
#endif

    bool walk(core::Direction dir) override {
#ifdef _WIN32
        // Send command to bridge if active
        int d = 0;
        WORD vk = 0;
        switch (dir) {
            case core::Direction::North:     d = 0; vk = VK_UP; break;
            case core::Direction::East:      d = 1; vk = VK_RIGHT; break;
            case core::Direction::South:     d = 2; vk = VK_DOWN; break;
            case core::Direction::West:      d = 3; vk = VK_LEFT; break;
            case core::Direction::NorthEast: d = 4; vk = VK_NUMPAD9; break;
            case core::Direction::SouthEast: d = 5; vk = VK_NUMPAD3; break;
            case core::Direction::SouthWest: d = 6; vk = VK_NUMPAD1; break;
            case core::Direction::NorthWest: d = 7; vk = VK_NUMPAD7; break;
            default: break;
        }

        sendBridgeCommand("WALK:" + std::to_string(d));

        if (m_hwnd && IsWindow(m_hwnd) && vk != 0) {
            sendKey(vk);
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
        sendBridgeCommand("SAY:" + spell);
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
        sendBridgeCommand("ATTACK");
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
        sendBridgeCommand("STOP");
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
        sendBridgeCommand("SAY:" + message);
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

    static std::string cleanNick(std::string nick) {
        while (!nick.empty() && (nick.back() == ' ' || nick.back() == '\r' || nick.back() == '\n' || nick.back() == '\t')) {
            nick.pop_back();
        }
        while (!nick.empty() && (nick.front() == ' ' || nick.front() == '\r' || nick.front() == '\n' || nick.front() == '\t')) {
            nick.erase(nick.begin());
        }
        // Remove trailing [brackets] or parentheses
        size_t b = nick.find(" [");
        if (b != std::string::npos) nick = nick.substr(0, b);
        size_t p = nick.find(" (");
        if (p != std::string::npos) nick = nick.substr(0, p);
        return nick;
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
