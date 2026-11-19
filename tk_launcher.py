#!/usr/bin/env python3
"""
TibiaBot – Root launcher forwarder.
Automatically launches the modern PySide6 Launcher from .venv.
"""
import os
import sys

_root_dir = os.path.dirname(os.path.abspath(__file__))
_venv_python = os.path.join(_root_dir, ".venv", "bin", "python")
_main_py = os.path.join(_root_dir, "launcher", "main.py")

# Prefer modern PySide6 Qt launcher
if "--tk" not in sys.argv and os.path.exists(_venv_python) and os.path.exists(_main_py):
    _args = [a for a in sys.argv[1:] if a != "--tk"]
    os.execv(_venv_python, [_venv_python, _main_py, "--auto-start"] + _args)

# Fallback to Tkinter
tk_path = os.path.join(_root_dir, "launcher", "tk_launcher.py")
if os.path.exists(tk_path):
    os.execv(sys.executable, [sys.executable, tk_path] + sys.argv[1:])
else:
    print(f"Error: {tk_path} not found.")
    sys.exit(1)
