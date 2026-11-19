import os
import sys
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
    QFrame, QLineEdit, QSpinBox, QComboBox, QMessageBox
)

class SettingsTab(QWidget):
    def __init__(self, engine_client, engine_process, parent=None):
        super().__init__(parent)
        self.client = engine_client
        self.process = engine_process
        if getattr(sys, 'frozen', False):
            self.configs_dir = os.path.join(os.path.dirname(sys.executable), "configs")
        else:
            self.configs_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", "configs"))
        self.init_ui()
        self.refresh_profiles()

    def init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(16)

        title = QLabel("Application & Profile Settings")
        title.setProperty("class", "sectionTitle")
        layout.addWidget(title)

        # 1. Profile Manager Card
        p_card = QFrame()
        p_card.setProperty("class", "cardFrame")
        p_layout = QVBoxLayout(p_card)

        p_title = QLabel("Profile Management")
        p_title.setProperty("class", "cardTitle")
        p_layout.addWidget(p_title)

        p_row = QHBoxLayout()
        self.cb_profiles = QComboBox()
        self.in_new_profile = QLineEdit()
        self.in_new_profile.setPlaceholderText("New profile name...")

        btn_load = QPushButton("Load Profile")
        btn_load.setProperty("class", "primaryButton")
        btn_load.clicked.connect(self.on_load_profile)

        btn_save = QPushButton("Save Current")
        btn_save.clicked.connect(self.on_save_profile)

        btn_create = QPushButton("Create New")
        btn_create.setProperty("class", "successButton")
        btn_create.clicked.connect(self.on_create_profile)

        p_row.addWidget(QLabel("Active Profile:"))
        p_row.addWidget(self.cb_profiles)
        p_row.addWidget(btn_load)
        p_row.addWidget(btn_save)
        p_row.addSpacing(20)
        p_row.addWidget(self.in_new_profile)
        p_row.addWidget(btn_create)
        p_layout.addLayout(p_row)

        layout.addWidget(p_card)

        # 2. Engine IPC Settings Card
        e_card = QFrame()
        e_card.setProperty("class", "cardFrame")
        e_layout = QVBoxLayout(e_card)

        e_title = QLabel("Engine Connection & Process")
        e_title.setProperty("class", "cardTitle")
        e_layout.addWidget(e_title)

        e_row = QHBoxLayout()
        self.in_host = QLineEdit("127.0.0.1")
        self.sp_port = QSpinBox()
        self.sp_port.setRange(1024, 65535)
        self.sp_port.setValue(8765)

        btn_restart = QPushButton("Restart Engine Process")
        btn_restart.clicked.connect(self.on_restart_engine)

        e_row.addWidget(QLabel("Host:"))
        e_row.addWidget(self.in_host)
        e_row.addWidget(QLabel("Port:"))
        e_row.addWidget(self.sp_port)
        e_row.addStretch()
        e_row.addWidget(btn_restart)
        e_layout.addLayout(e_row)

        layout.addWidget(e_card)

        # 3. Game Client Adapter (Universal Process Detector)
        client_card = QFrame()
        client_card.setProperty("class", "cardFrame")
        c_layout = QVBoxLayout(client_card)

        c_title = QLabel("Game Client Target & Detection (Universal Adapter)")
        c_title.setProperty("class", "cardTitle")
        c_layout.addWidget(c_title)

        c_row = QHBoxLayout()
        self.cb_client_proc = QComboBox()
        self.cb_client_proc.setEditable(True)
        self.cb_client_proc.addItems(["dbvStart.exe", "DBVictory.exe", "Tibia.exe", "otclient.exe", "client.exe", "tibia"])

        btn_apply_client = QPushButton("Set Target Client")
        btn_apply_client.setProperty("class", "primaryButton")
        btn_apply_client.clicked.connect(self.on_set_target_client)

        btn_scan_client = QPushButton("Scan Now")
        btn_scan_client.clicked.connect(self.on_scan_clients)

        c_row.addWidget(QLabel("Target Executable:"))
        c_row.addWidget(self.cb_client_proc)
        c_row.addWidget(btn_apply_client)
        c_row.addWidget(btn_scan_client)
        c_row.addStretch()
        c_layout.addLayout(c_row)

        self.lbl_adapter_status = QLabel("Status: Adapter searching for dbvStart.exe / DBVictory.exe / Tibia.exe...")
        self.lbl_adapter_status.setStyleSheet("color: #38bdf8; font-size: 12px; font-weight: 600; padding-top: 4px;")
        c_layout.addWidget(self.lbl_adapter_status)

        layout.addWidget(client_card)

        # 4. About Card
        about_card = QFrame()
        about_card.setProperty("class", "cardFrame")
        a_layout = QVBoxLayout(about_card)

        a_title = QLabel("About TibiaBot (Shrouded Architecture)")
        a_title.setProperty("class", "cardTitle")
        a_layout.addWidget(a_title)

        desc = QLabel(
            "TibiaBot combines a high-performance C++20 engine with a modern Python / PySide6 Launcher.\n"
            "• Engine: Native C++ Core, A* Pathfinding, Sub-50ms Reaction Tick Loop\n"
            "• Scripting: Lua 5.4 Embedded Environment\n"
            "• IPC: Line-Delimited JSON-RPC 2.0 over Local TCP Sockets\n"
            "• Version: 1.0.0 Release Candidate"
        )
        desc.setStyleSheet("color: #94a3b8; line-height: 1.6;")
        a_layout.addWidget(desc)

        layout.addWidget(about_card)
        layout.addStretch()

    def refresh_profiles(self):
        self.cb_profiles.clear()
        if not os.path.exists(self.configs_dir):
            os.makedirs(self.configs_dir, exist_ok=True)

        for fname in sorted(os.listdir(self.configs_dir)):
            if fname.endswith(".json"):
                self.cb_profiles.addItem(fname[:-5])

    def on_load_profile(self):
        profile = self.cb_profiles.currentText()
        if not profile:
            return
        if not self.client.is_connected:
            QMessageBox.warning(self, "Offline", "Connect to Engine to load profile.")
            return
        try:
            ok = self.client.load_profile(profile)
            if ok:
                QMessageBox.information(self, "Success", f"Profile '{profile}' loaded successfully into Engine.")
            else:
                QMessageBox.warning(self, "Failed", f"Engine failed to load profile '{profile}'.")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Error loading profile: {e}")

    def on_save_profile(self):
        profile = self.cb_profiles.currentText()
        if not profile:
            return
        if not self.client.is_connected:
            QMessageBox.warning(self, "Offline", "Connect to Engine to save profile.")
            return
        try:
            ok = self.client.save_profile(profile)
            if ok:
                QMessageBox.information(self, "Success", f"Profile '{profile}' saved successfully.")
            else:
                QMessageBox.warning(self, "Failed", f"Failed to save profile '{profile}'.")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Error saving profile: {e}")

    def on_create_profile(self):
        new_name = self.in_new_profile.text().strip()
        if not new_name:
            return
        fpath = os.path.join(self.configs_dir, f"{new_name}.json")
        if not os.path.exists(fpath):
            with open(fpath, "w", encoding="utf-8") as f:
                f.write("{\n    \"Healing\": {\"rules\": []},\n    \"Cavebot\": {\"waypoints\": []}\n}\n")
        self.in_new_profile.clear()
        self.refresh_profiles()
        self.cb_profiles.setCurrentText(new_name)

    def on_restart_engine(self):
        try:
            self.process.port = self.sp_port.value()
            self.client.port = self.sp_port.value()
            self.process.restart()
            self.client.connect()
            QMessageBox.information(self, "Restarted", "Engine process restarted.")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to restart engine: {e}")

    def on_set_target_client(self):
        proc_name = self.cb_client_proc.currentText().strip()
        if not proc_name:
            return
        if not self.client.is_connected:
            self.lbl_adapter_status.setText(f"Target set to: {proc_name} (Connect engine to apply)")
            return
        try:
            res = self.client.set_target_client(proc_name)
            self.lbl_adapter_status.setText(f"Target process set to: {proc_name}. Searching...")
            QMessageBox.information(self, "Success", f"Target client executable set to '{proc_name}'.\nEngine will automatically attach when the process starts.")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to set target client: {e}")

    def on_scan_clients(self):
        if not self.client.is_connected:
            QMessageBox.warning(self, "Offline", "Start and connect Engine first.")
            return
        try:
            status = self.client.get_client_status()
            if status and status.get("attached"):
                self.lbl_adapter_status.setText(f"● Connected to {status.get('targetProcess')} (PID: {status.get('pid')})")
                QMessageBox.information(self, "Attached", f"Client process active:\n{status.get('status')}")
            else:
                self.lbl_adapter_status.setText(f"○ {status.get('status', 'Searching...')}")
                QMessageBox.information(self, "Scan Result", f"{status.get('status', 'No matching process found yet.')}\nMake sure DBVictory.exe or Tibia.exe is running.")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Scan error: {e}")
            QMessageBox.critical(self, "Error", f"Failed to restart engine: {e}")
