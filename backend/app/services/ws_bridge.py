"""WebSocket bridge — runs on uvicorn, proxies to MT5 worker"""
import json
import logging
import socket
import asyncio

from fastapi import FastAPI, WebSocket, WebSocketDisconnect

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("ws_bridge")

app = FastAPI()

WORKER_HOST = "127.0.0.1"
WORKER_PORT = 5570


def _worker_cmd(cmd: dict) -> dict:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5)
    s.connect((WORKER_HOST, WORKER_PORT))
    s.sendall(json.dumps(cmd).encode() + b"\n")
    resp = s.recv(65536).decode().strip()
    s.close()
    return json.loads(resp)


@app.websocket("/ws/market-data")
async def market_data(ws: WebSocket):
    await ws.accept()
    symbol = "EURUSD.s"
    try:
        q = ws.url.query
        for p in q.split("&"):
            if "=" in p:
                k, v = p.split("=", 1)
                if k == "symbol":
                    symbol = v
    except Exception:
        pass
    logger.info("Market data WS: %s", symbol)
    try:
        while True:
            try:
                data = await asyncio.wait_for(ws.receive_text(), timeout=0.3)
                req = json.loads(data)
                if "symbol" in req:
                    symbol = req["symbol"]
            except asyncio.TimeoutError:
                pass
            try:
                rate = _worker_cmd({"action": "rate", "symbol": symbol})
                if "error" not in rate:
                    await ws.send_json({"type": "market_data", "symbol": symbol,
                                        "bid": rate["bid"], "ask": rate["ask"],
                                        "spread": rate["spread"], "time": rate["time"]})
            except Exception as e:
                await ws.send_json({"type": "error", "error": str(e)})
            await asyncio.sleep(0.1)
    except WebSocketDisconnect:
        logger.info("Market data WS disconnected: %s", symbol)


@app.websocket("/ws/orders")
async def orders(ws: WebSocket):
    await ws.accept()
    logger.info("Orders WS connected")
    import msgpack as _mp
    try:
        while True:
            raw = await ws.receive()
            cmd = None
            mode = "json"
            try:
                if "bytes" in raw:
                    mode = "msgpack"
                    data = raw["bytes"]
                    cmd = _mp.unpackb(data)
                    if isinstance(cmd, dict):
                        cmd = {k.decode() if isinstance(k, bytes) else k:
                               v.decode() if isinstance(v, bytes) else v for k, v in cmd.items()}
                elif "text" in raw:
                    cmd = json.loads(raw["text"])
                else:
                    continue
            except Exception:
                await ws.send_json({"error": "invalid message"})
                continue
            if not isinstance(cmd, dict):
                await ws.send_json({"error": "expected object"})
                continue
            action = cmd.get("action", "order" if cmd.get("side") else "").lower()
            if action == "order" or cmd.get("side"):
                result = _worker_cmd({"action": "market",
                                      "symbol": cmd.get("symbol", "EURUSD.s"),
                                      "side": cmd.get("side", "BUY"),
                                      "volume": float(cmd.get("volume", 0.01)),
                                      "magic": int(cmd.get("magic", 831846))})
                msg = {"ticket": result.get("order_id", 0), "status": "FILLED" if result.get("retcode") == 10009 else "REJECTED",
                       "client_order_id": cmd.get("clOrdId", ""), "fill_price": result.get("price", 0),
                       "latency_ms": result.get("latency_ms", 0), "error": result.get("comment", "")}
                if mode == "msgpack": await ws.send_bytes(_mp.packb(msg))
                else: await ws.send_json(msg)
            elif action == "ping":
                r = _worker_cmd({"action": "ping"})
                m = {"status": "ok", "balance": r.get("balance", 0)}
                if mode == "msgpack": await ws.send_bytes(_mp.packb(m))
                else: await ws.send_json(m)
            elif action == "balance":
                r = _worker_cmd({"action": "balance"})
                if mode == "msgpack": await ws.send_bytes(_mp.packb(r))
                else: await ws.send_json(r)
            elif action == "rate":
                r = _worker_cmd({"action": "rate", "symbol": cmd.get("symbol", "EURUSD.s")})
                if mode == "msgpack": await ws.send_bytes(_mp.packb(r))
                else: await ws.send_json(r)
            else:
                m = {"error": f"unknown action: {action}"}
                if mode == "msgpack": await ws.send_bytes(_mp.packb(m))
                else: await ws.send_json(m)
    except WebSocketDisconnect:
        logger.info("Orders WS disconnected")


@app.websocket("/ws/mt5")
async def mt5_ws(ws: WebSocket):
    await ws.accept()
    logger.info("MT5 WS connected")
    try:
        while True:
            await ws.receive_text()
    except WebSocketDisconnect:
        logger.info("MT5 WS disconnected")


@app.websocket("/ws/polygon")
async def polygon_ws(ws: WebSocket):
    await ws.accept()
    logger.info("Polygon WS connected")
    try:
        while True:
            data = await ws.receive_text()
            try:
                result = _worker_cmd(json.loads(data)) if data else {"error": "empty"}
                await ws.send_json(result)
            except Exception as e:
                await ws.send_json({"error": str(e)})
    except WebSocketDisconnect:
        logger.info("Polygon WS disconnected")


@app.websocket("/ws/news")
async def news_ws(ws: WebSocket):
    await ws.accept()
    logger.info("News WS connected")
    try:
        while True:
            await ws.receive_text()
            await ws.send_json({"type": "news", "data": []})
    except WebSocketDisconnect:
        logger.info("News WS disconnected")


@app.websocket("/community/ws/mcp-reload")
async def mcp_reload_ws(ws: WebSocket):
    await ws.accept()
    logger.info("MCP reload WS connected")
    try:
        while True:
            await ws.receive_text()
    except WebSocketDisconnect:
        logger.info("MCP reload WS disconnected")


@app.websocket("/backtest/ws/tick-replay")
async def tick_replay_ws(ws: WebSocket):
    await ws.accept()
    logger.info("Tick replay WS connected")
    try:
        while True:
            data = await ws.receive_text()
            try:
                result = _worker_cmd(json.loads(data)) if data else {"error": "empty"}
                await ws.send_json(result)
            except Exception as e:
                await ws.send_json({"error": str(e)})
    except WebSocketDisconnect:
        logger.info("Tick replay WS disconnected")


@app.get("/health")
async def health():
    return {"status": "ws_bridge_ok"}


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8156, log_level="info")
