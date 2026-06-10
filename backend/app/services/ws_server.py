"""WebSocket server — streams MT5 market data & routes orders via MT5 worker"""
import asyncio
import json
import logging
import socket
import struct
import time

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("ws_server")

WS_HOST = "0.0.0.0"
WS_PORT = 8156
WORKER_HOST = "127.0.0.1"
WORKER_PORT = 5570


def _worker_cmd(cmd: dict, timeout=5) -> dict:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect((WORKER_HOST, WORKER_PORT))
    s.sendall(json.dumps(cmd).encode() + b"\n")
    resp = s.recv(65536).decode().strip()
    s.close()
    return json.loads(resp)


async def handle_market_data(ws):
    symbol = "EURUSD.s"
    try:
        query = ws.request_path.split("?")[1] if "?" in ws.request_path else ""
        for part in query.split("&"):
            if "=" in part:
                k, v = part.split("=", 1)
                if k == "symbol":
                    symbol = v
    except Exception:
        pass

    logger.info("Market data WS connected for %s", symbol)
    try:
        while True:
            try:
                msg = await asyncio.wait_for(ws.recv(), timeout=0.5)
                req = json.loads(msg)
                if "symbol" in req:
                    symbol = req["symbol"]
            except asyncio.TimeoutError:
                pass

            try:
                rate = _worker_cmd({"action": "rate", "symbol": symbol}, timeout=3)
                if "error" not in rate:
                    payload = {
                        "type": "market_data",
                        "symbol": symbol,
                        "bid": rate["bid"],
                        "ask": rate["ask"],
                        "spread": rate["spread"],
                        "time": rate["time"],
                    }
                    await ws.send(json.dumps(payload))
            except Exception as e:
                await ws.send(json.dumps({"type": "error", "error": str(e)}))

            await asyncio.sleep(0.1)
    except Exception:
        logger.info("Market data WS disconnected for %s", symbol)


async def handle_orders(ws):
    logger.info("Orders WS connected")
    try:
        async for raw in ws:
            try:
                cmd = json.loads(raw)
                action = cmd.get("action")
                if action == "order":
                    symbol = cmd.get("symbol", "EURUSD.s")
                    side = cmd.get("side", "BUY")
                    volume = float(cmd.get("volume", 0.01))
                    magic = int(cmd.get("magic", 831846))
                    result = _worker_cmd({"action": "market", "symbol": symbol,
                                          "side": side, "volume": volume, "magic": magic})
                    await ws.send(json.dumps(result))
                elif action == "ping":
                    result = _worker_cmd({"action": "ping"})
                    await ws.send(json.dumps(result))
                elif action == "balance":
                    result = _worker_cmd({"action": "balance"})
                    await ws.send(json.dumps(result))
                else:
                    await ws.send(json.dumps({"error": f"unknown action: {action}"}))
            except Exception as e:
                await ws.send(json.dumps({"error": str(e)}))
    except Exception:
        logger.info("Orders WS disconnected")


async def handler(ws):
    path = ws.request_path
    if "/ws/market-data" in path:
        await handle_market_data(ws)
    elif "/ws/orders" in path:
        await handle_orders(ws)
    else:
        await ws.close()


async def main():
    import websockets
    async with websockets.serve(handler, WS_HOST, WS_PORT, ping_interval=30):
        logger.info("WebSocket server on ws://%s:%s", WS_HOST, WS_PORT)
        await asyncio.Future()


if __name__ == "__main__":
    asyncio.run(main())
