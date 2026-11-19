# -*- mode: python ; coding: utf-8 -*-
import os
import sys

block_cipher = None

# In PyInstaller .spec files, __file__ is not defined. We use os.getcwd() or SPECPATH.
project_root = os.path.abspath(SPECPATH if 'SPECPATH' in globals() else os.getcwd())

# Data files to embed in bundle
datas = [
    (os.path.join(project_root, 'configs'), 'configs'),
    (os.path.join(project_root, 'scripts'), 'scripts'),
]

# Binaries to embed in bundle (C++ Engine)
binaries = []

engine_exe = os.path.join(project_root, 'engine', 'build', 'Release', 'tibia_engine.exe')
if not os.path.exists(engine_exe):
    engine_exe = os.path.join(project_root, 'engine', 'build', 'tibia_engine.exe')
if not os.path.exists(engine_exe):
    engine_exe = os.path.join(project_root, 'engine', 'build', 'tibia_engine')

if os.path.exists(engine_exe):
    # Place executable at root of unpacked bundle
    binaries.append((engine_exe, '.'))
else:
    print(f"WARNING: C++ Engine binary not found at {engine_exe}! Build C++ engine first.")

a = Analysis(
    [os.path.join(project_root, 'launcher', 'main.py')],
    pathex=[project_root],
    binaries=binaries,
    datas=datas,
    hiddenimports=[
        'PySide6',
        'PySide6.QtCore',
        'PySide6.QtGui',
        'PySide6.QtWidgets',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=['tkinter'],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='TibiaBot',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=None
)
