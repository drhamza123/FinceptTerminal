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


def worker_cmd(cmd: dict) -> dict:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10)
    s.connect((WORKER_HOST, WORKER_PORT))
    s.sendall(json.dumps(cmd).encode() + b"\n")
    resp = s.recv(65536).decode().strip()
    s.close()
    return json.loads(resp)


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
        return json.loads(e.read()) if e.read() else {"success": False, "error": str(e)}
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
        if path == "/market/quotes":
            symbols = qs.get("symbols", ["AAPL"])[0]
            # Try MT5 worker for each symbol
            sym_list = [s.strip() for s in symbols.split(",")]
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
                results.append({"symbol": sym, "name": sym, "price": 0, "source": "mt5_unavailable"})
                errors.append(sym)
            # Fall back to yfinance for symbols MT5 couldn't provide
            if errors:
                try:
                    import yfinance as yf
                    yf_tickers = [s + ".s" if not any(c in s for c in ".^") else s for s in errors]
                    tickers = yf.Tickers(" ".join(yf_tickers) if yf_tickers else "")
                    for i, sym in enumerate(errors):
                        if i < len(yf_tickers):
                            t = tickers.tickers.get(yf_tickers[i])
                            if t and t.info:
                                info = t.info
                                results.append({
                                    "symbol": sym, "name": info.get("shortName", sym),
                                    "price": info.get("currentPrice", info.get("regularMarketPrice", 0)),
                                    "change": info.get("regularMarketChange", 0),
                                    "change_percent": info.get("regularMarketChangePercent", 0),
                                    "source": "yfinance",
                                })
                except Exception:
                    pass
            self.json({"success": True, "data": results, "errors": errors})
            return

        if path in ("/market/ohlc", "/mt5/market/ohlc"):
            symbol = qs.get("symbol", ["EURUSD.s"])[0]
            tf = qs.get("timeframe", ["1h"])[0]
            count = int(qs.get("count", [100])[0])
            # Try MT5 worker first
            try:
                mt5_data = worker_cmd({"action": "candles", "symbol": symbol, "timeframe": tf, "count": count})
                if "error" not in mt5_data and mt5_data.get("candles"):
                    self.json({"success": True, "data": mt5_data["candles"], "source": "mt5_live"})
                    return
            except Exception:
                pass
            # Fall back to yfinance
            self.json(self.yfinance_ohlc(symbol, tf, count))
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
            self.json(worker_cmd({
                "action": "connect",
                "login": body.get("login"),
                "password": body.get("password"),
                "server": body.get("server"),
            }))
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
            period_map = {"1m": "1d", "5m": "5d", "15m": "10d", "30m": "1mo",
                          "1h": "1mo", "4h": "2mo", "1d": "6mo", "1w": "2y"}
            period = period_map.get(timeframe, "1mo")
            t = yf.Ticker(symbol)
            df = t.history(period=period, interval=timeframe)
            if df.empty:
                return {"success": False, "error": "no data"}
            candles = []
            for idx, row in df.iterrows():
                candles.append({
                    "time": int(idx.timestamp()),
                    "open": float(row["Open"]), "high": float(row["High"]),
                    "low": float(row["Low"]), "close": float(row["Close"]),
                    "volume": int(row["Volume"]),
                })
            return {"success": True, "data": candles[-count:]}
        except Exception as e:
            return {"success": False, "error": str(e)}

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
