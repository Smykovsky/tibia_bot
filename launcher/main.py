import sys
import os
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
    parser.add_argument("--auto-start", action="store_true", help="Automatically start engine on launch")
    args = parser.parse_args()

    app = QApplication(sys.argv)
    app.setApplicationName("TibiaBot Launcher")

    engine_proc = EngineProcessManager(port=args.port)
    engine_client = EngineClient(port=args.port)

    if args.auto_start:
        try:
            engine_proc.start()
            engine_client.connect()
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
