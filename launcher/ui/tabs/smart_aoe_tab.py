from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
    QTableWidget, QTableWidgetItem, QHeaderView, QFrame,
    QLineEdit, QSpinBox, QMessageBox
)

class SmartAoeTab(QWidget):
    def __init__(self, engine_client, parent=None):
        super().__init__(parent)
        self.client = engine_client
        self.rules = []
        self.init_ui()

    def init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(16)

        header = QHBoxLayout()
        title = QLabel("Smart AOE Strategy (Area Spells & Runes)")
        title.setProperty("class", "sectionTitle")
        header.addWidget(title)
        header.addStretch()

        self.btn_fetch = QPushButton("Fetch from Engine")
        self.btn_fetch.clicked.connect(self.fetch_from_engine)
        header.addWidget(self.btn_fetch)

        self.btn_apply = QPushButton("Apply to Engine")
        self.btn_apply.setProperty("class", "primaryButton")
        self.btn_apply.clicked.connect(self.apply_to_engine)
        header.addWidget(self.btn_apply)
        layout.addLayout(header)

        # Table
        self.table = QTableWidget(0, 5)
        self.table.setHorizontalHeaderLabels([
            "Spell Words", "Min Monsters", "Max Distance", "Min Mana", "Cooldown"
        ])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        layout.addWidget(self.table)

        # Form
        card = QFrame()
        card.setProperty("class", "cardFrame")
        c_layout = QHBoxLayout(card)

        self.in_spell = QLineEdit()
        self.in_spell.setPlaceholderText("Spell (e.g. exori)")

        self.sp_min_monsters = QSpinBox()
        self.sp_min_monsters.setRange(1, 20)
        self.sp_min_monsters.setValue(3)

        self.sp_max_dist = QSpinBox()
        self.sp_max_dist.setRange(1, 6)
        self.sp_max_dist.setValue(1)

        self.sp_min_mana = QSpinBox()
        self.sp_min_mana.setRange(0, 5000)
        self.sp_min_mana.setValue(115)

        self.sp_cooldown = QSpinBox()
        self.sp_cooldown.setRange(500, 10000)
        self.sp_cooldown.setValue(2000)
        self.sp_cooldown.setSuffix(" ms")

        btn_add = QPushButton("Add AOE Rule")
        btn_add.setProperty("class", "successButton")
        btn_add.clicked.connect(self.on_add)

        btn_del = QPushButton("Delete Selected")
        btn_del.setProperty("class", "dangerButton")
        btn_del.clicked.connect(self.on_del)

        c_layout.addWidget(QLabel("Spell:"))
        c_layout.addWidget(self.in_spell)
        c_layout.addWidget(QLabel("Min Monsters:"))
        c_layout.addWidget(self.sp_min_monsters)
        c_layout.addWidget(QLabel("Max Distance:"))
        c_layout.addWidget(self.sp_max_dist)
        c_layout.addWidget(QLabel("Min Mana:"))
        c_layout.addWidget(self.sp_min_mana)
        c_layout.addWidget(QLabel("Cooldown:"))
        c_layout.addWidget(self.sp_cooldown)
        c_layout.addStretch()
        c_layout.addWidget(btn_add)
        c_layout.addWidget(btn_del)

        layout.addWidget(card)

    def on_add(self):
        spell = self.in_spell.text().strip() or "exori"
        rule = {
            "spell": spell,
            "minMonsters": self.sp_min_monsters.value(),
            "maxDistance": self.sp_max_dist.value(),
            "minMana": self.sp_min_mana.value(),
            "cooldownMs": self.sp_cooldown.value()
        }
        self.rules.append(rule)
        self.refresh_table()

    def on_del(self):
        row = self.table.currentRow()
        if 0 <= row < len(self.rules):
            del self.rules[row]
            self.refresh_table()

    def refresh_table(self):
        self.table.setRowCount(len(self.rules))
        for r, rule in enumerate(self.rules):
            self.table.setItem(r, 0, QTableWidgetItem(rule.get("spell", "")))
            self.table.setItem(r, 1, QTableWidgetItem(str(rule.get("minMonsters", 3))))
            self.table.setItem(r, 2, QTableWidgetItem(f"{rule.get('maxDistance', 1)} tiles"))
            self.table.setItem(r, 3, QTableWidgetItem(str(rule.get("minMana", 115))))
            self.table.setItem(r, 4, QTableWidgetItem(f"{rule.get('cooldownMs', 2000)}ms"))

    def fetch_from_engine(self):
        if not self.client.is_connected:
            return
        try:
            cfg = self.client.get_module_config("SmartAOE")
            self.rules = cfg.get("rules", [])
            self.refresh_table()
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to fetch SmartAOE: {e}")

    def apply_to_engine(self):
        if not self.client.is_connected:
            QMessageBox.warning(self, "Offline", "Engine is offline.")
            return
        try:
            self.client.set_module_config("SmartAOE", {"rules": self.rules})
            QMessageBox.information(self, "Success", "SmartAOE rules applied to Engine successfully!")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to apply SmartAOE: {e}")
