#!/usr/bin/env bash
# TibiaBot Launcher startup script
# Supports both Modern PySide6/Qt interface and native Tkinter universal interface.

cd "$(dirname "$0")"

# Check if PySide6 is available in virtualenv
if [ -f ".venv/bin/python" ] && .venv/bin/python -c "import PySide6" 2>/dev/null; then
    echo "Starting TibiaBot PySide6 Launcher..."
    .venv/bin/python launcher/main.py --auto-start "$@"
else
    echo "PySide6 not yet installed or downloading in background."
    echo "Starting TibiaBot Universal Launcher (Native GUI)..."
    python3 launcher/tk_launcher.py "$@"
fi
