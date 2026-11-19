from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
    QTableWidget, QTableWidgetItem, QHeaderView, QFrame,
    QLineEdit, QSpinBox, QComboBox, QMessageBox
)

class CavebotTab(QWidget):
    def __init__(self, engine_client, parent=None):
        super().__init__(parent)
        self.client = engine_client
        self.waypoints = []
        self.init_ui()

    def init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(16)

        # Header
        header = QHBoxLayout()
        title = QLabel("Cavebot & Waypoint Navigation")
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
        self.table = QTableWidget(0, 7)
        self.table.setHorizontalHeaderLabels([
            "#", "Type", "X", "Y", "Z", "Label", "Tolerance / Script"
        ])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        layout.addWidget(self.table)

        # Control & Form Card
        card = QFrame()
        card.setProperty("class", "cardFrame")
        c_layout = QVBoxLayout(card)

        row1 = QHBoxLayout()
        self.cb_type = QComboBox()
        self.cb_type.addItems(["walk", "node", "stand", "rope", "ladder", "shovel", "action"])

        self.sp_x = QSpinBox()
        self.sp_x.setRange(0, 100000)
        self.sp_x.setValue(32300)

        self.sp_y = QSpinBox()
        self.sp_y.setRange(0, 100000)
        self.sp_y.setValue(32200)

        self.sp_z = QSpinBox()
        self.sp_z.setRange(0, 15)
        self.sp_z.setValue(7)

        self.in_label = QLineEdit()
        self.in_label.setPlaceholderText("Label (optional)")

        self.in_extra = QLineEdit()
        self.in_extra.setPlaceholderText("Script / Tolerance")

        row1.addWidget(QLabel("Type:"))
        row1.addWidget(self.cb_type)
        row1.addWidget(QLabel("X:"))
        row1.addWidget(self.sp_x)
        row1.addWidget(QLabel("Y:"))
        row1.addWidget(self.sp_y)
        row1.addWidget(QLabel("Z:"))
        row1.addWidget(self.sp_z)
        row1.addWidget(QLabel("Label:"))
        row1.addWidget(self.in_label)
        row1.addWidget(self.in_extra)
        c_layout.addLayout(row1)

        row2 = QHBoxLayout()
        btn_add = QPushButton("Add Waypoint")
        btn_add.setProperty("class", "successButton")
        btn_add.clicked.connect(self.on_add_waypoint)

        btn_add_cur = QPushButton("Add Current Pos")
        btn_add_cur.clicked.connect(self.on_add_current_pos)

        btn_up = QPushButton("Move Up")
        btn_up.clicked.connect(self.on_move_up)

        btn_down = QPushButton("Move Down")
        btn_down.clicked.connect(self.on_move_down)

        btn_del = QPushButton("Delete Selected")
        btn_del.setProperty("class", "dangerButton")
        btn_del.clicked.connect(self.on_delete)

        btn_clear = QPushButton("Clear All")
        btn_clear.clicked.connect(self.on_clear)

        row2.addWidget(btn_add)
        row2.addWidget(btn_add_cur)
        row2.addWidget(btn_up)
        row2.addWidget(btn_down)
        row2.addStretch()
        row2.addWidget(btn_del)
        row2.addWidget(btn_clear)
        c_layout.addLayout(row2)

        layout.addWidget(card)

    def on_add_waypoint(self):
        wp = {
            "type": self.cb_type.currentText(),
            "x": self.sp_x.value(),
            "y": self.sp_y.value(),
            "z": self.sp_z.value(),
            "label": self.in_label.text().strip(),
            "script": self.in_extra.text().strip() if self.cb_type.currentText() == "action" else "",
            "tolerance": int(self.in_extra.text().strip()) if self.in_extra.text().strip().isdigit() else 1
        }
        self.waypoints.append(wp)
        self.refresh_table()

    def on_add_current_pos(self):
        if not self.client.is_connected:
            QMessageBox.warning(self, "Offline", "Connect to Engine to read current player position.")
            return
        try:
            state = self.client.get_game_state()
            pos = state.get("player", {}).get("position", {})
            self.sp_x.setValue(pos.get("x", 0))
            self.sp_y.setValue(pos.get("y", 0))
            self.sp_z.setValue(pos.get("z", 7))
            self.on_add_waypoint()
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to get position: {e}")

    def on_delete(self):
        row = self.table.currentRow()
        if 0 <= row < len(self.waypoints):
            del self.waypoints[row]
            self.refresh_table()

    def on_move_up(self):
        row = self.table.currentRow()
        if row > 0:
            self.waypoints[row], self.waypoints[row-1] = self.waypoints[row-1], self.waypoints[row]
            self.refresh_table()
            self.table.selectRow(row-1)

    def on_move_down(self):
        row = self.table.currentRow()
        if 0 <= row < len(self.waypoints) - 1:
            self.waypoints[row], self.waypoints[row+1] = self.waypoints[row+1], self.waypoints[row]
            self.refresh_table()
            self.table.selectRow(row+1)

    def on_clear(self):
        self.waypoints.clear()
        self.refresh_table()

    def refresh_table(self):
        self.table.setRowCount(len(self.waypoints))
        for r, wp in enumerate(self.waypoints):
            self.table.setItem(r, 0, QTableWidgetItem(str(r + 1)))
            self.table.setItem(r, 1, QTableWidgetItem(wp.get("type", "walk").upper()))
            self.table.setItem(r, 2, QTableWidgetItem(str(wp.get("x", 0))))
            self.table.setItem(r, 3, QTableWidgetItem(str(wp.get("y", 0))))
            self.table.setItem(r, 4, QTableWidgetItem(str(wp.get("z", 7))))
            self.table.setItem(r, 5, QTableWidgetItem(wp.get("label", "")))
            extra = wp.get("script") if wp.get("type") == "action" else f"Tol: {wp.get('tolerance', 1)}"
            self.table.setItem(r, 6, QTableWidgetItem(extra))

    def fetch_from_engine(self):
        if not self.client.is_connected:
            return
        try:
            cfg = self.client.get_module_config("Cavebot")
            self.waypoints = cfg.get("waypoints", [])
            self.refresh_table()
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to fetch Cavebot waypoints: {e}")

    def apply_to_engine(self):
        if not self.client.is_connected:
            QMessageBox.warning(self, "Offline", "Engine is offline. Start engine first.")
            return
        try:
            self.client.set_module_config("Cavebot", {"waypoints": self.waypoints})
            QMessageBox.information(self, "Success", "Waypoints applied to Engine successfully!")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to apply waypoints: {e}")
