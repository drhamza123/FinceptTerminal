"""TradingView webhook endpoint — receives alerts and forwards to Trade Server."""
import json
import logging
import socket
from fastapi import APIRouter, Request

logger = logging.getLogger("guardian.webhook")
router = APIRouter(tags=["webhook"])

TRADE_SERVER_HOST = "127.0.0.1"
TRADE_SERVER_PORT = 5559

def _send_command(command: str) -> str:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(3.0)
        s.connect((TRADE_SERVER_HOST, TRADE_SERVER_PORT))
        s.sendall((command + "\n").encode())
        data = s.recv(65536).decode().strip()
        s.close()
        return data
    except Exception as e:
        return '{"error":"%s"}' % str(e)


@router.post("/webhook/tradingview")
async def tradingview_webhook(request: Request):
    """Receive TradingView alert webhook and forward to Trade Server."""
    try:
        body = await request.json()
    except Exception:
        body = {}
    
    logger.info(f"TradingView webhook: {json.dumps(body)[:200]}")
    
    # Parse TradingView alert format
    symbol = (body.get("symbol") or body.get("ticker") or "XAUUSD").upper()
    side = (body.get("side") or body.get("action") or "buy").upper()
    
    # Map TradingView order actions
    if side in ("BUY", "LONG", "ENTRY_LONG"):
        side = "BUY"
    elif side in ("SELL", "SHORT", "ENTRY_SHORT"):
        side = "SELL"
    elif side in ("EXIT_LONG", "EXIT_SHORT", "CLOSE"):
        # For exit signals, send a close via trade server
        cmd = f"POSITIONS"
        resp = _send_command(cmd)
        return {"success": True, "message": "Exit signal received", "data": resp[:200]}
    else:
        return {"success": False, "error": f"Unknown side: {side}"}
    
    volume = float(body.get("volume") or body.get("qty") or body.get("quantity", 0.01))
    order_type = "MARKET"
    price = float(body.get("price", 0))
    sl = float(body.get("stop_loss") or body.get("sl", 0))
    tp = float(body.get("take_profit") or body.get("tp", 0))
    
    cmd = f"PLACE|{symbol}|{side}|{volume}|{order_type}|{price}|{sl}|{tp}|0"
    resp = _send_command(cmd)
    
    logger.info(f"Trade result: {resp[:150]}")
    try:
        data = json.loads(resp)
    except Exception:
        data = {"raw": resp}
    return {"success": True, "data": data}


@router.post("/webhook/tradingview-strategy")
async def tradingview_strategy_webhook(request: Request):
    """Parse TradingView strategy alert format with {{strategy.*}} variables."""
    try:
        body = await request.json()
    except Exception:
        body = {}
    
    # Map TradingView strategy variables
    symbol = (body.get("symbol") or "XAUUSD").upper().replace(" ", "")
    action = (body.get("action") or body.get("strategy_order_action") or "flat").upper()
    
    if "BUY" in action:
        side = "BUY"
    elif "SELL" in action:
        side = "SELL"
    else:
        return {"success": False, "message": f"No action: {action}"}
    
    volume = float(body.get("volume") or body.get("strategy_order_contracts", 0.01))
    price = float(body.get("price") or body.get("strategy_order_price", 0))
    
    cmd = f"PLACE|{symbol}|{side}|{volume}|MARKET|{price}|0|0|0"
    resp = _send_command(cmd)
    
    return {"success": True, "data": json.loads(resp) if resp.startswith("{") else resp}
