@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo    TibiaBot - One-Click Single Executable (.exe) Builder
echo ========================================================

:: 1. Check Python
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python not found in PATH! Please install Python 3.9+ and add to PATH.
    pause
    exit /b 1
)

:: 2. Check CMake
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found in PATH! Please install CMake.
    pause
    exit /b 1
)

:: 3. Build C++ Engine
echo.
echo [1/4] Building C++ Engine (Release mode)...
if not exist "engine\build" mkdir "engine\build"
cd engine\build

cmake -DCMAKE_BUILD_TYPE=Release ..
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed!
    cd ..\..
    pause
    exit /b 1
)

cmake --build . --config Release --parallel
if %errorlevel% neq 0 (
    echo [ERROR] C++ Engine build failed!
    cd ..\..
    pause
    exit /b 1
)
cd ..\..

:: Verify engine binary exists
if not exist "engine\build\Release\tibia_engine.exe" (
    if not exist "engine\build\tibia_engine.exe" (
        echo [ERROR] Could not locate tibia_engine.exe!
        pause
        exit /b 1
    )
)
echo [OK] C++ Engine compiled successfully!

:: 4. Setup Python dependencies
echo.
echo [2/4] Installing Python requirements (PySide6, PyInstaller)...
python -m pip install --upgrade pip
python -m pip install pyinstaller PySide6-Essentials

:: 5. Package into standalone .exe with PyInstaller
echo.
echo [3/4] Packaging Launcher and C++ Engine into single TibiaBot.exe...
pyinstaller TibiaBot.spec --clean --noconfirm

if %errorlevel% neq 0 (
    echo [ERROR] PyInstaller packaging failed!
    pause
    exit /b 1
)

:: 6. Ensure configs and scripts are in dist folder
echo.
echo [4/4] Finalizing distribution package...
if not exist "dist\configs" xcopy "configs" "dist\configs" /E /I /Y >nul
if not exist "dist\scripts" xcopy "scripts" "dist\scripts" /E /I /Y >nul

echo.
echo ========================================================
echo [SUCCESS] Standalone Executable created successfully!
echo Binary path: %cd%\dist\TibiaBot.exe
echo ========================================================
echo.
echo The user can now simply double click TibiaBot.exe to run!
pause
