from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
    QTableWidget, QTableWidgetItem, QHeaderView, QFrame,
    QLineEdit, QSpinBox, QMessageBox
)

class LootingTab(QWidget):
    def __init__(self, engine_client, parent=None):
        super().__init__(parent)
        self.client = engine_client
        self.items = []
        self.init_ui()

    def init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(16)

        header = QHBoxLayout()
        title = QLabel("Looting & Container Management")
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

        # Quick Add Common Items Bar
        quick_bar = QHBoxLayout()
        quick_label = QLabel("Quick Add:")
        quick_label.setStyleSheet("font-weight: 600; color: #94a3b8;")
        quick_bar.addWidget(quick_label)

        presets = [
            ("Gold Coin", 3031),
            ("Platinum Coin", 3035),
            ("Crystal Coin", 3043),
            ("Small Diamond", 3028),
            ("Small Ruby", 3030),
            ("Dragon Hammer", 3322)
        ]
        for name, item_id in presets:
            btn = QPushButton(name)
            btn.clicked.connect(lambda _, n=name, i=item_id: self.add_item(i, n, 0))
            quick_bar.addWidget(btn)
        quick_bar.addStretch()
        layout.addLayout(quick_bar)

        # Table
        self.table = QTableWidget(0, 3)
        self.table.setHorizontalHeaderLabels([
            "Item ID", "Item Name", "Destination Container"
        ])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        layout.addWidget(self.table)

        # Add Card
        card = QFrame()
        card.setProperty("class", "cardFrame")
        c_layout = QHBoxLayout(card)

        self.sp_id = QSpinBox()
        self.sp_id.setRange(1, 100000)
        self.sp_id.setValue(3031)

        self.in_name = QLineEdit()
        self.in_name.setPlaceholderText("Item Name (e.g. Gold Coin)")

        self.sp_dest = QSpinBox()
        self.sp_dest.setRange(0, 16)
        self.sp_dest.setValue(0)

        btn_add = QPushButton("Add Item")
        btn_add.setProperty("class", "successButton")
        btn_add.clicked.connect(lambda: self.add_item(self.sp_id.value(), self.in_name.text(), self.sp_dest.value()))

        btn_del = QPushButton("Delete Selected")
        btn_del.setProperty("class", "dangerButton")
        btn_del.clicked.connect(self.on_del)

        c_layout.addWidget(QLabel("ID:"))
        c_layout.addWidget(self.sp_id)
        c_layout.addWidget(QLabel("Name:"))
        c_layout.addWidget(self.in_name)
        c_layout.addWidget(QLabel("Dest Container:"))
        c_layout.addWidget(self.sp_dest)
        c_layout.addStretch()
        c_layout.addWidget(btn_add)
        c_layout.addWidget(btn_del)

        layout.addWidget(card)

    def add_item(self, item_id: int, name: str, dest_container: int):
        clean_name = name.strip() or f"Item #{item_id}"
        # check duplicate
        for it in self.items:
            if it.get("id") == item_id:
                return
        self.items.append({"id": item_id, "name": clean_name, "destContainer": dest_container})
        self.refresh_table()

    def on_del(self):
        row = self.table.currentRow()
        if 0 <= row < len(self.items):
            del self.items[row]
            self.refresh_table()

    def refresh_table(self):
        self.table.setRowCount(len(self.items))
        for r, it in enumerate(self.items):
            self.table.setItem(r, 0, QTableWidgetItem(str(it.get("id", 0))))
            self.table.setItem(r, 1, QTableWidgetItem(it.get("name", "")))
            self.table.setItem(r, 2, QTableWidgetItem(f"Backpack #{it.get('destContainer', 0)}"))

    def fetch_from_engine(self):
        if not self.client.is_connected:
            return
        try:
            cfg = self.client.get_module_config("Looting")
            self.items = cfg.get("items", [])
            self.refresh_table()
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to fetch Looting: {e}")

    def apply_to_engine(self):
        if not self.client.is_connected:
            QMessageBox.warning(self, "Offline", "Engine is offline.")
            return
        try:
            self.client.set_module_config("Looting", {"items": self.items})
            QMessageBox.information(self, "Success", "Looting list applied to Engine successfully!")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to apply looting: {e}")
