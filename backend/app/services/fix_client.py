"""Sync FIX Client — bridges backend to FIX broker/simulator"""
import logging
import socket
import threading
import time
import uuid
from queue import Queue

import simplefix

logger = logging.getLogger("fix_client")
FIX_VERSION = "FIX.4.4"


class FixClient:
    def __init__(self):
        self.sock = None
        self.seq_num = 1
        self.connected = False
        self.parser = simplefix.FixParser()
        self.pending_orders = {}
        self._lock = threading.Lock()
        self._listener_thread = None
        self._running = False

    def connect(self, host="127.0.0.1", port=9878, timeout=10):
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(timeout)
            self.sock.connect((host, port))
            self.sock.settimeout(30)
            logger.info("Connected to FIX server %s:%s", host, port)

            msg = self.build_message("A", {98: b"0", 108: b"30"})
            self.sock.sendall(msg)
            logger.info("Logon sent")

            resp = self.sock.recv(4096)
            self.parser = simplefix.FixParser()
            self.parser.append_buffer(resp)
            logon_resp = self.parser.get_message()
            if logon_resp and logon_resp.get(35) and logon_resp.get(35).decode() == "A":
                self.connected = True
                logger.info("FIX Logon successful!")
            else:
                logger.error("FIX Logon failed: %s", resp)
                self.sock.close()
                self.sock = None
                return False

            self._running = True
            self._listener_thread = threading.Thread(target=self._listen, daemon=True)
            self._listener_thread.start()
            return True
        except Exception as e:
            logger.error("FIX connect failed: %s", e)
            self.connected = False
            if self.sock:
                self.sock.close()
                self.sock = None
            return False

    def disconnect(self):
        self._running = False
        if self.connected:
            try:
                msg = self.build_message("5", {})
                self.sock.sendall(msg)
            except Exception:
                pass
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
        self.connected = False
        logger.info("FIX disconnected")

    def _listen(self):
        while self._running and self.connected:
            try:
                data = self.sock.recv(4096)
                if not data:
                    logger.warning("FIX server disconnected")
                    self.connected = False
                    break
                self.parser.append_buffer(data)
                while True:
                    msg = self.parser.get_message()
                    if msg is None:
                        break
                    self._process_incoming(msg)
            except socket.timeout:
                continue
            except Exception as e:
                if self._running:
                    logger.error("FIX listener error: %s", e)
                self.connected = False
                break

    def _process_incoming(self, msg):
        try:
            msg_type = msg.get(35).decode()
        except Exception:
            return

        if msg_type == "8":
            cl_ord_id = msg.get(11).decode() if msg.get(11) else "?"
            exec_type = msg.get(150).decode() if msg.get(150) else "?"
            ord_status = msg.get(39).decode() if msg.get(39) else "?"
            logger.info("ExecReport: %s status=%s exec=%s", cl_ord_id, ord_status, exec_type)
            with self._lock:
                if cl_ord_id in self.pending_orders:
                    self.pending_orders[cl_ord_id].append({
                        "exec_type": exec_type,
                        "ord_status": ord_status,
                        "last_px": msg.get(31).decode() if msg.get(31) else None,
                        "last_qty": msg.get(32).decode() if msg.get(32) else None,
                    })

    def send_market_order(self, symbol, side, qty, timeout=5):
        if not self.connected:
            raise ConnectionError("FIX not connected")

        cl_ord_id = uuid.uuid4().hex[:8]
        fix_side = b"1" if side.upper() == "BUY" else b"2"

        with self._lock:
            self.pending_orders[cl_ord_id] = []

        msg = self.build_message("D", {
            11: cl_ord_id.encode(),
            55: symbol.encode(),
            54: fix_side,
            38: str(qty).encode(),
            40: b"1",
        })
        self.sock.sendall(msg)
        logger.info("Sent order %s: %s %s %s", cl_ord_id, symbol, side, qty)

        deadline = time.time() + timeout
        while time.time() < deadline:
            with self._lock:
                reports = list(self.pending_orders.get(cl_ord_id, []))
                if reports and any(r["exec_type"] == "F" for r in reports):
                    del self.pending_orders[cl_ord_id]
                    return {"cl_ord_id": cl_ord_id, "reports": reports, "filled": True}
            time.sleep(0.1)

        with self._lock:
            reports = list(self.pending_orders.pop(cl_ord_id, []))
        return {"cl_ord_id": cl_ord_id, "reports": reports, "filled": False,
                "error": "timeout waiting for fill"}

    def build_message(self, msg_type, fields):
        msg = simplefix.FixMessage()
        msg.append_pair(8, FIX_VERSION.encode())
        msg.append_pair(35, msg_type.encode())
        msg.append_pair(49, b"FINCEPT")
        msg.append_pair(56, b"SIMULATOR")
        msg.append_pair(34, str(self.seq_num).encode())
        self.seq_num += 1
        msg.append_utc_timestamp(52)
        for tag, val in fields.items():
            msg.append_pair(tag, val)
        return msg.encode()


fix_client = FixClient()
