"""TCP bridge to the C++ Trade Server."""
import json
import logging
import socket
import threading

logger = logging.getLogger("guardian.trade_server")

HOST = "127.0.0.1"
PORT = 5559

class TradeServerClient:
    def __init__(self):
        self._sock = None

    def connect(self):
        try:
            self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._sock.settimeout(5.0)
            self._sock.connect((HOST, PORT))
            return True
        except Exception as e:
            self._sock = None
            return False

    def send_command(self, command: str) -> str:
        for attempt in range(3):
            try:
                if not self._sock:
                    if not self.connect():
                        continue
                self._sock.sendall((command + "\n").encode())
                response = self._sock.recv(65536).decode().strip()
                return response
            except Exception as e:
                self._sock = None
                if attempt == 2:
                    return json.dumps({"error": str(e)})
        return json.dumps({"error": "Failed to connect"})

    def get_status(self) -> dict:
        resp = self.send_command("STATUS")
        try:
            return json.loads(resp)
        except json.JSONDecodeError:
            return {"error": resp}

    def update_price(self, symbol: str, bid: float, ask: float):
        self.send_command(f"PRICE|{symbol}|{bid}|{ask}")

    def close(self):
        if self._sock:
            self._sock.close()
            self._sock = None

trade_server = TradeServerClient()

