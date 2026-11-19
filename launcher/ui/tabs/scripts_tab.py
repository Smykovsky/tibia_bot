import os
import sys
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
    QListWidget, QPlainTextEdit, QFrame, QLineEdit, QMessageBox, QSplitter
)
from PySide6.QtCore import Qt

class ScriptsTab(QWidget):
    def __init__(self, engine_client, parent=None):
        super().__init__(parent)
        self.client = engine_client
        if getattr(sys, 'frozen', False):
            self.scripts_dir = os.path.join(os.path.dirname(sys.executable), "scripts")
        else:
            self.scripts_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", "scripts"))
        self.current_file = None
        self.init_ui()
        self.reload_file_list()

    def init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(16)

        # Header
        header = QHBoxLayout()
        title = QLabel("Lua Scripting Studio & Runner")
        title.setProperty("class", "sectionTitle")
        header.addWidget(title)
        header.addStretch()

        self.btn_run = QPushButton("Run Script in Engine")
        self.btn_run.setProperty("class", "primaryButton")
        self.btn_run.clicked.connect(self.on_run_script)
        header.addWidget(self.btn_run)
        layout.addLayout(header)

        # Splitter: Left (Files) & Right (Editor + Console)
        splitter = QSplitter(Qt.Orientation.Horizontal)

        # Left File List Card
        left_card = QFrame()
        left_card.setProperty("class", "cardFrame")
        left_layout = QVBoxLayout(left_card)

        left_title = QLabel("Available Scripts")
        left_title.setProperty("class", "cardTitle")
        left_layout.addWidget(left_title)

        self.file_list = QListWidget()
        self.file_list.itemClicked.connect(self.on_file_selected)
        left_layout.addWidget(self.file_list)

        new_row = QHBoxLayout()
        self.in_new_name = QLineEdit()
        self.in_new_name.setPlaceholderText("new_script.lua")
        btn_new = QPushButton("Create")
        btn_new.clicked.connect(self.on_create_file)
        new_row.addWidget(self.in_new_name)
        new_row.addWidget(btn_new)
        left_layout.addLayout(new_row)

        splitter.addWidget(left_card)

        # Right Editor & Output
        right_card = QFrame()
        right_card.setProperty("class", "cardFrame")
        right_layout = QVBoxLayout(right_card)

        ed_header = QHBoxLayout()
        self.lbl_editing = QLabel("Editing: (No file selected)")
        self.lbl_editing.setStyleSheet("font-weight: 600; color: #38bdf8;")
        ed_header.addWidget(self.lbl_editing)
        ed_header.addStretch()

        btn_save = QPushButton("Save File")
        btn_save.clicked.connect(self.on_save_file)
        ed_header.addWidget(btn_save)
        right_layout.addLayout(ed_header)

        self.editor = QPlainTextEdit()
        self.editor.setPlaceholderText("-- Write your custom Lua script here...\nlocal p = Game.getPlayer()\nBot.log('HP: ' .. p.hp)")
        right_layout.addWidget(self.editor, stretch=3)

        out_title = QLabel("Execution Output")
        out_title.setStyleSheet("font-weight: 600; color: #94a3b8;")
        right_layout.addWidget(out_title)

        self.console_output = QPlainTextEdit()
        self.console_output.setReadOnly(True)
        self.console_output.setMaximumBlockCount(200)
        right_layout.addWidget(self.console_output, stretch=1)

        splitter.addWidget(right_card)
        splitter.setSizes([250, 750])

        layout.addWidget(splitter)

    def reload_file_list(self):
        self.file_list.clear()
        if not os.path.exists(self.scripts_dir):
            os.makedirs(self.scripts_dir, exist_ok=True)

        for fname in sorted(os.listdir(self.scripts_dir)):
            if fname.endswith(".lua"):
                self.file_list.addItem(fname)

    def on_file_selected(self, item):
        filename = item.text()
        filepath = os.path.join(self.scripts_dir, filename)
        if os.path.exists(filepath):
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()
            self.current_file = filepath
            self.lbl_editing.setText(f"Editing: {filename}")
            self.editor.setPlainText(content)

    def on_create_file(self):
        fname = self.in_new_name.text().strip()
        if not fname:
            return
        if not fname.endswith(".lua"):
            fname += ".lua"

        fpath = os.path.join(self.scripts_dir, fname)
        if not os.path.exists(fpath):
            with open(fpath, "w", encoding="utf-8") as f:
                f.write("-- New script\nlocal p = Game.getPlayer()\nBot.log('Running ' .. p.name)\n")
        self.in_new_name.clear()
        self.reload_file_list()

    def on_save_file(self):
        if not self.current_file:
            QMessageBox.warning(self, "No File", "Select or create a file first.")
            return
        with open(self.current_file, "w", encoding="utf-8") as f:
            f.write(self.editor.toPlainText())
        self.console_output.appendPlainText(f"[Saved] {os.path.basename(self.current_file)}")

    def on_run_script(self):
        code = self.editor.toPlainText().strip()
        if not code:
            return
        if not self.client.is_connected:
            self.console_output.appendPlainText("[Error] Engine is offline. Connect engine first.")
            return

        try:
            ok = self.client.execute_lua(code)
            if ok:
                self.console_output.appendPlainText("[Success] Script executed in Engine.")
            else:
                self.console_output.appendPlainText("[Error] Lua execution failed.")
        except Exception as e:
            self.console_output.appendPlainText(f"[Exception] {e}")
