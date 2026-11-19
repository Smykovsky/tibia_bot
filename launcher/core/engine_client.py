import socket
import json
import threading
import time
from typing import Callable, Optional, Dict, Any

class EngineClient:
    def __init__(self, host: str = "127.0.0.1", port: int = 8765):
        self.host = host
        self.port = port
        self.sock: Optional[socket.socket] = None
        self.file_reader = None
        self.is_connected = False
        self._lock = threading.Lock()
        self._req_id = 0
        self._pending_requests: Dict[int, threading.Event] = {}
        self._request_responses: Dict[int, Any] = {}
        self._listener_thread: Optional[threading.Thread] = None
        self._running = False

        # Callbacks
        self.on_log_callback: Optional[Callable[[str, str, str], None]] = None
        self.on_event_callback: Optional[Callable[[str, Dict[str, Any]], None]] = None
        self.on_disconnect_callback: Optional[Callable[[], None]] = None

    def connect(self, timeout: float = 2.0) -> bool:
        with self._lock:
            if self.is_connected:
                return True
            try:
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.sock.settimeout(timeout)
                self.sock.connect((self.host, self.port))
                self.sock.settimeout(None)
                self.file_reader = self.sock.makefile('r', encoding='utf-8')
                self.is_connected = True
                self._running = True

                self._listener_thread = threading.Thread(target=self._listen_loop, daemon=True)
                self._listener_thread.start()
                return True
            except Exception as e:
                self.is_connected = False
                if self.sock:
                    try:
                        self.sock.close()
                    except:
                        pass
                self.sock = None
                return False

    def disconnect(self):
        with self._lock:
            self._running = False
            if self.sock:
                try:
                    self.sock.shutdown(socket.SHUT_RDWR)
                    self.sock.close()
                except:
                    pass
                self.sock = None
            self.file_reader = None
            self.is_connected = False

        # Wake up any pending requests
        for event in self._pending_requests.values():
            event.set()

    def _listen_loop(self):
        while self._running and self.file_reader:
            try:
                line = self.file_reader.readline()
                if not line:
                    break
                data = json.loads(line.strip())
                self._handle_incoming(data)
            except Exception:
                break

        self.is_connected = False
        if self.on_disconnect_callback:
            try:
                self.on_disconnect_callback()
            except:
                pass

    def _handle_incoming(self, data: Dict[str, Any]):
        # Notification or response?
        req_id = data.get("id")
        if req_id is not None and req_id in self._pending_requests:
            self._request_responses[req_id] = data
            self._pending_requests[req_id].set()
            return

        method = data.get("method")
        params = data.get("params", {})
        if method == "onLog" and self.on_log_callback:
            level = params.get("level", "info")
            msg = params.get("message", "")
            ts = params.get("timestamp", "")
            self.on_log_callback(level, msg, ts)
        elif method == "onEvent" and self.on_event_callback:
            ev_type = params.get("type", "")
            ev_data = params.get("data", {})
            self.on_event_callback(ev_type, ev_data)

    def send_request(self, method: str, params: Optional[Dict[str, Any]] = None, timeout: float = 3.0) -> Any:
        if not self.is_connected or not self.sock:
            raise ConnectionError("Not connected to Engine")

        with self._lock:
            self._req_id += 1
            current_id = self._req_id
            payload = {
                "jsonrpc": "2.0",
                "method": method,
                "id": current_id
            }
            if params is not None:
                payload["params"] = params

            event = threading.Event()
            self._pending_requests[current_id] = event

            msg = (json.dumps(payload) + "\n").encode('utf-8')
            self.sock.sendall(msg)

        # Wait outside of mutex
        signaled = event.wait(timeout=timeout)
        with self._lock:
            self._pending_requests.pop(current_id, None)
            resp = self._request_responses.pop(current_id, None)

        if not signaled or resp is None:
            raise TimeoutError(f"Request {method} timed out")

        if "error" in resp:
            raise RuntimeError(f"Engine RPC error: {resp['error']}")

        return resp.get("result")

    # High-level API calls
    def ping(self) -> str:
        return self.send_request("ping")

    def get_engine_state(self) -> Dict[str, Any]:
        return self.send_request("getEngineState")

    def start_engine(self):
        return self.send_request("startEngine")

    def stop_engine(self):
        return self.send_request("stopEngine")

    def pause_engine(self):
        return self.send_request("pauseEngine")

    def resume_engine(self):
        return self.send_request("resumeEngine")

    def get_game_state(self) -> Dict[str, Any]:
        return self.send_request("getGameState")

    def get_modules(self) -> list:
        return self.send_request("getModules")

    def set_module_state(self, name: str, enabled: bool):
        return self.send_request("setModuleState", {"name": name, "enabled": enabled})

    def get_module_config(self, name: str) -> Dict[str, Any]:
        return self.send_request("getModuleConfig", {"name": name})

    def set_module_config(self, name: str, config: Dict[str, Any]):
        return self.send_request("setModuleConfig", {"name": name, "config": config})

    def load_profile(self, profile: str) -> bool:
        res = self.send_request("loadProfile", {"profile": profile})
        return res.get("success", False)

    def save_profile(self, profile: str) -> bool:
        res = self.send_request("saveProfile", {"profile": profile})
        return res.get("success", False)

    def execute_lua(self, code: str) -> bool:
        res = self.send_request("executeLua", {"code": code})
        return res.get("success", False)

    def get_client_status(self) -> Dict[str, Any]:
        return self.send_request("getClientStatus")

    def set_target_client(self, process_name: str) -> Dict[str, Any]:
        return self.send_request("setTargetClient", {"processName": process_name})
