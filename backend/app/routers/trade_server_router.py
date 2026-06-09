"""REST API for the C++ Trade Server — bypasses MT5 entirely."""
import json
import logging
import socket
from fastapi import APIRouter, Body, Depends
from app.routers.auth import resolve_user

logger = logging.getLogger("guardian.trade_server_router")
router = APIRouter(tags=["trade_server"])

TRADE_SERVER_HOST = "127.0.0.1"
TRADE_SERVER_PORT = 5559

def _send_command(command: str) -> dict:
    """Send a command to the C++ Trade Server and parse JSON response."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5.0)
        s.connect((TRADE_SERVER_HOST, TRADE_SERVER_PORT))
        s.sendall((command + "\n").encode())

        # Read all data (handle TCP fragmentation)
        chunks = []
        while True:
            try:
                chunk = s.recv(65536)
                if not chunk:
                    break
                chunks.append(chunk)
                if len(chunk) < 65536:
                    break  # short read = end of message
            except socket.timeout:
                break
        s.close()

        data = b"".join(chunks).decode().strip()
        if not data:
            return {"error": "Empty response from trade server"}
        return json.loads(data)

    except socket.timeout:
        return {"error": "Trade server timeout"}
    except ConnectionRefusedError:
        return {"error": "Trade server not running on port 5559"}
    except json.JSONDecodeError as e:
        return {"error": f"Invalid JSON from trade server: {data[:200] if 'data' in dir() else str(e)}"}
    except Exception as e:
        return {"error": str(e)}


@router.get("/trade/status")
async def trade_status(user=Depends(resolve_user)):
    """Get trade server status — bypasses MT5."""
    result = _send_command("STATUS")
    if "error" in result:
        return {"success": False, "error": result["error"]}
    return {"success": True, "data": result}


@router.get("/trade/positions")
async def trade_positions(user=Depends(resolve_user)):
    """Get all open positions from trade server."""
    result = _send_command("POSITIONS")
    if "error" in result:
        return {"success": False, "error": result["error"]}
    return {"success": True, "data": result.get("positions", [])}


@router.get("/trade/orders")
async def trade_orders(user=Depends(resolve_user)):
    """Get all orders from trade server."""
    result = _send_command("ORDERS")
    if "error" in result:
        return {"success": False, "error": result["error"]}
    return {"success": True, "data": result.get("orders", [])}


@router.post("/trade/order/market")
async def trade_market_order(body: dict = Body(...), user=Depends(resolve_user)):
    """Place a market order through the C++ Trade Server (no MT5 needed)."""
    symbol = body.get("symbol", "XAUUSD").upper()
    side = body.get("side", "BUY").upper()
    volume = float(body.get("volume", 0.01))
    sl = float(body.get("sl", 0))
    tp = float(body.get("tp", 0))
    cmd = f"PLACE|{symbol}|{side}|{volume}|MARKET|0|{sl}|{tp}|0"
    result = _send_command(cmd)
    if "error" in result:
        return {"success": False, "error": result["error"]}
    return {"success": True, "data": result}


@router.post("/trade/order/limit")
async def trade_limit_order(body: dict = Body(...), user=Depends(resolve_user)):
    """Place a limit order."""
    symbol = body.get("symbol", "XAUUSD").upper()
    side = body.get("side", "BUY").upper()
    volume = float(body.get("volume", 0.01))
    price = float(body.get("price", 0))
    sl = float(body.get("sl", 0))
    tp = float(body.get("tp", 0))
    cmd = f"PLACE|{symbol}|{side}|{volume}|LIMIT|{price}|{sl}|{tp}|0"
    result = _send_command(cmd)
    if "error" in result:
        return {"success": False, "error": result["error"]}
    return {"success": True, "data": result}


@router.post("/trade/order/stop")
async def trade_stop_order(body: dict = Body(...), user=Depends(resolve_user)):
    """Place a stop order."""
    symbol = body.get("symbol", "XAUUSD").upper()
    side = body.get("side", "BUY").upper()
    volume = float(body.get("volume", 0.01))
    price = float(body.get("price", 0))
    cmd = f"PLACE|{symbol}|{side}|{volume}|STOP|{price}|0|0|0"
    result = _send_command(cmd)
    if "error" in result:
        return {"success": False, "error": result["error"]}
    return {"success": True, "data": result}


@router.post("/trade/order/cancel")
async def trade_cancel_order(body: dict = Body(...), user=Depends(resolve_user)):
    """Cancel an order by ID."""
    order_id = int(body.get("order_id", 0))
    cmd = f"CANCEL|{order_id}"
    result = _send_command(cmd)
    if "error" in result:
        return {"success": False, "error": result["error"]}
    return {"success": True, "data": result}


@router.post("/trade/price")
async def trade_price_update(body: dict = Body(...), user=Depends(resolve_user)):
    """Push a price update to the trade server for SL/TP evaluation."""
    symbol = body.get("symbol", "XAUUSD").upper()
    bid = float(body.get("bid", 0))
    ask = float(body.get("ask", 0))
    cmd = f"PRICE|{symbol}|{bid}|{ask}"
    result = _send_command(cmd)
    if "error" in result:
        return {"success": False, "error": result["error"]}
    return {"success": True, "data": result}
