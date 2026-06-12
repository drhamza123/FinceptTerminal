"""Local HTTP server — all-in-one proxy for FinceptTerminal.
Run on the user's machine. Routes traffic:
  - /user/*, /chat/* → VPS (64.235.61.6:8155)
  - /mt5/worker/* → local MT5 worker (127.0.0.1:5570)
  - /market/* → local yfinance (direct from user's machine)
"""
import json
import socket
import urllib.request
import urllib.error
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

VPS_HOST = "64.235.61.6"
VPS_PORT = 8155
WORKER_HOST = "127.0.0.1"
WORKER_PORT = 5570


def mt5_symbol(symbol: str) -> str:
    return (symbol or "").strip().upper()


def yfinance_symbol(symbol: str) -> str:
    sym = mt5_symbol(symbol)
    return {
        "XAUUSD": "GC=F", "XAGUSD": "SI=F", "XPTUSD": "PL=F",
        "WTI": "CL=F", "BRENT": "BZ=F",
        "EURUSD": "EURUSD=X", "GBPUSD": "GBPUSD=X", "USDJPY": "JPY=X",
        "AUDUSD": "AUDUSD=X", "USDCAD": "USDCAD=X", "NZDUSD": "NZDUSD=X",
        "USDCHF": "CHF=X", "GBPJPY": "GBPJPY=X", "EURJPY": "EURJPY=X",
        "BTCUSD": "BTC-USD", "ETHUSD": "ETH-USD", "SOLUSD": "SOL-USD",
        "XRPUSD": "XRP-USD", "ADAUSD": "ADA-USD", "DOTUSD": "DOT-USD",
    }.get(sym, symbol)


def worker_timeframe(timeframe: str) -> str:
    tf = (timeframe or "1h").strip()
    return {
        "M1": "1m", "M5": "5m", "M15": "15m", "M30": "30m",
        "H1": "1h", "H4": "4h", "D1": "1d", "W1": "1w",
        "MN1": "1mn", "1wk": "1w",
    }.get(tf.upper(), tf.lower())


def yfinance_interval(timeframe: str) -> str:
    tf = (timeframe or "1h").strip()
    return {
        "M1": "1m", "M5": "5m", "M15": "15m", "M30": "30m",
        "H1": "1h", "H4": "4h", "D1": "1d", "W1": "1wk",
        "MN1": "1mo", "1w": "1wk", "1mn": "1mo",
    }.get(tf.upper(), tf.lower())


def worker_cmd(cmd: dict) -> dict:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.settimeout(10)
        s.connect((WORKER_HOST, WORKER_PORT))
        s.sendall(json.dumps(cmd).encode() + b"\n")
        resp = s.recv(65536).decode().strip()
        return json.loads(resp)
    finally:
        s.close()


def vps_get(path: str) -> dict:
    try:
        r = urllib.request.urlopen(f"http://{VPS_HOST}:{VPS_PORT}{path}", timeout=15)
        return json.loads(r.read())
    except Exception as e:
        return {"success": False, "error": str(e)}


def vps_post(path: str, body: dict) -> dict:
    try:
        data = json.dumps(body).encode()
        req = urllib.request.Request(f"http://{VPS_HOST}:{VPS_PORT}{path}",
                                     data=data, headers={"Content-Type": "application/json"})
        r = urllib.request.urlopen(req, timeout=15)
        return json.loads(r.read())
    except urllib.error.HTTPError as e:
        payload = e.read()
        return json.loads(payload) if payload else {"success": False, "error": str(e)}
    except Exception as e:
        return {"success": False, "error": str(e)}


class LocalHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        qs = parse_qs(parsed.query)

        # ── MT5 Worker endpoints ──
        if path.startswith("/mt5/worker/"):
            action = path.split("/")[-1]
            if action == "rate":
                symbol = qs.get("symbol", ["EURUSD.s"])[0]
                self.json(worker_cmd({"action": "rate", "symbol": symbol}))
            elif action == "candles":
                symbol = qs.get("symbol", ["EURUSD.s"])[0]
                tf = qs.get("timeframe", ["1h"])[0]
                count = int(qs.get("count", [100])[0])
                self.json(worker_cmd({"action": "candles", "symbol": symbol, "timeframe": tf, "count": count}))
            elif action == "balance":
                self.json(worker_cmd({"action": "balance"}))
            elif action == "status":
                self.json(worker_cmd({"action": "ping"}))
            else:
                self.json(worker_cmd({"action": action, **{k: v[0] for k, v in qs.items()}}))
            return

        # ── Market data — try MT5 worker first (live broker data), fall back to yfinance ──
        if path in ("/market/quotes", "/mt5/market/quotes"):
            symbols = qs.get("symbols", ["AAPL"])[0]
            sym_list = [mt5_symbol(s) for s in symbols.split(",") if s.strip()]
            results = []
            errors = []
            for sym in sym_list:
                try:
                    mt5_rate = worker_cmd({"action": "rate", "symbol": sym})
                    if "error" not in mt5_rate:
                        results.append({
                            "symbol": sym, "name": sym,
                            "price": mt5_rate.get("ask", mt5_rate.get("bid", 0)),
                            "bid": mt5_rate.get("bid", 0), "ask": mt5_rate.get("ask", 0),
                            "spread": mt5_rate.get("spread", 0),
                            "high": mt5_rate.get("high", 0), "low": mt5_rate.get("low", 0),
                            "source": "mt5_live",
                        })
                        continue
                except Exception:
                    pass
                errors.append(sym)
            if errors:
                try:
                    import yfinance as yf
                    yf_tickers = [yfinance_symbol(s) for s in errors]
                    tickers = yf.Tickers(" ".join(yf_tickers) if yf_tickers else "")
                    recovered = set()
                    for i, sym in enumerate(errors):
                        if i < len(yf_tickers):
                            t = tickers.tickers.get(yf_tickers[i])
                            info = getattr(t, "info", {}) if t else {}
                            if info:
                                results.append({
                                    "symbol": sym, "name": info.get("shortName", sym),
                                    "price": info.get("currentPrice", info.get("regularMarketPrice", 0)),
                                    "change": info.get("regularMarketChange", 0),
                                    "change_percent": info.get("regularMarketChangePercent", 0),
                                    "high": info.get("dayHigh", 0), "low": info.get("dayLow", 0),
                                    "volume": info.get("volume", 0),
                                    "source": "yfinance",
                                })
                                recovered.add(sym)
                    errors = [s for s in errors if s not in recovered]
                except Exception:
                    pass
            for sym in errors:
                results.append({"symbol": sym, "name": sym, "price": 0, "source": "unavailable",
                                "error": "MT5 worker and yfinance returned no quote"})
            self.json({"success": True, "data": results, "errors": errors})
            return

        if path in ("/market/ohlc", "/mt5/market/ohlc"):
            symbol = mt5_symbol(qs.get("symbol", ["EURUSD"])[0])
            tf = qs.get("timeframe", ["1h"])[0]
            count = int(qs.get("count", [100])[0])
            try:
                mt5_data = worker_cmd({"action": "candles", "symbol": symbol,
                                       "timeframe": worker_timeframe(tf), "count": count})
                if "error" not in mt5_data and mt5_data.get("candles"):
                    self.json({"success": True, "data": mt5_data["candles"], "source": "mt5_live"})
                    return
            except Exception:
                pass
            self.json(self.yfinance_ohlc(symbol, tf, count))
            return

        if path == "/mt5/market/orderbook":
            symbol = mt5_symbol(qs.get("symbol", ["XAUUSD"])[0])
            self.json(self.synthetic_orderbook(symbol))
            return

        # ── All other GET → proxy to VPS ──
        self.json(vps_get(self.path))

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(length)) if length else {}

        # ── MT5 Worker order / connect ──
        if self.path == "/mt5/worker/order":
            result = worker_cmd({
                "action": "market",
                "symbol": body.get("symbol", "EURUSD.s"),
                "side": body.get("side", "BUY"),
                "volume": float(body.get("volume", 0.01)),
                "magic": int(body.get("magic", 831846)),
            })
            self.json({"success": result.get("retcode") == 10009, "data": result, "error": ""})
            return
        if self.path == "/mt5/worker/connect":
            try:
                self.json(worker_cmd({
                    "action": "connect",
                    "login": body.get("login"),
                    "password": body.get("password"),
                    "server": body.get("server"),
                }))
            except Exception as e:
                self.json({"success": False, "error": str(e)})
            return

        # ── MT5 Compile / Deploy → local ──
        if self.path == "/execution/scripts/deploy":
            self.json(self.local_compile(body))
            return

        # ── Auth / Chat → proxy to VPS ──
        self.json(vps_post(self.path, body))

    # ── Local yfinance helpers ──
    def yfinance_quotes(self, symbols: str):
        try:
            import yfinance as yf
            tickers = yf.Tickers([s.strip() for s in symbols.split(",")])
            data = []
            for sym in symbols.split(","):
                sym = sym.strip()
                t = tickers.tickers.get(sym)
                if t and t.info:
                    info = t.info
                    data.append({
                        "symbol": sym, "name": info.get("shortName", sym),
                        "price": info.get("currentPrice", info.get("regularMarketPrice", 0)),
                        "change": info.get("regularMarketChange", 0),
                        "change_percent": info.get("regularMarketChangePercent", 0),
                        "high": info.get("dayHigh", 0), "low": info.get("dayLow", 0),
                        "volume": info.get("volume", 0),
                    })
            return {"success": True, "data": data}
        except Exception as e:
            return {"success": False, "error": str(e)}

    def yfinance_ohlc(self, symbol: str, timeframe: str, count: int):
        try:
            import yfinance as yf
            interval = yfinance_interval(timeframe)
            period_map = {"1m": "1d", "5m": "5d", "15m": "10d", "30m": "1mo",
                          "1h": "1mo", "4h": "3mo", "1d": "1y", "1wk": "2y",
                          "1mo": "5y"}
            period = period_map.get(interval, "1mo")
            t = yf.Ticker(yfinance_symbol(symbol))
            df = t.history(period=period, interval=interval)
            if df.empty:
                return {"success": False, "error": "no data"}
            candles = []
            for idx, row in df.iterrows():
                candles.append({
                    "time": int(idx.timestamp()),
                    "open": float(row["Open"]), "high": float(row["High"]),
                    "low": float(row["Low"]), "close": float(row["Close"]),
                    "volume": int(row["Volume"]), "tick_volume": int(row["Volume"]),
                })
            return {"success": True, "data": candles[-count:], "source": "yfinance"}
        except Exception as e:
            return {"success": False, "error": str(e)}

    def synthetic_orderbook(self, symbol: str):
        try:
            rate = worker_cmd({"action": "rate", "symbol": symbol})
            if "error" not in rate:
                bid = float(rate.get("bid", 0) or 0)
                ask = float(rate.get("ask", 0) or 0)
                mid = (bid + ask) / 2 if bid and ask else float(rate.get("price", 0) or 0)
                spread = abs(ask - bid) if bid and ask else max(mid * 0.0002, 0.01)
                if mid:
                    return self.orderbook_from_mid(symbol, mid, spread, "mt5_live")
        except Exception:
            pass
        base = {"XAUUSD": 2350, "XAGUSD": 31.5, "EURUSD": 1.12,
                "GBPUSD": 1.33, "USDJPY": 155.0, "BTCUSD": 72000,
                "ETHUSD": 3800, "AAPL": 180, "MSFT": 420, "TSLA": 250}.get(symbol, 100)
        return self.orderbook_from_mid(symbol, base, max(base * 0.0002, 0.01), "synthetic")

    def orderbook_from_mid(self, symbol: str, mid: float, spread: float, source: str):
        import random
        best_bid = mid - spread / 2
        best_ask = mid + spread / 2
        bids, asks = [], []
        decimals = 5 if mid < 10 else 2
        for i in range(15):
            bids.append({"price": round(best_bid - i * spread * 0.5, decimals),
                         "volume": round(random.uniform(0.5, 10), 2)})
            asks.append({"price": round(best_ask + i * spread * 0.5, decimals),
                         "volume": round(random.uniform(0.5, 10), 2)})
        return {"success": True, "symbol": symbol, "last_price": mid,
                "spread": spread, "bids": bids, "asks": asks, "source": source}

    def json(self, data):
        resp = json.dumps(data).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(resp)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(resp)

    def log_message(self, fmt, *args):
        print(f"[Local] {args[0]} {args[1]} {args[2]}")


    def local_compile(self, body: dict) -> dict:
        """Compile MQL5 code locally using MT5's MetaEditor."""
        import subprocess, tempfile, os, shutil

        source = body.get("source", "")
        filename = body.get("filename", "script.mq5")

        if not source:
            return {"success": False, "error": "No source code"}

        # Find MetaEditor
        metaeditor = None
        for p in [
            r"C:\Program Files\MetaTrader 5\metaeditor64.exe",
            r"C:\Program Files\MetaTrader 5\metaeditor.exe",
            os.path.expandvars(r"%PROGRAMFILES%\MetaTrader 5\metaeditor64.exe"),
        ]:
            if os.path.exists(p):
                metaeditor = p
                break

        if not metaeditor:
            return {"success": False, "error": "MetaEditor not found. Install MetaTrader 5 first.",
                    "hint": "Download from your broker's website or https://www.metatrader5.com"}

        # Write source to temp file and compile
        tmp_dir = tempfile.mkdtemp(prefix="fincept_mql_")
        src_path = os.path.join(tmp_dir, filename)
        with open(src_path, "w", encoding="utf-8") as f:
            f.write(source)

        ex5_path = src_path.replace(".mq5", ".ex5").replace(".mq4", ".ex4")
        log_path = os.path.join(tmp_dir, "compile.log")

        try:
            result = subprocess.run(
                [metaeditor, f"/compile:{src_path}", f"/log:{log_path}"],
                capture_output=True, timeout=60, cwd=tmp_dir
            )
            compile_ok = os.path.exists(ex5_path) and os.path.getsize(ex5_path) > 0

            log_content = ""
            if os.path.exists(log_path):
                with open(log_path) as f:
                    log_content = f.read()

            if compile_ok:
                # Copy to MT5 Experts folder
                experts_dir = os.path.expandvars(r"%APPDATA%\MetaQuotes\Terminal\Common\Experts")
                if not os.path.exists(experts_dir):
                    os.makedirs(experts_dir, exist_ok=True)
                dest = os.path.join(experts_dir, os.path.basename(ex5_path))
                shutil.copy2(ex5_path, dest)

                self.log_message("", "COMPILE", f"OK → {dest}")
                return {"success": True, "message": f"Compiled and deployed to {dest}",
                        "data": {"path": dest, "log": log_content[:500]}}

            errors = [l for l in log_content.split("\n") if "error" in l.lower() or "warning" in l.lower()]
            return {"success": False, "error": "Compilation failed",
                    "data": {"errors": errors[:10], "log": log_content[:500]}}

        except subprocess.TimeoutExpired:
            return {"success": False, "error": "Compilation timed out (60s)"}
        except FileNotFoundError:
            return {"success": False, "error": "MetaEditor not found at " + metaeditor}
        except Exception as e:
            return {"success": False, "error": str(e)}
        finally:
            try:
                shutil.rmtree(tmp_dir, ignore_errors=True)
            except Exception:
                pass


if __name__ == "__main__":
    import sys
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8157
    print(f"FinceptTerminal Local Server on http://127.0.0.1:{port}")
    print(f"Set your app's API URL to: http://127.0.0.1:{port}")
    print(f"  /user/*, /chat/* → VPS proxy")
    print(f"  /mt5/worker/*   → local MT5 worker")
    print(f"  /market/*       → local yfinance")
    HTTPServer(("127.0.0.1", port), LocalHandler).serve_forever()
