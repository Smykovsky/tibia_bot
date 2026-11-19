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

# Binaries to embed in bundle (C++ Engine & runtime DLLs)
binaries = []

engine_candidates = [
    os.path.join(project_root, 'engine', 'build', 'Release', 'tibia_engine.exe'),
    os.path.join(project_root, 'engine', 'build', 'tibia_engine.exe'),
    os.path.join(project_root, 'engine', 'build', 'tibia_engine'),
]
engine_exe = next((c for c in engine_candidates if os.path.exists(c)), None)

if engine_exe:
    # Place executable at root of unpacked bundle
    binaries.append((engine_exe, '.'))
    # Collect all DLLs alongside engine_exe (e.g. lua.dll)
    engine_dir = os.path.dirname(engine_exe)
    for fname in os.listdir(engine_dir):
        if fname.lower().endswith('.dll'):
            binaries.append((os.path.join(engine_dir, fname), '.'))
else:
    print(f"WARNING: C++ Engine binary not found! Searched in: {engine_candidates}")

# Also check vcpkg installed directory for DLLs in CI
vcpkg_root = os.environ.get('VCPKG_INSTALLATION_ROOT')
if vcpkg_root:
    vcpkg_bin = os.path.join(vcpkg_root, 'installed', 'x64-windows', 'bin')
    if os.path.exists(vcpkg_bin):
        for fname in os.listdir(vcpkg_bin):
            if fname.lower().endswith('.dll'):
                dll_path = os.path.join(vcpkg_bin, fname)
                if not any(b[0] == dll_path for b in binaries):
                    binaries.append((dll_path, '.'))

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
