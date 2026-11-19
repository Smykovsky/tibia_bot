import os
from PySide6.QtWidgets import (
    QMainWindow, QWidget, QHBoxLayout, QVBoxLayout,
    QListWidget, QListWidgetItem, QStackedWidget,
    QLabel, QFrame, QMessageBox
)
from PySide6.QtCore import Qt, QTimer, Signal, QObject
from PySide6.QtGui import QFont, QIcon

from launcher.ui.style import DARK_THEME_QSS
from launcher.ui.tabs.dashboard_tab import DashboardTab
from launcher.ui.tabs.healing_tab import HealingTab
from launcher.ui.tabs.cavebot_tab import CavebotTab
from launcher.ui.tabs.targeting_tab import TargetingTab
from launcher.ui.tabs.looting_tab import LootingTab
from launcher.ui.tabs.smart_aoe_tab import SmartAoeTab
from launcher.ui.tabs.scripts_tab import ScriptsTab
from launcher.ui.tabs.settings_tab import SettingsTab

class SignalBridge(QObject):
    log_received = Signal(str, str, str) # level, message, timestamp
    event_received = Signal(str, dict)    # type, data

class MainWindow(QMainWindow):
    def __init__(self, engine_client, engine_process):
        super().__init__()
        self.client = engine_client
        self.process = engine_process

        self.setWindowTitle("TibiaBot – Shrouded Edition (Launcher)")
        self.resize(1150, 720)
        self.setMinimumSize(950, 600)
        self.setStyleSheet(DARK_THEME_QSS)

        self.signals = SignalBridge()
        self.signals.log_received.connect(self.on_log_received)
        self.signals.event_received.connect(self.on_event_received)

        # Connect client callbacks to signal bridge
        self.client.on_log_callback = lambda lvl, msg, ts: self.signals.log_received.emit(lvl, msg, ts)
        self.client.on_event_callback = lambda t, d: self.signals.event_received.emit(t, d)

        self.init_ui()
        self.load_default_config()

        # Telemetry & State polling timer (4Hz = 250ms)
        self.poll_timer = QTimer(self)
        self.poll_timer.timeout.connect(self.poll_engine_state)
        self.poll_timer.start(250)

    def init_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)

        main_layout = QHBoxLayout(central_widget)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)

        # 1. Left Sidebar
        sidebar_frame = QFrame()
        sidebar_frame.setObjectName("sidebarFrame")
        sidebar_frame.setFixedWidth(220)
        sidebar_frame.setStyleSheet("background-color: #0f172a; border-right: 1px solid #1e293b;")
        sb_layout = QVBoxLayout(sidebar_frame)
        sb_layout.setContentsMargins(0, 0, 0, 0)
        sb_layout.setSpacing(0)

        # App Brand Header
        brand_frame = QFrame()
        brand_frame.setFixedHeight(70)
        brand_layout = QVBoxLayout(brand_frame)
        brand_layout.setContentsMargins(20, 16, 20, 10)

        brand_lbl = QLabel("TIBIABOT")
        brand_lbl.setStyleSheet("font-size: 18px; font-weight: 800; color: #38bdf8; letter-spacing: 1px;")
        sub_lbl = QLabel("SHROUDED EDITION")
        sub_lbl.setStyleSheet("font-size: 10px; font-weight: 600; color: #64748b; letter-spacing: 2px;")
        brand_layout.addWidget(brand_lbl)
        brand_layout.addWidget(sub_lbl)
        sb_layout.addWidget(brand_frame)

        # Navigation List
        self.nav_list = QListWidget()
        self.nav_list.setObjectName("navSidebar")
        nav_items = [
            ("Dashboard", "📊"),
            ("Healing", "💊"),
            ("Cavebot", "🗺️"),
            ("Targeting", "⚔️"),
            ("Looting", "🎒"),
            ("Smart AOE", "🔥"),
            ("Lua Scripts", "📜"),
            ("Settings", "⚙️")
        ]
        for name, icon in nav_items:
            item = QListWidgetItem(f" {icon}   {name}")
            self.nav_list.addItem(item)

        self.nav_list.currentRowChanged.connect(self.on_tab_changed)
        sb_layout.addWidget(self.nav_list)

        # Bottom Version Label
        ver_lbl = QLabel("v1.0.0 (C++ Core + PySide6)")
        ver_lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
        ver_lbl.setStyleSheet("color: #475569; font-size: 11px; padding: 12px;")
        sb_layout.addWidget(ver_lbl)

        main_layout.addWidget(sidebar_frame)

        # 2. Right Stacked Widget for Tab Content
        self.stack = QStackedWidget()
        self.tab_dashboard = DashboardTab(self.client, self.process)
        self.tab_healing = HealingTab(self.client)
        self.tab_cavebot = CavebotTab(self.client)
        self.tab_targeting = TargetingTab(self.client)
        self.tab_looting = LootingTab(self.client)
        self.tab_smart_aoe = SmartAoeTab(self.client)
        self.tab_scripts = ScriptsTab(self.client)
        self.tab_settings = SettingsTab(self.client, self.process)

        self.stack.addWidget(self.tab_dashboard)
        self.stack.addWidget(self.tab_healing)
        self.stack.addWidget(self.tab_cavebot)
        self.stack.addWidget(self.tab_targeting)
        self.stack.addWidget(self.tab_looting)
        self.stack.addWidget(self.tab_smart_aoe)
        self.stack.addWidget(self.tab_scripts)
        self.stack.addWidget(self.tab_settings)

        main_layout.addWidget(self.stack)

        # Select Dashboard by default
        self.nav_list.setCurrentRow(0)

    def load_default_config(self):
        import json
        import sys
        if getattr(sys, 'frozen', False):
            base = getattr(sys, '_MEIPASS', os.path.dirname(sys.executable))
            candidates = [
                os.path.join(base, "configs", "default.json"),
                os.path.join(os.path.dirname(sys.executable), "configs", "default.json"),
            ]
            config_path = next((c for c in candidates if os.path.exists(c)), candidates[0])
        else:
            config_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "configs", "default.json"))
        if not os.path.exists(config_path):
            return
        try:
            with open(config_path, "r", encoding="utf-8") as f:
                cfg = json.load(f)
            if "Healing" in cfg:
                self.tab_healing.rules = cfg["Healing"].get("rules", [])
                self.tab_healing.refresh_table()
            if "Cavebot" in cfg:
                self.tab_cavebot.waypoints = cfg["Cavebot"].get("waypoints", [])
                self.tab_cavebot.refresh_table()
            if "Targeting" in cfg:
                self.tab_targeting.targets = cfg["Targeting"].get("targets", [])
                self.tab_targeting.refresh_table()
            if "Looting" in cfg:
                self.tab_looting.items = cfg["Looting"].get("items", [])
                self.tab_looting.refresh_table()
            if "SmartAOE" in cfg:
                self.tab_smart_aoe.rules = cfg["SmartAOE"].get("rules", [])
                self.tab_smart_aoe.refresh_table()
        except Exception as e:
            print("Failed to load default config into tabs:", e)

    def on_tab_changed(self, index: int):
        self.stack.setCurrentIndex(index)
        # Auto-fetch config when switching to module tab
        if self.client.is_connected:
            current_tab = self.stack.widget(index)
            if hasattr(current_tab, "fetch_from_engine"):
                try:
                    current_tab.fetch_from_engine()
                except:
                    pass

    def on_log_received(self, level: str, message: str, timestamp: str):
        color = "#e2e8f0"
        if level == "warn":
            color = "#f59e0b"
        elif level == "error":
            color = "#ef4444"
        elif level == "debug":
            color = "#64748b"

        formatted = f"{timestamp} [{level.upper()}] {message}"
        self.tab_dashboard.append_log(formatted)

    def on_event_received(self, ev_type: str, data: dict):
        # Notify dashboard of key events
        if ev_type in ["PlayerDamageTaken", "PlayerHealed", "WaypointReached", "ExecuteScript"]:
            self.tab_dashboard.append_log(f">> Event: {ev_type} -> {data}")

    def poll_engine_state(self):
        # Auto-connect if process running but not connected
        if self.process.is_running() and not self.client.is_connected:
            self.client.connect()

        if self.client.is_connected:
            try:
                eng_state = self.client.get_engine_state()
                game_state = self.client.get_game_state()
                modules = self.client.get_modules()
                self.tab_dashboard.update_state(game_state, eng_state, modules)
            except Exception:
                pass
        else:
            self.tab_dashboard.update_state({}, {"state": "offline"}, [])

    def closeEvent(self, event):
        self.poll_timer.stop()
        self.client.disconnect()
        # Optionally prompt to stop engine or let it run
        event.accept()
