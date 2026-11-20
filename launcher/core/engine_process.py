import subprocess
import os
import sys
import signal
import time
import threading
from typing import Optional, Callable

class EngineProcessManager:
    def __init__(self, binary_path: Optional[str] = None, port: int = 8765):
        self.port = port
        self.proc: Optional[subprocess.Popen] = None
        self._monitor_thread: Optional[threading.Thread] = None
        self._running = False
        self.on_crash_callback: Optional[Callable[[int], None]] = None
        self.on_output_callback: Optional[Callable[[str, str], None]] = None

        if binary_path is None:
            self.binary_path = self._resolve_engine_binary()
        else:
            self.binary_path = binary_path

    def kill_orphaned_engines(self):
        if os.name == "nt":
            try:
                subprocess.run(
                    ["taskkill", "/F", "/IM", "tibia_engine.exe"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    timeout=2
                )
            except:
                pass

    def _resolve_engine_binary(self) -> str:
        exe_names = ["tibia_engine.exe", "tibia_engine"] if os.name == "nt" else ["tibia_engine", "tibia_engine.exe"]
        candidates = []

        if getattr(sys, 'frozen', False):
            # Running inside PyInstaller bundled .exe
            exe_dir = os.path.dirname(sys.executable)
            base_dir = getattr(sys, '_MEIPASS', exe_dir)
            for name in exe_names:
                candidates.extend([
                    os.path.join(exe_dir, name),          # 1. Extracted release folder
                    os.path.join(base_dir, name),         # 2. PyInstaller temp bundle
                    os.path.join(exe_dir, "engine", name),
                    os.path.join(base_dir, "engine", name),
                    os.path.join(exe_dir, "dist", name),
                    os.path.join(base_dir, "dist", name),
                ])
        else:
            root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
            for name in exe_names:
                candidates.extend([
                    os.path.join(root_dir, "engine", "build", "Release", name),
                    os.path.join(root_dir, "engine", "build", name),
                    os.path.join(root_dir, "engine", "build", "Debug", name),
                    os.path.join(root_dir, name),
                    os.path.join(root_dir, "dist", name),
                ])

        for c in candidates:
            if os.path.exists(c):
                return c
        return candidates[0] if candidates else "tibia_engine.exe"

    def is_running(self) -> bool:
        if self.proc is None:
            return False
        return self.proc.poll() is None

    def start(self) -> bool:
        if self.is_running():
            return True

        # Kill any zombie engine instances holding port 8765
        self.kill_orphaned_engines()

        # Refresh resolution in case binary was placed next to exe
        if not os.path.exists(self.binary_path):
            self.binary_path = self._resolve_engine_binary()

        if not os.path.exists(self.binary_path):
            raise FileNotFoundError(
                f"Engine executable not found at: {self.binary_path}\n"
                f"Make sure tibia_engine.exe is located in the same directory as TibiaBot.exe."
            )

        if getattr(sys, 'frozen', False):
            exe_dir = os.path.dirname(sys.executable)
            base_dir = getattr(sys, '_MEIPASS', exe_dir)
            if os.path.exists(os.path.join(exe_dir, "configs")):
                workdir = exe_dir
            elif os.path.exists(os.path.join(base_dir, "configs")):
                workdir = base_dir
            else:
                workdir = exe_dir
        else:
            workdir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

        cmd = [self.binary_path, "--port", str(self.port)]

        env = os.environ.copy()
        bin_dir = os.path.dirname(self.binary_path)
        meipass = getattr(sys, '_MEIPASS', '')
        env["PATH"] = bin_dir + (os.pathsep + meipass if meipass else "") + os.pathsep + env.get("PATH", "")

        creationflags = 0
        if os.name == "nt":
            creationflags = getattr(subprocess, 'CREATE_NO_WINDOW', 0)

        self.proc = subprocess.Popen(
            cmd,
            cwd=workdir,
            env=env,
            creationflags=creationflags,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        self._running = True

        # Stream stdout and stderr in background threads to avoid blocking pipe buffer
        def _read_stream(stream, prefix):
            try:
                for line in iter(stream.readline, ''):
                    s = line.strip()
                    if s and self.on_output_callback:
                        self.on_output_callback(prefix, s)
            except:
                pass
            finally:
                try:
                    stream.close()
                except:
                    pass

        threading.Thread(target=_read_stream, args=(self.proc.stdout, "Engine"), daemon=True).start()
        threading.Thread(target=_read_stream, args=(self.proc.stderr, "Engine-Err"), daemon=True).start()

        self._monitor_thread = threading.Thread(target=self._monitor_loop, daemon=True)
        self._monitor_thread.start()

        # Give it a moment to bind socket
        time.sleep(0.5)
        return self.is_running()

    def stop(self, timeout: float = 3.0):
        self._running = False
        if self.proc and self.is_running():
            try:
                self.proc.send_signal(signal.SIGINT)
                self.proc.wait(timeout=timeout)
            except Exception:
                try:
                    self.proc.kill()
                except:
                    pass
        self.proc = None

    def restart(self) -> bool:
        self.stop()
        time.sleep(0.5)
        return self.start()

    def _monitor_loop(self):
        if not self.proc:
            return
        code = self.proc.wait()
        if self.on_crash_callback:
            self.on_crash_callback(code)
