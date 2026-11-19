from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
    QTableWidget, QTableWidgetItem, QHeaderView, QFrame,
    QLineEdit, QSpinBox, QComboBox, QMessageBox
)

class TargetingTab(QWidget):
    def __init__(self, engine_client, parent=None):
        super().__init__(parent)
        self.client = engine_client
        self.targets = []
        self.init_ui()

    def init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(16)

        header = QHBoxLayout()
        title = QLabel("Targeting & Combat Strategy")
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
            "Monster Name", "Priority", "Combat Stance", "Distance", "Attack Spells"
        ])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        layout.addWidget(self.table)

        # Form
        card = QFrame()
        card.setProperty("class", "cardFrame")
        c_layout = QVBoxLayout(card)

        card_title = QLabel("Add Target Rule")
        card_title.setProperty("class", "cardTitle")
        c_layout.addWidget(card_title)

        row1 = QHBoxLayout()
        self.in_name = QLineEdit()
        self.in_name.setPlaceholderText("Monster Name (e.g. Dragon, or * for all)")

        self.sp_prio = QSpinBox()
        self.sp_prio.setRange(1, 10)
        self.sp_prio.setValue(1)

        self.cb_stance = QComboBox()
        self.cb_stance.addItems(["chase", "keep_distance", "stand"])

        self.sp_dist = QSpinBox()
        self.sp_dist.setRange(1, 8)
        self.sp_dist.setValue(3)

        self.in_spells = QLineEdit()
        self.in_spells.setPlaceholderText("Spells comma-separated (e.g. exori, exori hur)")

        row1.addWidget(QLabel("Name:"))
        row1.addWidget(self.in_name)
        row1.addWidget(QLabel("Priority:"))
        row1.addWidget(self.sp_prio)
        row1.addWidget(QLabel("Stance:"))
        row1.addWidget(self.cb_stance)
        row1.addWidget(QLabel("Dist:"))
        row1.addWidget(self.sp_dist)
        row1.addWidget(QLabel("Spells:"))
        row1.addWidget(self.in_spells)
        c_layout.addLayout(row1)

        row2 = QHBoxLayout()
        btn_add = QPushButton("Add Target")
        btn_add.setProperty("class", "successButton")
        btn_add.clicked.connect(self.on_add)

        btn_del = QPushButton("Delete Selected")
        btn_del.setProperty("class", "dangerButton")
        btn_del.clicked.connect(self.on_del)

        row2.addStretch()
        row2.addWidget(btn_add)
        row2.addWidget(btn_del)
        c_layout.addLayout(row2)

        layout.addWidget(card)

    def on_add(self):
        name = self.in_name.text().strip() or "*"
        spells_str = self.in_spells.text().strip()
        spells = [s.strip() for s in spells_str.split(",") if s.strip()]

        target = {
            "name": name,
            "priority": self.sp_prio.value(),
            "stance": self.cb_stance.currentText(),
            "desiredDistance": self.sp_dist.value(),
            "spells": spells
        }
        self.targets.append(target)
        self.refresh_table()

    def on_del(self):
        row = self.table.currentRow()
        if 0 <= row < len(self.targets):
            del self.targets[row]
            self.refresh_table()

    def refresh_table(self):
        self.table.setRowCount(len(self.targets))
        for r, t in enumerate(self.targets):
            self.table.setItem(r, 0, QTableWidgetItem(t.get("name", "*")))
            self.table.setItem(r, 1, QTableWidgetItem(str(t.get("priority", 1))))
            self.table.setItem(r, 2, QTableWidgetItem(t.get("stance", "chase").upper()))
            self.table.setItem(r, 3, QTableWidgetItem(str(t.get("desiredDistance", 3))))
            self.table.setItem(r, 4, QTableWidgetItem(", ".join(t.get("spells", []))))

    def fetch_from_engine(self):
        if not self.client.is_connected:
            return
        try:
            cfg = self.client.get_module_config("Targeting")
            self.targets = cfg.get("targets", [])
            self.refresh_table()
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to fetch Targeting: {e}")

    def apply_to_engine(self):
        if not self.client.is_connected:
            QMessageBox.warning(self, "Offline", "Engine is offline.")
            return
        try:
            self.client.set_module_config("Targeting", {"targets": self.targets})
            QMessageBox.information(self, "Success", "Targeting rules applied to Engine successfully!")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to apply targeting: {e}")
