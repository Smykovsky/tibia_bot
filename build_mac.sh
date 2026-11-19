#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

echo "========================================================"
echo "   TibiaBot - Standalone Bundle Builder (macOS)"
echo "========================================================"

echo "[1/3] Compiling C++ Engine..."
cmake -B engine/build -S engine -DCMAKE_BUILD_TYPE=Release
cmake --build engine/build --config Release --parallel

echo "[2/3] Installing PyInstaller in .venv..."
.venv/bin/pip install -q pyinstaller

echo "[3/3] Packaging into standalone binary..."
.venv/bin/pyinstaller TibiaBot.spec --clean --noconfirm

mkdir -p dist/configs dist/scripts
cp -r configs/* dist/configs/ 2>/dev/null || true
cp -r scripts/* dist/scripts/ 2>/dev/null || true

echo "========================================================"
echo "[SUCCESS] Standalone app created at dist/TibiaBot!"
echo "Run it with: ./dist/TibiaBot"
echo "========================================================"
