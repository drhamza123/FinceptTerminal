"""FIX Simulator — mock broker FIX acceptor for testing"""
import logging
import socket
import threading
import time
import uuid

import simplefix

logger = logging.getLogger("fix_simulator")
FIX_VERSION = "FIX.4.4"


class FixSimulator:
    def __init__(self, host="127.0.0.1", port=9878):
        self.host = host
        self.port = port
        self.server = None
        self.running = False
        self.seq_num = 1

    def start(self):
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind((self.host, self.port))
        self.server.listen(5)
        self.running = True
        logger.info("FIX Simulator listening on %s:%s", self.host, self.port)

        while self.running:
            try:
                conn, addr = self.server.accept()
                logger.info("Client connected from %s", addr)
                t = threading.Thread(target=self.handle_client, args=(conn, addr), daemon=True)
                t.start()
            except OSError:
                break

    def stop(self):
        self.running = False
        try:
            self.server.close()
        except Exception:
            pass

    def handle_client(self, conn, addr):
        parser = simplefix.FixParser()
        while self.running:
            try:
                data = conn.recv(4096)
                if not data:
                    break
                parser.append_buffer(data)
                while True:
                    msg = parser.get_message()
                    if msg is None:
                        break
                    self.process_message(msg, conn)
            except Exception as e:
                logger.error("Client handler error: %s", e)
                break
        conn.close()
        logger.info("Client %s disconnected", addr)

    def process_message(self, msg, conn):
        try:
            msg_type = msg.get(35).decode()
        except Exception:
            return

        if msg_type == "A":
            logger.info("Received Logon, sending reply")
            reply = self.build_message("A", {98: b"0", 108: b"30"})
            conn.sendall(reply)

        elif msg_type == "D":
            cl_ord_id = msg.get(11).decode() if msg.get(11) else "?"
            symbol = msg.get(55).decode() if msg.get(55) else "?"
            side = msg.get(54).decode() if msg.get(54) else "?"
            qty = msg.get(38).decode() if msg.get(38) else "0"
            price = "1.1000"
            logger.info("Order %s: %s %s %s @ %s", cl_ord_id, symbol, side, qty, price)

            ack = self.build_message("8", {
                37: b"SIM_1",
                11: cl_ord_id.encode(),
                17: b"EXEC_ACK_1",
                150: b"0",
                39: b"0",
                55: symbol.encode(),
                54: side.encode(),
                38: qty.encode(),
                44: price.encode(),
            })
            conn.sendall(ack)
            time.sleep(0.05)

            fill = self.build_message("8", {
                37: b"SIM_1",
                11: cl_ord_id.encode(),
                17: b"EXEC_FILL_1",
                150: b"F",
                39: b"2",
                55: symbol.encode(),
                54: side.encode(),
                38: qty.encode(),
                31: price.encode(),
                32: qty.encode(),
            })
            conn.sendall(fill)
            logger.info("Sent fill for %s", cl_ord_id)

        elif msg_type == "5":
            logger.info("Logout received")
            reply = self.build_message("5", {})
            conn.sendall(reply)

    def build_message(self, msg_type, fields):
        msg = simplefix.FixMessage()
        msg.append_pair(8, FIX_VERSION.encode())
        msg.append_pair(35, msg_type.encode())
        msg.append_pair(49, b"SIMULATOR")
        msg.append_pair(56, b"CLIENT")
        msg.append_pair(34, str(self.seq_num).encode())
        self.seq_num += 1
        msg.append_utc_timestamp(52)
        for tag, val in fields.items():
            msg.append_pair(tag, val)
        return msg.encode()


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s [%(levelname)s] %(message)s")
    sim = FixSimulator()
    try:
        sim.start()
    except KeyboardInterrupt:
        sim.stop()
        logger.info("Simulator stopped")
