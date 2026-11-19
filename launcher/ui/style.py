DARK_THEME_QSS = """
/* Global Window Style */
QMainWindow, QDialog {
    background-color: #0b0f19;
    color: #e2e8f0;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
    font-size: 13px;
}

QWidget {
    color: #e2e8f0;
    outline: none;
}

/* Sidebar Navigation */
QListWidget#navSidebar {
    background-color: #0f172a;
    border: none;
    border-right: 1px solid #1e293b;
    padding-top: 15px;
}

QListWidget#navSidebar::item {
    height: 44px;
    padding-left: 18px;
    margin: 3px 10px;
    border-radius: 8px;
    color: #94a3b8;
    font-size: 13px;
    font-weight: 500;
}

QListWidget#navSidebar::item:hover {
    background-color: #1e293b;
    color: #f8fafc;
}

QListWidget#navSidebar::item:selected {
    background-color: #3b82f6;
    color: #ffffff;
    font-weight: 600;
}

/* Header & Cards */
QFrame.cardFrame {
    background-color: #131c31;
    border: 1px solid #1e293b;
    border-radius: 10px;
    padding: 16px;
}

QLabel.cardTitle {
    font-size: 15px;
    font-weight: 700;
    color: #f1f5f9;
    margin-bottom: 8px;
}

QLabel.sectionTitle {
    font-size: 17px;
    font-weight: 700;
    color: #38bdf8;
    margin-bottom: 12px;
}

QLabel.statValue {
    font-size: 20px;
    font-weight: 700;
    color: #10b981;
}

QLabel.statLabel {
    font-size: 12px;
    color: #94a3b8;
}

/* Buttons */
QPushButton {
    background-color: #1e293b;
    color: #f8fafc;
    border: 1px solid #334155;
    border-radius: 6px;
    padding: 7px 16px;
    font-size: 12px;
    font-weight: 600;
}

QPushButton:hover {
    background-color: #334155;
    border-color: #475569;
}

QPushButton:pressed {
    background-color: #0f172a;
}

QPushButton.primaryButton {
    background-color: #2563eb;
    border: 1px solid #3b82f6;
    color: #ffffff;
}

QPushButton.primaryButton:hover {
    background-color: #1d4ed8;
}

QPushButton.successButton {
    background-color: #059669;
    border: 1px solid #10b981;
    color: #ffffff;
}

QPushButton.successButton:hover {
    background-color: #047857;
}

QPushButton.dangerButton {
    background-color: #dc2626;
    border: 1px solid #ef4444;
    color: #ffffff;
}

QPushButton.dangerButton:hover {
    background-color: #b91c1c;
}

/* Inputs & Form Controls */
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background-color: #0f172a;
    border: 1px solid #334155;
    border-radius: 6px;
    color: #f8fafc;
    padding: 6px 10px;
    font-size: 12px;
}

QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
    border: 1px solid #38bdf8;
    background-color: #1e293b;
}

QComboBox::drop-down {
    border: none;
    padding-right: 8px;
}

/* Progress Bars */
QProgressBar {
    background-color: #0f172a;
    border: 1px solid #1e293b;
    border-radius: 5px;
    text-align: center;
    font-size: 11px;
    font-weight: 700;
    color: #ffffff;
    height: 18px;
}

QProgressBar#hpBar::chunk {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #dc2626, stop:1 #10b981);
    border-radius: 4px;
}

QProgressBar#mpBar::chunk {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1d4ed8, stop:1 #38bdf8);
    border-radius: 4px;
}

/* Tables */
QTableWidget {
    background-color: #0f172a;
    border: 1px solid #1e293b;
    border-radius: 8px;
    gridline-color: #1e293b;
    selection-background-color: #1e3a8a;
    selection-color: #ffffff;
}

QHeaderView::section {
    background-color: #131c31;
    color: #94a3b8;
    border: none;
    border-bottom: 1px solid #1e293b;
    padding: 8px;
    font-weight: 600;
    font-size: 11px;
    text-transform: uppercase;
}

QTableWidget::item {
    padding: 6px;
    border-bottom: 1px solid #141e33;
}

/* Checkboxes */
QCheckBox {
    spacing: 8px;
    font-size: 13px;
    color: #e2e8f0;
}

QCheckBox::indicator {
    width: 18px;
    height: 18px;
    border: 1px solid #475569;
    border-radius: 4px;
    background-color: #0f172a;
}

QCheckBox::indicator:hover {
    border-color: #38bdf8;
}

QCheckBox::indicator:checked {
    background-color: #10b981;
    border-color: #10b981;
}

/* Text Editors & Logs */
QPlainTextEdit, QTextEdit {
    background-color: #090d16;
    border: 1px solid #1e293b;
    border-radius: 8px;
    color: #38bdf8;
    font-family: "JetBrains Mono", "Courier New", monospace;
    font-size: 12px;
    padding: 10px;
}

/* Scrollbars */
QScrollBar:vertical {
    border: none;
    background: #0f172a;
    width: 8px;
    margin: 0;
}

QScrollBar::handle:vertical {
    background: #334155;
    min-height: 20px;
    border-radius: 4px;
}

QScrollBar::handle:vertical:hover {
    background: #475569;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}
"""
