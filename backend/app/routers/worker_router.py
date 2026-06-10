import json
import logging
import socket

from fastapi import APIRouter, HTTPException

logger = logging.getLogger("guardian.worker")

router = APIRouter(prefix="/mt5/worker", tags=["mt5_worker"])

WORKER_HOST = "127.0.0.1"
WORKER_PORT = 5570


def _send_worker(cmd: dict) -> dict:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(10)
        s.connect((WORKER_HOST, WORKER_PORT))
        s.sendall((json.dumps(cmd) + "\n").encode())
        resp = s.recv(65536).decode().strip()
        s.close()
        return json.loads(resp)
    except socket.timeout:
        raise HTTPException(status_code=504, detail="MT5 worker timed out")
    except ConnectionRefusedError:
        raise HTTPException(status_code=502, detail="MT5 worker not running (port 5570)")
    except json.JSONDecodeError as exc:
        raise HTTPException(status_code=502, detail=f"MT5 worker bad response: {exc}")
    except Exception as e:
        logger.error("MT5 worker command failed: %s", e)
        raise HTTPException(status_code=502, detail=f"MT5 worker unreachable: {e}")


@router.post("/order")
def worker_order(body: dict):
    symbol = body.get("symbol", "").strip()
    side = body.get("side", "BUY").strip().upper()
    volume = float(body.get("volume", 0.01))
    magic = int(body.get("magic", 831846))

    if not symbol:
        raise HTTPException(status_code=400, detail="symbol is required")

    cmd = {"action": "market", "symbol": symbol, "side": side,
           "volume": volume, "magic": magic}
    result = _send_worker(cmd)

    success = result.get("retcode") in (0, 10009)
    return {
        "success": success,
        "data": result,
        "error": result.get("comment", "") if not success else "",
    }


@router.get("/status")
def worker_status():
    result = _send_worker({"action": "ping"})
    return {
        "success": result.get("status") == "ok",
        "data": result,
    }


@router.get("/rate")
def worker_rate(symbol: str):
    if not symbol:
        raise HTTPException(status_code=400, detail="symbol query param required")
    result = _send_worker({"action": "rate", "symbol": symbol})
    return {"success": "error" not in result, "data": result}


@router.get("/candles")
def worker_candles(symbol: str, timeframe: str = "1h", count: int = 100):
    if not symbol:
        raise HTTPException(status_code=400, detail="symbol query param required")
    result = _send_worker({"action": "candles", "symbol": symbol,
                           "timeframe": timeframe, "count": count})
    return {"success": "error" not in result, "data": result}


@router.get("/balance")
def worker_balance():
    result = _send_worker({"action": "balance"})
    return {"success": "error" not in result, "data": result}
