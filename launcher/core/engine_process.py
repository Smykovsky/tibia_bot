import subprocess
import os
import sys
import signal
import time
import threading
from typing import Optional, Callable

class EngineProcessManager:
    def __init__(self, binary_path: Optional[str] = None, port: int = 8765):
        if binary_path is None:
            exe_name = "tibia_engine.exe" if os.name == "nt" else "tibia_engine"
            if getattr(sys, 'frozen', False):
                # Running inside PyInstaller bundled .exe
                base_dir = getattr(sys, '_MEIPASS', os.path.dirname(sys.executable))
                candidates = [
                    os.path.join(base_dir, exe_name),
                    os.path.join(os.path.dirname(sys.executable), exe_name),
                    os.path.join(base_dir, "engine", exe_name),
                    os.path.join(os.path.dirname(sys.executable), "engine", exe_name),
                ]
                self.binary_path = next((c for c in candidates if os.path.exists(c)), candidates[0])
            else:
                root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
                candidates = [
                    os.path.join(root_dir, "engine", "build", exe_name),
                    os.path.join(root_dir, "engine", "build", "Release", exe_name),
                    os.path.join(root_dir, "engine", "build", "Debug", exe_name),
                    os.path.join(root_dir, exe_name),
                    os.path.join(root_dir, "engine", "build", "tibia_engine"),
                ]
                self.binary_path = next((c for c in candidates if os.path.exists(c)), candidates[0])
        else:
            self.binary_path = binary_path

        self.port = port
        self.proc: Optional[subprocess.Popen] = None
        self._monitor_thread: Optional[threading.Thread] = None
        self._running = False
        self.on_crash_callback: Optional[Callable[[int], None]] = None

    def is_running(self) -> bool:
        if self.proc is None:
            return False
        return self.proc.poll() is None

    def start(self) -> bool:
        if self.is_running():
            return True

        if not os.path.exists(self.binary_path):
            raise FileNotFoundError(f"Engine executable not found at: {self.binary_path}")

        if getattr(sys, 'frozen', False):
            workdir = os.path.dirname(sys.executable)
        else:
            workdir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
        cmd = [self.binary_path, "--port", str(self.port)]

        self.proc = subprocess.Popen(
            cmd,
            cwd=workdir,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        self._running = True
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
        if self._running and self.on_crash_callback:
            self.on_crash_callback(code)
