import logging
import time
from typing import Any

from app.config import settings

logger = logging.getLogger("guardian.mt5_direct")

try:
    import MetaTrader5 as mt5  # type: ignore
except Exception:  # pragma: no cover - package is Windows/MT5-terminal specific.
    mt5 = None

# Cached connection state
_connected: bool | None = None
_connect_time: float = 0
_CONNECT_CACHE_TTL: float = 86400.0  # 24 hours - stay connected


def is_available() -> bool:
    return mt5 is not None


def is_enabled() -> bool:
    return bool(settings.MT5_DIRECT_ENABLED)


def reset_connection():
    """Force re-initialization on next connect() call."""
    global _connected, _connect_time
    _connected = None
    _connect_time = 0


_terminal_proc = None


def _load_creds() -> dict:
    import json, os
    config_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), "config", "mt5_creds.json")
    if os.path.exists(config_path):
        try:
            with open(config_path) as f:
                return json.load(f)
        except Exception:
            pass
    return {"login": "831846", "password": "i3Fc870", "server": "DooTechnology-Demo"}


def _ensure_terminal():
    global _terminal_proc
    import subprocess, time
    if _terminal_proc and _terminal_proc.poll() is None:
        return
    creds = _load_creds()
    login = creds.get('login', '831846')
    password = creds.get('password', 'i3Fc870')
    server = creds.get('server', 'DooTechnology-Demo')
    term_path = r"C:\Program Files\MetaTrader 5\terminal64.exe"
    try:
        # Use subprocess with winsta0\default desktop for proper IPC named pipe
        import ctypes
        si = subprocess.STARTUPINFO()
        si.lpDesktop = "winsta0\\default"
        _terminal_proc = subprocess.Popen(
            [term_path, f"/login:{login}", f"/password:{password}", f"/server:{server}"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            startupinfo=si)
        logger.info("Started MT5 terminal with account %s on %s", login, server)
    except Exception as e:
        logger.warning("Failed to start MT5 terminal: %s", e)


def connect(force: bool = False) -> tuple[bool, str]:
    global _connected, _connect_time

    if not is_enabled():
        return False, "MT5 direct mode is disabled"
    if mt5 is None:
        return False, "MetaTrader5 package unavailable"

    now = time.time()
    if not force and _connected is not None and (now - _connect_time) < _CONNECT_CACHE_TTL:
        return (_connected, "OK") if _connected else (False, "Cached failure")

    _ensure_terminal()
    _connect_time = now

    ok = mt5.initialize()
    if not ok:
        _connected = False
        err = mt5.last_error()
        logger.warning("MT5 init failed: %s", err)
        return False, f"MT5 init failed: {err}"

    account = mt5.account_info()
    if account is None:
        _connected = False
        logger.warning("MT5 init: no account info")
        return False, "No MT5 account info"

    _connected = True
    login = getattr(account, "login", "")
    return True, f"Connected to MT5 account {login}" if login else "Connected to MT5"


def _as_dict(obj: Any) -> dict:
    if obj is None:
        return {}
    if hasattr(obj, "_asdict"):
        return obj._asdict()
    return dict(obj)


def _filling_type() -> int:
    value = str(settings.MT5_DIRECT_FILLING).upper()
    if value == "IOC":
        return mt5.ORDER_FILLING_IOC
    if value == "RETURN":
        return mt5.ORDER_FILLING_RETURN
    return mt5.ORDER_FILLING_FOK


def normalize_symbol(symbol: str) -> str:
    raw = (symbol or "").strip().upper()
    aliases = {
        "EURUSD=X": "EURUSD",
        "GBPUSD=X": "GBPUSD",
        "USDJPY=X": "USDJPY",
        "AUDUSD=X": "AUDUSD",
        "USDCAD=X": "USDCAD",
        "USDCHF=X": "USDCHF",
        "NZDUSD=X": "NZDUSD",
        "EURCHF=X": "EURCHF",
        "GBPJPY=X": "GBPJPY",
        "EURJPY=X": "EURJPY",
        "GC=F": "XAUUSD",
        "SI=F": "XAGUSD",
        "PL=F": "XPTUSD",
        "PA=F": "XPDUSD",
        "HG=F": "XCUUSD",
        "CL=F": "WTI",
        "BZ=F": "BRENT",
        "NG=F": "NGAS",
        "BTC-USD": "BTCUSD",
        "ETH-USD": "ETHUSD",
        "BNB-USD": "BNBUSD",
        "SOL-USD": "SOLUSD",
        "XRP-USD": "XRPUSD",
        "ADA-USD": "ADAUSD",
        "DOGE-USD": "DOGEUSD",
        "DOT-USD": "DOTUSD",
        "LTC-USD": "LTCUSD",
        "^GSPC": "US500",
        "^DJI": "US30",
        "^IXIC": "NAS100",
        "^RUT": "US2000",
        "^FTSE": "UK100",
        "^GDAXI": "GER40",
        "^FCHI": "FRA40",
        "^N225": "JP225",
        "^HSI": "HK50",
        "^VIX": "VIX",
        "DX-Y.NYB": "DXY",
    }
    return aliases.get(raw, raw.replace("/", "").replace("-", ""))


def select_symbol(symbol: str, _orig: str | None = None) -> tuple[bool, str, Any]:
    ok, msg = connect()
    if not ok:
        return False, msg, None
    sym = normalize_symbol(symbol)
    if mt5.symbol_select(sym, True):
        info = mt5.symbol_info(sym)
        if info is not None:
            return True, "OK", info
    # Fallback: try the original symbol as-is (brokers like DooTechnology use .s suffix)
    fallback = _orig or symbol
    if fallback != sym and mt5.symbol_select(fallback, True):
        info = mt5.symbol_info(fallback)
        if info is not None:
            return True, "OK", info
    return False, f"MT5 symbol_select failed for {sym} (tried {fallback}): {mt5.last_error()}", None


def live_tick(symbol: str) -> dict | None:
    ok, msg, info = select_symbol(symbol)
    if not ok:
        logger.warning("MT5 live tick unavailable: %s", msg)
        return None
    mt5_symbol = getattr(info, "name", normalize_symbol(symbol))
    tick = mt5.symbol_info_tick(mt5_symbol)
    if tick is None:
        return None
    point = getattr(info, "point", 0.0) or 0.0
    bid = float(getattr(tick, "bid", 0.0) or 0.0)
    ask = float(getattr(tick, "ask", 0.0) or 0.0)
    spread_points = round((ask - bid) / point) if point and ask and bid else 0
    return {
        "symbol": mt5_symbol,
        "bid": bid,
        "ask": ask,
        "last": float(getattr(tick, "last", 0.0) or 0.0),
        "time": int(getattr(tick, "time", time.time()) or time.time()),
        "spread": spread_points,
        "point": point,
        "source": "mt5_direct",
    }


def market_payload(symbol: str) -> dict | None:
    tick = live_tick(symbol)
    if not tick:
        return None
    bid = tick["bid"]
    ask = tick["ask"]
    last = tick["last"] or ((bid + ask) / 2 if bid and ask else bid or ask)
    spread = ask - bid if ask and bid else 0.0
    spread_pct = spread / last if last else 0.0
    return {
        "type": "market_data",
        "data": {
            "symbol": tick["symbol"],
            "bids": [[bid, 1.0]] if bid else [],
            "asks": [[ask, 1.0]] if ask else [],
            "bid": bid,
            "ask": ask,
            "spread": spread,
            "spread_points": tick["spread"],
            "spread_pct": spread_pct,
            "price": last,
            "time": tick["time"],
            "source": "mt5_direct",
        },
    }


def quote_payload(symbol: str) -> dict | None:
    payload = market_payload(symbol)
    if not payload:
        return None
    data = payload["data"]
    symbol = data.get("symbol", symbol.upper())
    price = float(data.get("price", 0.0) or 0.0)

    change = 0.0
    change_percent = 0.0
    high = 0.0
    low = 0.0
    volume = 0.0
    bars = ohlc(symbol, "D1", 2) or []
    if bars:
        latest = bars[-1]
        high = float(latest.get("high", 0.0) or 0.0)
        low = float(latest.get("low", 0.0) or 0.0)
        volume = float(latest.get("volume", 0.0) or 0.0)
        previous_close = float((bars[-2] if len(bars) > 1 else latest).get("close", 0.0) or 0.0)
        if previous_close:
            change = price - previous_close
            change_percent = (change / previous_close) * 100.0

    return {
        "symbol": symbol,
        "name": symbol,
        "price": price,
        "change": change,
        "change_percent": change_percent,
        "high": high,
        "low": low,
        "volume": volume,
        "bid": data.get("bid", 0.0),
        "ask": data.get("ask", 0.0),
        "time": data.get("time", int(time.time())),
        "source": "mt5_direct",
    }


def execute_market_order(symbol: str, volume: float, side: str, sl: float = 0.0, tp: float = 0.0) -> dict:
    ok, msg, info = select_symbol(symbol)
    if not ok:
        return {"success": False, "status": "REJECTED", "error": msg}

    symbol = getattr(info, "name", normalize_symbol(symbol))
    tick = mt5.symbol_info_tick(symbol)
    if tick is None:
        return {"success": False, "status": "REJECTED", "error": "No live MT5 tick data"}

    is_buy = side.upper() == "BUY"
    price = float(tick.ask if is_buy else tick.bid)
    request = {
        "action": mt5.TRADE_ACTION_DEAL,
        "symbol": symbol,
        "volume": float(volume),
        "type": mt5.ORDER_TYPE_BUY if is_buy else mt5.ORDER_TYPE_SELL,
        "price": price,
        "deviation": int(settings.MT5_DIRECT_DEVIATION),
        "magic": int(settings.MT5_DIRECT_MAGIC),
        "comment": "FinceptTerminal Execution",
        "type_time": mt5.ORDER_TIME_GTC,
        "type_filling": _filling_type(),
    }
    if sl and sl > 0:
        request["sl"] = float(sl)
    if tp and tp > 0:
        request["tp"] = float(tp)

    start = time.perf_counter()
    result = mt5.order_send(request)
    latency_ms = round((time.perf_counter() - start) * 1000, 2)
    if result is None:
        return {"success": False, "status": "REJECTED", "error": str(mt5.last_error()), "latency_ms": latency_ms}

    data = _as_dict(result)
    if result.retcode != mt5.TRADE_RETCODE_DONE:
        return {
            "success": False,
            "status": "REJECTED",
            "error": getattr(result, "comment", "MT5 order rejected"),
            "retcode": int(result.retcode),
            "latency_ms": latency_ms,
            "raw": data,
        }

    return {
        "success": True,
        "status": "FILLED",
        "ticket": int(getattr(result, "order", 0) or getattr(result, "deal", 0) or 0),
        "deal": int(getattr(result, "deal", 0) or 0),
        "symbol": symbol,
        "side": side.upper(),
        "volume": float(volume),
        "fill_price": float(getattr(result, "price", price) or price),
        "latency_ms": latency_ms,
        "retcode": int(result.retcode),
        "raw": data,
    }


def open_positions() -> list[dict]:
    ok, msg = connect()
    if not ok:
        logger.warning("MT5 positions unavailable: %s", msg)
        return []
    positions = mt5.positions_get()
    if not positions:
        return []
    rows = []
    for pos in positions:
        rows.append({
            "ticket": int(pos.ticket),
            "symbol": pos.symbol,
            "side": "BUY" if pos.type == mt5.POSITION_TYPE_BUY else "SELL",
            "type": "BUY" if pos.type == mt5.POSITION_TYPE_BUY else "SELL",
            "volume": float(pos.volume),
            "entry_price": float(pos.price_open),
            "open_price": float(pos.price_open),
            "current_price": float(pos.price_current),
            "pnl": float(pos.profit),
            "profit": float(pos.profit),
            "swap": float(pos.swap),
            "opened_at": int(pos.time),
            "source": "mt5_direct",
        })
    return rows


def account_summary() -> dict | None:
    ok, msg = connect()
    if not ok:
        logger.warning("MT5 account unavailable: %s", msg)
        return None
    account = mt5.account_info()
    if account is None:
        return None
    return {
        "login": account.login,
        "balance": float(account.balance),
        "equity": float(account.equity),
        "margin": float(account.margin),
        "free_margin": float(account.margin_free),
        "margin_free": float(account.margin_free),
        "margin_level": float(account.margin_level),
        "currency": account.currency,
        "server": account.server,
        "open_positions": len(open_positions()),
        "source": "mt5_direct",
    }


def _timeframe(value: str) -> int:
    mapping = {
        "M1": mt5.TIMEFRAME_M1,
        "M5": mt5.TIMEFRAME_M5,
        "M15": mt5.TIMEFRAME_M15,
        "M30": mt5.TIMEFRAME_M30,
        "H1": mt5.TIMEFRAME_H1,
        "H4": mt5.TIMEFRAME_H4,
        "D1": mt5.TIMEFRAME_D1,
        "W1": mt5.TIMEFRAME_W1,
        "MN1": mt5.TIMEFRAME_MN1,
    }
    return mapping.get(value.upper(), mt5.TIMEFRAME_H1)


def ohlc(symbol: str, timeframe: str = "H1", count: int = 100) -> list[dict] | None:
    ok, msg, info = select_symbol(symbol)
    if not ok:
        logger.warning("MT5 OHLC unavailable: %s", msg)
        return None
    symbol = getattr(info, "name", normalize_symbol(symbol))
    rates = mt5.copy_rates_from_pos(symbol, _timeframe(timeframe), 0, int(count))
    if rates is None:
        logger.warning("MT5 copy_rates_from_pos failed for %s: %s", symbol, mt5.last_error())
        return None
    rows = []
    for rate in rates:
        rows.append({
            "time": int(rate["time"]),
            "open": float(rate["open"]),
            "high": float(rate["high"]),
            "low": float(rate["low"]),
            "close": float(rate["close"]),
            "volume": int(rate["tick_volume"]),
            "spread": int(rate["spread"]),
            "real_volume": int(rate["real_volume"]),
            "source": "mt5_direct",
        })
    return rows
