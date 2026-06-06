import json
import logging

logger = logging.getLogger("guardian.mt5")

EA_MSG_TYPES = {"hello", "heartbeat", "trade", "error", "trade_history", "goodbye"}


def parse_ea_message(raw: str) -> dict | None:
    try:
        msg = json.loads(raw)
    except json.JSONDecodeError:
        logger.warning("Invalid JSON from EA: %s", raw[:200])
        return None
    if not isinstance(msg, dict) or "type" not in msg:
        return None
    if msg["type"] not in EA_MSG_TYPES:
        logger.warning("Unknown EA message type: %s", msg["type"])
        return None
    return msg


def make_hello(ea_name: str, magic: int, symbol: str, tf: str, balance: float, equity: float) -> str:
    return json.dumps({"type": "hello", "ea_name": ea_name, "magic": magic,
                        "symbol": symbol, "tf": tf, "balance": balance, "equity": equity}) + "\n"


def cmd_set_params(ea_key: str, params: dict) -> str:
    return json.dumps({"type": "set_params", "ea_key": ea_key, "params": params}) + "\n"


def cmd_close_all(ea_key: str) -> str:
    return json.dumps({"type": "close_all", "ea_key": ea_key}) + "\n"


def cmd_close_order(ea_key: str, order: int) -> str:
    return json.dumps({"type": "close_order", "ea_key": ea_key, "order": order}) + "\n"


def cmd_modify_order(ea_key: str, order: int, sl: float, tp: float) -> str:
    return json.dumps({"type": "modify_order", "ea_key": ea_key, "order": order, "sl": sl, "tp": tp}) + "\n"


def cmd_shutdown(ea_key: str) -> str:
    return json.dumps({"type": "shutdown", "ea_key": ea_key}) + "\n"


def cmd_ping(ea_key: str) -> str:
    return json.dumps({"type": "ping", "ea_key": ea_key}) + "\n"


# ── Advanced Order Types ────────────────────────────────────────


def cmd_bracket_order(ea_key: str, side: str, volume: float, symbol: str,
                      entry_price: float, sl: float, tp: float) -> str:
    """Bracket order: entry + SL + TP as single unit."""
    return json.dumps({
        "type": "bracket_order", "ea_key": ea_key,
        "side": side, "volume": volume, "symbol": symbol,
        "entry": entry_price, "sl": sl, "tp": tp
    }) + "\n"


def cmd_oco_order(ea_key: str, side: str, volume: float, symbol: str,
                  entry_price: float, stop_price: float, limit_price: float) -> str:
    """One-Cancels-Other: stop and limit order paired."""
    return json.dumps({
        "type": "oco_order", "ea_key": ea_key,
        "side": side, "volume": volume, "symbol": symbol,
        "entry": entry_price, "stop_price": stop_price, "limit_price": limit_price
    }) + "\n"


def cmd_oto_order(ea_key: str, trigger_side: str, trigger_price: float,
                  target_side: str, target_volume: float, symbol: str,
                  target_price: float, sl: float = 0, tp: float = 0) -> str:
    """One-Triggers-Other: first order triggers second."""
    return json.dumps({
        "type": "oto_order", "ea_key": ea_key,
        "trigger_side": trigger_side, "trigger_price": trigger_price,
        "target_side": target_side, "target_volume": target_volume,
        "symbol": symbol, "target_price": target_price, "sl": sl, "tp": tp
    }) + "\n"


def cmd_trailing_stop(ea_key: str, order: int, trailing_distance: float) -> str:
    """Activate trailing stop on an open position."""
    return json.dumps({
        "type": "trailing_stop", "ea_key": ea_key,
        "order": order, "trailing_distance": trailing_distance
    }) + "\n"


def cmd_pending_order(ea_key: str, order_type: str, volume: float,
                      symbol: str, price: float, sl: float = 0,
                      tp: float = 0, expiration: int = 0) -> str:
    """Pending order: buy/sell limit/stop.
    order_type: 'buy_limit', 'sell_limit', 'buy_stop', 'sell_stop'
    """
    return json.dumps({
        "type": "pending_order", "ea_key": ea_key,
        "order_type": order_type, "volume": volume, "symbol": symbol,
        "price": price, "sl": sl, "tp": tp, "expiration": expiration
    }) + "\n"


def cmd_cancel_pending(ea_key: str, ticket: int) -> str:
    """Cancel a pending order."""
    return json.dumps({"type": "cancel_pending", "ea_key": ea_key, "ticket": ticket}) + "\n"


def cmd_iceberg_order(ea_key: str, side: str, volume: float, symbol: str,
                      price: float, visible_volume: float, sl: float = 0, tp: float = 0) -> str:
    """Iceberg order: display only a portion of total volume."""
    return json.dumps({
        "type": "iceberg_order", "ea_key": ea_key,
        "side": side, "volume": volume, "symbol": symbol,
        "price": price, "visible_volume": visible_volume, "sl": sl, "tp": tp
    }) + "\n"


MQL5_AGENT_PROMPT = """You are an MQL5 expert. Generate complete, compilable Expert Advisor code.
Core MQL5 API reference:
- Trade execution: Use `CTrade` class (`m_trade.Buy()`, `m_trade.Sell()`, `m_trade.PositionClose()`)
- Order/Position select: `PositionGetTicket()`, `PositionGetDouble(POSITION_PROFIT)`
- Deal select: `HistoryDealGetDouble(DEAL_PROFIT)`, `HistoryDealGetInteger(DEAL_MAGIC)`
- Socket functions: `SocketCreate(SOCKET_AF_INET, SOCKET_STREAM, SOCKET_IPPROTO_TCP)`, `SocketConnect()`, `SocketSend()`, `SocketRead()`, `SocketClose()`, `SocketIsConnected()`, `SocketIsReadable()`
Rules:
1. Always include `OnInit()`, `OnDeinit()`, `OnTick()`, `OnTimer()`, `OnTradeTransaction()`
2. Use `SocketCreate` with explicit parameters. Use `uchar` arrays for `SocketRead` and `SocketSend`.
3. Include runtime variables (`gLotSize`, etc.) modified by `set_params` — never write to `input` variables.
4. Use `OnTradeTransaction()` for trade event capture. Check `DEAL_MAGIC` to filter trades.
5. Keep socket reads strictly non-blocking (`timeout=0`) inside `OnTick()`.
"""

AGENT_PROMPTS = {
    "mql5_generator": MQL5_AGENT_PROMPT,
}
