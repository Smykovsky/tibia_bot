from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout, QLabel,
    QPushButton, QProgressBar, QCheckBox, QPlainTextEdit, QFrame, QGroupBox
)
from PySide6.QtCore import Qt, QTimer
from launcher.core.models import PlayerState

class DashboardTab(QWidget):
    def __init__(self, engine_client, engine_process, parent=None):
        super().__init__(parent)
        self.client = engine_client
        self.process = engine_process
        self.init_ui()

    def init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(16)

        # 1. Top Bar: Engine Control & Connection Status
        top_bar = QFrame()
        top_bar.setProperty("class", "cardFrame")
        top_layout = QHBoxLayout(top_bar)
        top_layout.setContentsMargins(15, 12, 15, 12)

        self.lbl_engine_status = QLabel("Engine: OFFLINE")
        self.lbl_engine_status.setStyleSheet("font-weight: 700; font-size: 14px; color: #ef4444;")
        top_layout.addWidget(self.lbl_engine_status)

        self.lbl_ipc_status = QLabel("IPC: Disconnected")
        self.lbl_ipc_status.setStyleSheet("color: #94a3b8; font-size: 13px;")
        top_layout.addWidget(self.lbl_ipc_status)

        top_layout.addStretch()

        self.btn_start = QPushButton("Start Engine")
        self.btn_start.setProperty("class", "successButton")
        self.btn_start.clicked.connect(self.on_start_engine)
        top_layout.addWidget(self.btn_start)

        self.btn_pause = QPushButton("Pause")
        self.btn_pause.clicked.connect(self.on_pause_engine)
        top_layout.addWidget(self.btn_pause)

        self.btn_stop = QPushButton("Stop Engine")
        self.btn_stop.setProperty("class", "dangerButton")
        self.btn_stop.clicked.connect(self.on_stop_engine)
        top_layout.addWidget(self.btn_stop)

        layout.addWidget(top_bar)

        # 2. Main Grid: Character HUD (Left) + Modules Quick-Toggle (Right)
        grid_layout = QHBoxLayout()
        grid_layout.setSpacing(16)

        # Left: Character HUD
        hud_card = QFrame()
        hud_card.setProperty("class", "cardFrame")
        hud_layout = QVBoxLayout(hud_card)

        hud_title = QLabel("Character Status (HUD)")
        hud_title.setProperty("class", "cardTitle")
        hud_layout.addWidget(hud_title)

        info_grid = QGridLayout()
        self.lbl_char_name = QLabel("Name: -")
        self.lbl_char_level = QLabel("Level: -")
        self.lbl_char_pos = QLabel("Pos: (0, 0, 0)")
        self.lbl_char_cap = QLabel("Cap: 0")

        self.lbl_char_name.setStyleSheet("font-weight: 600; color: #f8fafc;")
        self.lbl_char_level.setStyleSheet("color: #94a3b8;")
        self.lbl_char_pos.setStyleSheet("color: #38bdf8; font-family: monospace;")
        self.lbl_char_cap.setStyleSheet("color: #94a3b8;")

        info_grid.addWidget(self.lbl_char_name, 0, 0)
        info_grid.addWidget(self.lbl_char_level, 0, 1)
        info_grid.addWidget(self.lbl_char_pos, 1, 0)
        info_grid.addWidget(self.lbl_char_cap, 1, 1)

        self.lbl_client_info = QLabel("Client: Searching for dbvStart.exe / DBVictory.exe / Tibia.exe...")
        self.lbl_client_info.setStyleSheet("color: #f59e0b; font-size: 11px; font-weight: 600;")
        info_grid.addWidget(self.lbl_client_info, 2, 0, 1, 2)

        hud_layout.addLayout(info_grid)

        # HP Bar
        hud_layout.addSpacing(6)
        self.lbl_hp_text = QLabel("HP: 0 / 0 (0%)")
        self.lbl_hp_text.setStyleSheet("font-size: 11px; font-weight: 600; color: #f87171;")
        hud_layout.addWidget(self.lbl_hp_text)

        self.hp_bar = QProgressBar()
        self.hp_bar.setObjectName("hpBar")
        self.hp_bar.setValue(0)
        hud_layout.addWidget(self.hp_bar)

        # MP Bar
        hud_layout.addSpacing(6)
        self.lbl_mp_text = QLabel("Mana: 0 / 0 (0%)")
        self.lbl_mp_text.setStyleSheet("font-size: 11px; font-weight: 600; color: #60a5fa;")
        hud_layout.addWidget(self.lbl_mp_text)

        self.mp_bar = QProgressBar()
        self.mp_bar.setObjectName("mpBar")
        self.mp_bar.setValue(0)
        hud_layout.addWidget(self.mp_bar)

        grid_layout.addWidget(hud_card, stretch=3)

        # Right: Modules Quick Toggles
        modules_card = QFrame()
        modules_card.setProperty("class", "cardFrame")
        modules_layout = QVBoxLayout(modules_card)

        mod_title = QLabel("Active Modules")
        mod_title.setProperty("class", "cardTitle")
        modules_layout.addWidget(mod_title)

        self.chk_modules = {}
        for mod_name in ["Healing", "Cavebot", "Targeting", "Looting", "SmartAOE"]:
            chk = QCheckBox(f"Enable {mod_name}")
            chk.stateChanged.connect(lambda state, name=mod_name: self.on_module_toggle(name, state))
            self.chk_modules[mod_name] = chk
            modules_layout.addWidget(chk)

        modules_layout.addStretch()
        grid_layout.addWidget(modules_card, stretch=2)

        layout.addLayout(grid_layout)

        # 3. Live Log Viewer
        log_card = QFrame()
        log_card.setProperty("class", "cardFrame")
        log_layout = QVBoxLayout(log_card)

        log_header = QHBoxLayout()
        log_title = QLabel("Live Engine Log Console")
        log_title.setProperty("class", "cardTitle")
        log_header.addWidget(log_title)
        log_header.addStretch()

        btn_clear_log = QPushButton("Clear Console")
        btn_clear_log.clicked.connect(self.clear_logs)
        log_header.addWidget(btn_clear_log)
        log_layout.addLayout(log_header)

        self.log_text = QPlainTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setMaximumBlockCount(1000)
        log_layout.addWidget(self.log_text)

        layout.addWidget(log_card, stretch=1)

    def on_start_engine(self):
        if not self.process.is_running():
            try:
                started = self.process.start()
                if started:
                    self.append_log(f"Started Engine process: {self.process.binary_path}")
                    # Give socket a small moment to accept connections
                    QTimer.singleShot(600, self._connect_ipc)
                else:
                    self.append_log(f"Engine process could not be started.")
            except Exception as e:
                self.append_log(f"Error starting engine: {e}")
        else:
            try:
                self.client.resume_engine()
                self.append_log("Resumed Engine execution.")
            except Exception as e:
                self.append_log(f"Error resuming engine: {e}")

    def _connect_ipc(self):
        try:
            connected = self.client.connect()
            if connected:
                self.append_log("IPC connected to tibia_engine on port 8765.")
            else:
                self.append_log("Warning: IPC connection to 127.0.0.1:8765 failed. Will retry automatically.")
        except Exception as e:
            self.append_log(f"IPC connection error: {e}")

    def on_pause_engine(self):
        try:
            self.client.pause_engine()
        except Exception as e:
            self.append_log(f"Error pausing engine: {e}")

    def on_stop_engine(self):
        try:
            self.client.stop_engine()
        except:
            pass
        self.process.stop()

    def on_module_toggle(self, name: str, state: int):
        enabled = (state == Qt.CheckState.Checked.value)
        if self.client.is_connected:
            try:
                self.client.set_module_state(name, enabled)
            except Exception as e:
                self.append_log(f"Error updating module {name}: {e}")

    def append_log(self, text: str):
        self.log_text.appendPlainText(text)

    def clear_logs(self):
        self.log_text.clear()

    def update_state(self, game_state: dict, engine_state: dict, modules: list):
        # Update Engine Status
        state_str = engine_state.get("state", "offline").upper()
        if state_str == "RUNNING":
            self.lbl_engine_status.setText("Engine: RUNNING")
            self.lbl_engine_status.setStyleSheet("font-weight: 700; font-size: 14px; color: #10b981;")
        elif state_str == "PAUSED":
            self.lbl_engine_status.setText("Engine: PAUSED")
            self.lbl_engine_status.setStyleSheet("font-weight: 700; font-size: 14px; color: #f59e0b;")
        else:
            self.lbl_engine_status.setText("Engine: STOPPED")
            self.lbl_engine_status.setStyleSheet("font-weight: 700; font-size: 14px; color: #ef4444;")

        if self.client.is_connected:
            self.lbl_ipc_status.setText("IPC: Connected (127.0.0.1)")
            self.lbl_ipc_status.setStyleSheet("color: #38bdf8; font-size: 13px;")
        else:
            self.lbl_ipc_status.setText("IPC: Disconnected")
            self.lbl_ipc_status.setStyleSheet("color: #94a3b8; font-size: 13px;")

        # Update HUD
        p = game_state.get("player", {})
        if p:
            name = p.get("name", "Unknown")
            lvl = p.get("level", 1)
            pos = p.get("position", {})
            cap = p.get("capacity", 0.0)
            hp = p.get("hp", 0)
            max_hp = p.get("maxHp", 100)
            hp_pct = int(p.get("hpPercent", 0))
            mp = p.get("mp", 0)
            max_mp = p.get("maxMp", 100)
            mp_pct = int(p.get("mpPercent", 0))

            self.lbl_char_name.setText(f"Name: {name}")
            self.lbl_char_level.setText(f"Level: {lvl}")
            self.lbl_char_pos.setText(f"Pos: ({pos.get('x',0)}, {pos.get('y',0)}, {pos.get('z',7)})")
            self.lbl_char_cap.setText(f"Cap: {cap:.1f}")

            self.hp_bar.setValue(hp_pct)
            self.lbl_hp_text.setText(f"HP: {hp} / {max_hp} ({hp_pct}%)")

            self.mp_bar.setValue(mp_pct)
            self.lbl_mp_text.setText(f"Mana: {mp} / {max_mp} ({mp_pct}%)")

            # Update client process status
            if "Waiting for" in name:
                self.lbl_client_info.setText(f"Game Client: {name}...")
                self.lbl_client_info.setStyleSheet("color: #f59e0b; font-size: 11px; font-weight: 600;")
            else:
                self.lbl_client_info.setText(f"Game Client: ● Active ({name})")
                self.lbl_client_info.setStyleSheet("color: #10b981; font-size: 11px; font-weight: 600;")

        # Update Module Checkboxes without retriggering signal
        for m in modules:
            name = m.get("name")
            enabled = m.get("enabled", False)
            if name in self.chk_modules:
                chk = self.chk_modules[name]
                chk.blockSignals(True)
                chk.setChecked(enabled)
                chk.blockSignals(False)
