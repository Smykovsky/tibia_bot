from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
    QTableWidget, QTableWidgetItem, QHeaderView, QFrame,
    QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QMessageBox
)
from PySide6.QtCore import Qt

class HealingTab(QWidget):
    def __init__(self, engine_client, parent=None):
        super().__init__(parent)
        self.client = engine_client
        self.rules = []
        self.init_ui()

    def init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(16)

        # Title Card
        header = QHBoxLayout()
        title = QLabel("Healing & Restoration Configuration")
        title.setProperty("class", "sectionTitle")
        header.addWidget(title)
        header.addStretch()

        self.btn_sync = QPushButton("Fetch from Engine")
        self.btn_sync.clicked.connect(self.fetch_from_engine)
        header.addWidget(self.btn_sync)

        self.btn_apply = QPushButton("Apply to Engine")
        self.btn_apply.setProperty("class", "primaryButton")
        self.btn_apply.clicked.connect(self.apply_to_engine)
        header.addWidget(self.btn_apply)
        layout.addLayout(header)

        # Rules Table
        self.table = QTableWidget(0, 8)
        self.table.setHorizontalHeaderLabels([
            "Name", "Trigger", "Threshold", "Action", "Value", "Min Mana", "Cooldown", "Priority"
        ])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        layout.addWidget(self.table)

        # Form to add a new rule
        form_frame = QFrame()
        form_frame.setProperty("class", "cardFrame")
        form_layout = QVBoxLayout(form_frame)

        form_title = QLabel("Add New Healing Rule")
        form_title.setProperty("class", "cardTitle")
        form_layout.addWidget(form_title)

        row1 = QHBoxLayout()
        self.in_name = QLineEdit()
        self.in_name.setPlaceholderText("Rule Name (e.g. Vita)")

        self.cb_trigger = QComboBox()
        self.cb_trigger.addItems(["hp_below", "mp_below", "poison"])

        self.sp_threshold = QDoubleSpinBox()
        self.sp_threshold.setRange(1.0, 100.0)
        self.sp_threshold.setValue(70.0)
        self.sp_threshold.setSuffix("%")

        self.cb_action = QComboBox()
        self.cb_action.addItems(["spell", "item"])

        self.in_value = QLineEdit()
        self.in_value.setPlaceholderText("Spell (exura) or Item ID (266)")

        row1.addWidget(QLabel("Name:"))
        row1.addWidget(self.in_name)
        row1.addWidget(QLabel("Trigger:"))
        row1.addWidget(self.cb_trigger)
        row1.addWidget(QLabel("Threshold:"))
        row1.addWidget(self.sp_threshold)
        row1.addWidget(QLabel("Action:"))
        row1.addWidget(self.cb_action)
        row1.addWidget(QLabel("Spell/Item:"))
        row1.addWidget(self.in_value)
        form_layout.addLayout(row1)

        row2 = QHBoxLayout()
        self.sp_min_mana = QSpinBox()
        self.sp_min_mana.setRange(0, 10000)
        self.sp_min_mana.setValue(40)

        self.sp_cooldown = QSpinBox()
        self.sp_cooldown.setRange(100, 30000)
        self.sp_cooldown.setValue(1000)
        self.sp_cooldown.setSuffix(" ms")

        self.sp_priority = QSpinBox()
        self.sp_priority.setRange(0, 10)
        self.sp_priority.setValue(1)

        btn_add = QPushButton("Add Rule")
        btn_add.setProperty("class", "successButton")
        btn_add.clicked.connect(self.on_add_rule)

        btn_del = QPushButton("Delete Selected")
        btn_del.setProperty("class", "dangerButton")
        btn_del.clicked.connect(self.on_delete_rule)

        row2.addWidget(QLabel("Min Mana:"))
        row2.addWidget(self.sp_min_mana)
        row2.addWidget(QLabel("Cooldown:"))
        row2.addWidget(self.sp_cooldown)
        row2.addWidget(QLabel("Priority:"))
        row2.addWidget(self.sp_priority)
        row2.addStretch()
        row2.addWidget(btn_add)
        row2.addWidget(btn_del)
        form_layout.addLayout(row2)

        layout.addWidget(form_frame)

    def on_add_rule(self):
        name = self.in_name.text().strip() or "Heal"
        trigger = self.cb_trigger.currentText()
        threshold = self.sp_threshold.value()
        action = self.cb_action.currentText()
        val = self.in_value.text().strip() or ("exura" if action == "spell" else "266")
        min_mana = self.sp_min_mana.value()
        cooldown = self.sp_cooldown.value()
        priority = self.sp_priority.value()

        rule = {
            "name": name,
            "enabled": True,
            "trigger": trigger,
            "thresholdPercent": threshold,
            "action": action,
            "spellWords": val if action == "spell" else "exura",
            "itemId": int(val) if action == "item" and val.isdigit() else 266,
            "minMana": min_mana,
            "cooldownMs": cooldown,
            "priority": priority
        }
        self.rules.append(rule)
        self.refresh_table()

    def on_delete_rule(self):
        row = self.table.currentRow()
        if 0 <= row < len(self.rules):
            del self.rules[row]
            self.refresh_table()

    def refresh_table(self):
        self.table.setRowCount(len(self.rules))
        for r, rule in enumerate(self.rules):
            self.table.setItem(r, 0, QTableWidgetItem(rule.get("name", "")))
            self.table.setItem(r, 1, QTableWidgetItem(rule.get("trigger", "")))
            self.table.setItem(r, 2, QTableWidgetItem(f"{rule.get('thresholdPercent', 0)}%"))
            self.table.setItem(r, 3, QTableWidgetItem(rule.get("action", "")))
            val = rule.get("spellWords") if rule.get("action") == "spell" else str(rule.get("itemId"))
            self.table.setItem(r, 4, QTableWidgetItem(val))
            self.table.setItem(r, 5, QTableWidgetItem(str(rule.get("minMana", 0))))
            self.table.setItem(r, 6, QTableWidgetItem(f"{rule.get('cooldownMs', 1000)}ms"))
            self.table.setItem(r, 7, QTableWidgetItem(str(rule.get("priority", 1))))

    def fetch_from_engine(self):
        if not self.client.is_connected:
            return
        try:
            cfg = self.client.get_module_config("Healing")
            self.rules = cfg.get("rules", [])
            self.refresh_table()
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to fetch Healing config: {e}")

    def apply_to_engine(self):
        if not self.client.is_connected:
            QMessageBox.warning(self, "Offline", "Engine is offline. Start engine first.")
            return
        try:
            self.client.set_module_config("Healing", {"rules": self.rules})
            QMessageBox.information(self, "Success", "Healing rules applied to Engine successfully!")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to apply Healing config: {e}")
