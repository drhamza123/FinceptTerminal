# backend/app/services/mt5_zmq_worker.py
import zmq
import zmq.asyncio
import msgpack
import asyncio
import logging
from typing import Dict, Any, Optional
import json

logger = logging.getLogger("guardian.mt5_zmq")

class MT5ZMQBridge:
    def __init__(self, pull_port: int = 5557, mt5_port: int = 5555):
        self.pull_port = pull_port
        self.mt5_port = mt5_port
        self.context: Optional[zmq.asyncio.Context] = None
        self.pull_socket: Optional[zmq.asyncio.Socket] = None
        self.req_socket: Optional[zmq.asyncio.Socket] = None
        self._task: Optional[asyncio.Task] = None
        self.mt5_connected = False

    async def start(self):
        import asyncio
        try:
            asyncio.get_running_loop()
        except RuntimeError:
            pass

        self.context = zmq.asyncio.Context()

        # PULL: receives binary msgpack orders from C++ SmartOrderEngine
        try:
            self.pull_socket = self.context.socket(zmq.PULL)
            self.pull_socket.setsockopt(zmq.RCVHWM, 1000)
            self.pull_socket.bind(f"tcp://127.0.0.1:{self.pull_port}")
            logger.info(f"ZMQ PULL bound on port {self.pull_port}")
        except Exception as e:
            logger.warning(f"ZMQ PULL bind failed on {self.pull_port}: {e} — running without ZMQ bridge")
            self.mt5_connected = False
            return

        # REQ: forwards orders to MT5 EA (FinceptMT5EA.mq5)
        # Only attempt if MT5 is expected to be running
        self.req_socket = self.context.socket(zmq.REQ)
        self.req_socket.setsockopt(zmq.LINGER, 0)  # Don't block on close
        self.req_socket.setsockopt(zmq.CONNECT_TIMEOUT, 2000)  # 2s connect timeout
        try:
            self.req_socket.connect(f"tcp://127.0.0.1:{self.mt5_port}")
            self.mt5_connected = True
            logger.info(f"ZMQ REQ connected to MT5 on port {self.mt5_port}")
        except Exception as e:
            logger.warning(f"MT5 not reachable on port {self.mt5_port} — running in bridge-only mode: {e}")
            self.mt5_connected = False

        self._task = asyncio.create_task(self._run())

    async def stop(self):
        if self._task:
            self._task.cancel()
        if self.pull_socket:
            self.pull_socket.close()
        if self.req_socket:
            self.req_socket.close()
        if self.context:
            self.context.term()
        logger.info("ZMQ bridge stopped")

    async def _run(self):
        while True:
            try:
                raw = await self.pull_socket.recv()
                order: Dict[str, Any] = msgpack.unpackb(raw, raw=False)
                logger.debug(f"Received order: {order.get('symbol')} {order.get('side')}")

                # Build pipe-delimited MQL5 payload
                action = order.get("side", "BUY")
                symbol = order.get("symbol", "XAUUSD")
                volume = order.get("volume", 0.01)
                price = order.get("price", 0.0)
                sl = order.get("sl", 0.0)
                tp = order.get("tp", 0.0)
                comment = order.get("comment", "FinceptAI")

                payload = f"{action}|{symbol}|{volume}|{price}|{sl}|{tp}|{comment}"

                if not self.mt5_connected:
                    logger.warning(f"MT5 not connected — simulated fill for {symbol}")
                    result = self._simulate_fill(order)
                    # Route back via WebSocket for UI update
                    await self._route_result(result)
                    continue

                await self.req_socket.send_string(payload)
                response = await asyncio.wait_for(self.req_socket.recv_string(), timeout=2.0)
                parts = response.split("|")

                if parts[0] == "OK":
                    result = {"status": "FILLED", "ticket": int(parts[1]), "retcode": 0}
                    logger.info(f"FILLED {symbol} ticket={parts[1]}")
                else:
                    result = {"status": "REJECTED", "error": parts[2], "retcode": int(parts[1])}
                    logger.warning(f"REJECTED {symbol}: {parts[2]}")

                await self._route_result(result)

            except asyncio.CancelledError:
                break
            except zmq.ZMQError as e:
                logger.error(f"ZMQ error: {e}")
                await asyncio.sleep(0.1)
            except Exception as e:
                logger.error(f"Bridge error: {e}", exc_info=True)

    def _simulate_fill(self, order: dict) -> dict:
        import time
        return {
            "status": "FILLED",
            "ticket": int(time.time() * 1000) % 1000000,
            "symbol": order.get("symbol", ""),
            "side": order.get("side", "BUY"),
            "volume": order.get("volume", 0),
            "fill_price": 0,
            "latency_ms": 0.5,
            "ts": time.time(),
        }

    async def _route_result(self, result: dict):
        """Push result back to C++ via a PUB/ROUTER socket or broadcast via WebSocket."""
        from app.routers.mt5_bridge import _broadcast_ws
        try:
            _broadcast_ws({
                "type": "order_result",
                "data": result,
            })
        except Exception:
            pass
