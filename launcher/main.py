import sys
import os
import time
import argparse

# Add launcher parent dir to sys.path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from PySide6.QtWidgets import QApplication
from launcher.core.engine_process import EngineProcessManager
from launcher.core.engine_client import EngineClient
from launcher.ui.main_window import MainWindow

def main():
    parser = argparse.ArgumentParser(description="TibiaBot Launcher (Shrouded Architecture)")
    parser.add_argument("--port", type=int, default=8765, help="Engine IPC port (default: 8765)")
    parser.add_argument("--auto-start", action="store_true", default=True, help="Automatically start engine on launch")
    parser.add_argument("--no-auto-start", dest="auto_start", action="store_false", help="Do not auto-start engine")
    args = parser.parse_args()

    app = QApplication(sys.argv)
    app.setApplicationName("TibiaBot Launcher")

    engine_proc = EngineProcessManager(port=args.port)
    engine_client = EngineClient(port=args.port)

    if args.auto_start:
        try:
            started = engine_proc.start()
            if started:
                for _ in range(6):
                    if engine_client.connect():
                        break
                    time.sleep(0.2)
        except Exception as e:
            print(f"Warning: Failed to auto-start engine: {e}")

    window = MainWindow(engine_client, engine_proc)
    window.show()

    exit_code = app.exec()

    # Clean shutdown
    engine_client.disconnect()
    engine_proc.stop()

    sys.exit(exit_code)

if __name__ == "__main__":
    main()
