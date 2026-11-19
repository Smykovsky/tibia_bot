import socket
import json
import time
import subprocess
import signal

class IpcClient:
    def __init__(self, host="127.0.0.1", port=8799):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.s.connect((host, port))
        self.f = self.s.makefile('r', encoding='utf-8')

    def request(self, method, params=None, req_id=1):
        payload = {"jsonrpc": "2.0", "method": method, "id": req_id}
        if params is not None:
            payload["params"] = params
        self.s.sendall((json.dumps(payload) + "\n").encode('utf-8'))

        while True:
            line = self.f.readline()
            if not line:
                raise ConnectionResetError("Connection closed")
            msg = json.loads(line.strip())
            # Skip notifications like onLog / onEvent
            if msg.get("id") == req_id:
                return msg

    def close(self):
        self.s.close()

def main():
    engine_bin = "/Users/kamil/Desktop/bot/engine/build/tibia_engine"
    print(f"Starting {engine_bin}...")
    proc = subprocess.Popen([engine_bin, "--port", "8799"])
    time.sleep(1.0)

    try:
        client = IpcClient("127.0.0.1", 8799)
        print("Connected to TibiaBot Engine via TCP 127.0.0.1:8799")

        # 1. Ping
        resp = client.request("ping", req_id=1)
        print("1. Ping result:", resp)
        assert resp["result"] == "pong"

        # 2. Get Engine State
        resp = client.request("getEngineState", req_id=2)
        print("2. State result:", resp)
        assert resp["result"]["state"] == "running"

        # 3. Get Modules
        resp = client.request("getModules", req_id=3)
        print("3. Modules count:", len(resp["result"]))
        module_names = [m["name"] for m in resp["result"]]
        print("   Registered modules:", module_names)
        assert "Healing" in module_names
        assert "Cavebot" in module_names
        assert "Targeting" in module_names

        # 4. Execute Lua
        lua_code = 'Bot.log("Hello from Python Launcher via IPC!"); local p = Game.getPlayer(); Bot.log("Player HP: " .. p.hp)'
        resp = client.request("executeLua", {"code": lua_code}, req_id=4)
        print("4. Lua exec result:", resp)
        assert resp["result"]["success"] is True

        client.close()
        print("All IPC integration tests verified successfully!")

    finally:
        print("Terminating engine process...")
        proc.send_signal(signal.SIGINT)
        proc.wait(timeout=3)
        print("Engine stopped cleanly with return code:", proc.returncode)

if __name__ == "__main__":
    main()
