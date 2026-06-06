import asyncio
import json
import logging
import os
from typing import List, Dict, Set, Optional
import websockets

logger = logging.getLogger("guardian.polygon")

POLYGON_WS_URL = "wss://socket.polygon.io/stocks"
POLYGON_API_KEY = os.environ.get("POLYGON_API_KEY", "")

class PolygonStreamer:
    def __init__(self):
        self.ws: Optional[websockets.WebSocketClientProtocol] = None
        self.subscribed: Set[str] = set()
        self._task: Optional[asyncio.Task] = None
        self._clients: List[callable] = []  # callbacks for broadcasting

    def on_data(self, callback: callable):
        self._clients.append(callback)

    async def subscribe(self, symbols: List[str], channel: str = "T"):
        for s in symbols:
            self.subscribed.add(f"{channel}.{s}")
        if self.ws and self.ws.open:
            for sub in self.subscribed:
                await self.ws.send(json.dumps({"action": "subscribe", "params": sub}))

    async def unsubscribe(self, symbols: List[str], channel: str = "T"):
        for s in symbols:
            self.subscribed.discard(f"{channel}.{s}")

    async def start(self):
        if not POLYGON_API_KEY:
            logger.warning("POLYGON_API_KEY not set — Polygon streamer disabled")
            return
        self._task = asyncio.create_task(self._run())

    async def _run(self):
        while True:
            try:
                async with websockets.connect(POLYGON_WS_URL) as ws:
                    self.ws = ws
                    # Authenticate
                    auth = json.dumps({"action": "auth", "params": POLYGON_API_KEY})
                    await ws.send(auth)
                    resp = await ws.recv()
                    logger.info("Polygon auth: %s", resp[:100])

                    # Re-subscribe any pending symbols
                    for sub in self.subscribed:
                        await ws.send(json.dumps({"action": "subscribe", "params": sub}))

                    # Read messages and broadcast
                    async for raw in ws:
                        try:
                            msgs = json.loads(raw)
                            for msg in msgs if isinstance(msgs, list) else [msgs]:
                                ev = msg.get("ev", "")
                                if ev in ("T", "Q", "A", "AM"):
                                    for cb in self._clients:
                                        try:
                                            cb(msg)
                                        except Exception:
                                            pass
                        except json.JSONDecodeError:
                            pass
            except asyncio.CancelledError:
                break
            except Exception as e:
                logger.error("Polygon WS error: %s — reconnecting in 5s", e)
                await asyncio.sleep(5)

    async def stop(self):
        if self._task:
            self._task.cancel()
            self._task = None

polygon_streamer = PolygonStreamer()
