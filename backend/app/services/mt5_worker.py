"""MT5 Worker — standalone TCP server for MetaTrader 5 execution.

Runs in the user's RDP session (Windows session 1) where MetaTrader 5
terminal is logged in.  Listens on TCP 5570 for JSON commands, executes
them via the MetaTrader5 Python module, and returns JSON responses.

Usage:
    python mt5_worker.py

Accepts:
    {"action": "market", "symbol": "EURUSD.s", "side": "BUY", "volume": 0.01, "magic": 831846}
    {"action": "ping"}
"""

import json
import logging
import signal
import socket
import sys
import time
from datetime import datetime, timezone

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.FileHandler("mt5_worker.log"),
        logging.StreamHandler(sys.stdout),
    ],
)
logger = logging.getLogger("mt5_worker")

HOST = "127.0.0.1"
PORT = 5570
RECV_SIZE = 65536
BACKLOG = 5

_mt5 = None


def _import_mt5():
    global _mt5
    if _mt5 is not None:
        return _mt5
    try:
        import MetaTrader5 as mt5

        _mt5 = mt5
    except ImportError as exc:
        logger.error("MetaTrader5 module not available: %s", exc)
        raise
    return _mt5


_worker_login = None
_worker_password = None
_worker_server = None

def set_credentials(login=None, password=None, server=None):
    global _worker_login, _worker_password, _worker_server
    _worker_login = login
    _worker_password = password
    _worker_server = server
    logger.info("Credentials set: login=%s server=%s", login, server)


def _ensure_initialized() -> bool:
    mt5 = _import_mt5()
    if mt5.terminal_info() is not None and mt5.account_info() is not None:
        return True
    kwargs = {}
    if _worker_login: kwargs["login"] = int(_worker_login)
    if _worker_password: kwargs["password"] = _worker_password
    if _worker_server: kwargs["server"] = _worker_server
    initialized = mt5.initialize(**kwargs) if kwargs else mt5.initialize()
    if initialized:
        logger.info(
            "MT5 initialized — terminal: %s, account: %s",
            mt5.terminal_info().name if mt5.terminal_info() else "?",
            mt5.account_info().login if mt5.account_info() else "?",
        )
    else:
        error = mt5.last_error()
        logger.error("MT5 initialize failed: %s", error)
    return initialized


def _account_balance() -> float:
    mt5 = _import_mt5()
    info = mt5.account_info()
    if info is None:
        return 0.0
    return info.balance


def _filling_mode_name(mode: int) -> str:
    mt5 = _import_mt5()
    mapping = {
        mt5.ORDER_FILLING_FOK: "FOK",
        mt5.ORDER_FILLING_IOC: "IOC",
        mt5.ORDER_FILLING_RETURN: "RETURN",
    }
    return mapping.get(mode, f"UNKNOWN({mode})")


def _try_order(symbol: str, side: str, volume: float, magic: int,
               filling_flags: int) -> dict:
    mt5 = _import_mt5()
    order_type = mt5.ORDER_TYPE_BUY if side.upper() == "BUY" else mt5.ORDER_TYPE_SELL

    tick = mt5.symbol_info_tick(symbol)
    if tick is None:
        return {"retcode": -1, "error": f"symbol_info_tick failed for {symbol}"}

    price = tick.ask if order_type == mt5.ORDER_TYPE_BUY else tick.bid
    if price is None or price == 0.0:
        return {"retcode": -2, "error": f"invalid price for {symbol}"}

    request = {
        "action": mt5.TRADE_ACTION_DEAL,
        "symbol": symbol,
        "volume": volume,
        "type": order_type,
        "price": price,
        "deviation": 20,
        "magic": magic,
        "comment": "mt5_worker",
        "type_time": mt5.ORDER_TIME_GTC,
        "type_filling": filling_flags,
    }

    result = mt5.order_send(request)
    if result is None:
        return {"retcode": -3, "error": "order_send returned None"}

    return {
        "retcode": result.retcode,
        "order_id": result.order,
        "price": result.price,
        "comment": result.comment,
        "volume": result.volume,
        "request_id": result.request_id if hasattr(result, "request_id") else 0,
    }


def _execute_market_order(symbol: str, side: str, volume: float,
                          magic: int) -> dict:
    mt5 = _import_mt5()

    if not _ensure_initialized():
        return {"retcode": -10, "order_id": 0, "price": 0.0,
                "comment": "MT5 not initialized"}

    if mt5.symbol_info(symbol) is None:
        return {"retcode": -11, "order_id": 0, "price": 0.0,
                "comment": f"symbol {symbol} not found"}

    filling_modes = [mt5.ORDER_FILLING_FOK, mt5.ORDER_FILLING_IOC, mt5.ORDER_FILLING_RETURN]

    attempts = []
    for fm in filling_modes:
        result = _try_order(symbol, side, volume, magic, fm)
        attempts.append((fm, result))
        if result["retcode"] == mt5.TRADE_RETCODE_DONE:
            logger.info("Fill success: mode=%s order_id=%s price=%s",
                        fm, result["order_id"], result["price"])
            return result

    last = attempts[-1][1] if attempts else {"retcode": -99, "order_id": 0,
                                              "price": 0.0, "comment": "all filling modes exhausted"}
    fm_names = {mt5.ORDER_FILLING_FOK: "FOK", mt5.ORDER_FILLING_IOC: "IOC", mt5.ORDER_FILLING_RETURN: "RETURN"}
    last["attempts"] = [{"mode": fm_names.get(a[0], str(a[0])), "retcode": a[1]["retcode"]} for a in attempts]
    last["comment"] = f"All filling modes exhausted: {last['attempts']}"
    logger.error("All filling modes failed: %s", last["attempts"])
    return last


def _handle_ping() -> str:
    balance = 0.0
    error = None
    try:
        if _ensure_initialized():
            balance = _account_balance()
        else:
            error = "MT5 not initialized"
    except Exception as exc:
        error = str(exc)

    response = {"status": "ok" if not error else "error", "balance": balance}
    if error:
        response["error"] = error
    return json.dumps(response) + "\n"


def _handle_market(cmd: dict) -> str:
    symbol = cmd.get("symbol", "")
    side = cmd.get("side", "BUY")
    volume = float(cmd.get("volume", 0.01))
    magic = int(cmd.get("magic", 831846))

    if not symbol:
        return json.dumps({"retcode": -20, "order_id": 0, "price": 0.0,
                           "comment": "symbol is required"}) + "\n"

    start = time.perf_counter()
    result = _execute_market_order(symbol, side, volume, magic)
    elapsed_ms = round((time.perf_counter() - start) * 1000, 2)
    result["latency_ms"] = elapsed_ms
    logger.info("Order completed in %sms: retcode=%s order_id=%s price=%s",
                elapsed_ms, result.get("retcode"), result.get("order_id"),
                result.get("price"))
    return json.dumps(result) + "\n"


def _handle_rate(cmd: dict) -> str:
    symbol = cmd.get("symbol", "")
    if not symbol:
        return json.dumps({"error": "symbol required"}) + "\n"
    mt5 = _import_mt5()
    _ensure_initialized()
    tick = mt5.symbol_info_tick(symbol)
    if tick is None:
        return json.dumps({"error": f"no tick for {symbol}"}) + "\n"
    info = mt5.symbol_info(symbol)
    return json.dumps({
        "symbol": symbol,
        "bid": tick.bid,
        "ask": tick.ask,
        "spread": tick.ask - tick.bid if tick.ask and tick.bid else 0,
        "time": tick.time,
        "high": info.price_high if hasattr(info, "price_high") else (tick.ask if tick.ask else 0),
        "low": info.price_low if hasattr(info, "price_low") else (tick.bid if tick.bid else 0),
        "volume": tick.volume or 0,
    }) + "\n"


def _handle_candles(cmd: dict) -> str:
    symbol = cmd.get("symbol", "")
    timeframe = cmd.get("timeframe", "1h")
    count = int(cmd.get("count", 100))
    if not symbol:
        return json.dumps({"error": "symbol required"}) + "\n"
    mt5 = _import_mt5()
    _ensure_initialized()
    tf_map = {
        "1m": mt5.TIMEFRAME_M1, "5m": mt5.TIMEFRAME_M5, "15m": mt5.TIMEFRAME_M15,
        "30m": mt5.TIMEFRAME_M30, "1h": mt5.TIMEFRAME_H1, "4h": mt5.TIMEFRAME_H4,
        "1d": mt5.TIMEFRAME_D1, "1w": mt5.TIMEFRAME_W1, "1mn": mt5.TIMEFRAME_MN1,
    }
    tf = tf_map.get(timeframe, mt5.TIMEFRAME_H1)
    rates = mt5.copy_rates_from_pos(symbol, tf, 0, count)
    if rates is None:
        return json.dumps({"error": f"no rates for {symbol} {timeframe}"}) + "\n"
    candles = []
    for r in rates:
        candles.append({
            "time": int(r[0]),
            "open": float(r[1]),
            "high": float(r[2]),
            "low": float(r[3]),
            "close": float(r[4]),
            "tick_volume": int(r[5]),
            "spread": int(r[6]),
            "real_volume": int(r[7]),
        })
    return json.dumps({"symbol": symbol, "timeframe": timeframe, "candles": candles}) + "\n"


def _handle_connect(cmd: dict) -> str:
    login = cmd.get("login")
    password = cmd.get("password")
    server = cmd.get("server")
    set_credentials(login, password, server)
    mt5 = _import_mt5()
    ok = _ensure_initialized()
    return json.dumps({"status": "ok" if ok else "error",
                        "account": mt5.account_info().login if ok and mt5.account_info() else None}) + "\n"


def _handle_balance(cmd: dict) -> str:
    mt5 = _import_mt5()
    _ensure_initialized()
    info = mt5.account_info()
    if info is None:
        return json.dumps({"error": "no account info"}) + "\n"
    positions = mt5.positions_get()
    return json.dumps({
        "balance": info.balance,
        "equity": info.equity,
        "margin": info.margin,
        "margin_free": info.margin_free,
        "margin_level": info.margin_level,
        "profit": info.profit,
        "leverage": info.leverage,
        "currency": info.currency,
        "login": info.login,
        "server": info.server,
        "open_positions": len(positions) if positions else 0,
    }) + "\n"


def _handle_command(cmd: dict) -> str:
    action = cmd.get("action", "").strip().lower()
    if action == "ping":
        return _handle_ping()
    elif action == "market":
        return _handle_market(cmd)
    elif action == "rate":
        return _handle_rate(cmd)
    elif action == "candles":
        return _handle_candles(cmd)
    elif action == "connect":
        return _handle_connect(cmd)
    elif action == "balance":
        return _handle_balance(cmd)
    else:
        return json.dumps({"retcode": -99, "order_id": 0, "price": 0.0,
                           "comment": f"unknown action: {action}"}) + "\n"


def _process_client(conn: socket.socket):
    buffer = b""
    try:
        while True:
            raw = conn.recv(RECV_SIZE)
            if not raw:
                break
            buffer += raw
            try:
                text = buffer.decode("utf-8").strip()
            except UnicodeDecodeError:
                buffer = b""
                continue

            try:
                cmd = json.loads(text)
            except json.JSONDecodeError as exc:
                conn.sendall(
                    json.dumps({"retcode": -98, "order_id": 0, "price": 0.0,
                                "comment": f"invalid JSON: {exc}"}).encode()
                    + b"\n"
                )
                buffer = b""
                continue

            buffer = b""
            response = _handle_command(cmd)
            conn.sendall(response.encode())
    except ConnectionResetError:
        logger.debug("Client disconnected (reset)")
    except Exception as exc:
        logger.error("Client handler error: %s", exc)
        try:
            conn.sendall(
                json.dumps({"retcode": -1, "order_id": 0, "price": 0.0,
                            "comment": f"internal error: {exc}"}).encode()
                + b"\n"
            )
        except Exception:
            pass


def main():
    _import_mt5()
    _ensure_initialized()

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((HOST, PORT))
    server.listen(BACKLOG)
    server.settimeout(1.0)

    shutdown_flag = False

    def _shutdown(signum, frame):
        nonlocal shutdown_flag
        if shutdown_flag:
            return
        shutdown_flag = True
        logger.info("Received signal %s, shutting down...", signum)
        try:
            server.close()
        except Exception:
            pass

    signal.signal(signal.SIGTERM, _shutdown)
    signal.signal(signal.SIGINT, _shutdown)

    logger.info("MT5 Worker listening on %s:%s", HOST, PORT)
    logger.info("PID: %s", signal is not None or "N/A")

    while not shutdown_flag:
        try:
            conn, addr = server.accept()
            logger.info("Connection from %s", addr)
            _process_client(conn)
            conn.close()
            logger.debug("Connection closed from %s", addr)
        except socket.timeout:
            continue
        except OSError:
            if shutdown_flag:
                break
            logger.exception("Accept error")
        except Exception as exc:
            logger.exception("Unexpected error: %s", exc)

    mt5 = _import_mt5()
    try:
        mt5.shutdown()
    except Exception:
        pass
    logger.info("MT5 Worker stopped")


if __name__ == "__main__":
    main()
