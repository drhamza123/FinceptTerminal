"""Bridges live market data into the C++ Trade Server for execution.
Uses yfinance since MT5 can't stream ticks in session 0."""
import json
import logging
import socket
import time
import threading
import pandas as pd
import yfinance as yf

logger = logging.getLogger("guardian.price_bridge")

TRADE_SERVER_HOST = "127.0.0.1"
TRADE_SERVER_PORT = 5559

# Stocks
STOCK_SYMBOLS = ["SPY", "QQQ", "AAPL", "MSFT", "NVDA", "TSLA", "GOOGL", "AMZN"]
# Forex + Metals (mapped to yfinance tickers)
FOREX_SYMBOLS = {
    "EURUSD": "EURUSD=X", "GBPUSD": "GBPUSD=X", "USDJPY": "USDJPY=X",
    "AUDUSD": "AUDUSD=X", "USDCAD": "USDCAD=X", "USDCHF": "USDCHF=X",
    "NZDUSD": "NZDUSD=X", "XAUUSD": "GC=F", "XAGUSD": "SI=F",
}

class PriceBridge:
    """Fetches prices from yfinance and pushes them to the C++ Trade Server."""

    def __init__(self):
        self._running = False
        self._thread = None

    def _send_command(self, command: str) -> dict:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(3.0)
            s.connect((TRADE_SERVER_HOST, TRADE_SERVER_PORT))
            s.sendall((command + "\n").encode())
            data = s.recv(65536).decode().strip()
            s.close()
            return json.loads(data)
        except Exception as e:
            return {"error": str(e)}

    def _push_price(self, symbol: str, bid: float, ask: float):
        if bid <= 0 or ask <= 0:
            return
        try:
            result = self._send_command(f"PRICE|{symbol}|{bid}|{ask}")
            if "error" not in result:
                logger.info(f"Price: {symbol} bid={bid:.4f} ask={ask:.4f}")
        except Exception:
            pass

    def _fetch_and_push(self):
        """Fetch all prices and push to trade server."""
        # Stocks
        try:
            for sym in STOCK_SYMBOLS:
                ticker = yf.Ticker(sym)
                data = ticker.history(period="1d", interval="1m")
                if not data.empty:
                    last = data.iloc[-1]
                    price = float(last['Close'])
                    bid = price * 0.9999
                    ask = price * 1.0001
                    self._push_price(sym, bid, ask)
        except Exception as e:
            logger.debug(f"Stock fetch error: {e}")

        # Forex + Metals
        try:
            all_forex = list(FOREX_SYMBOLS.values())
            data = yf.download(all_forex, period="1d", interval="1m", progress=False, group_by="ticker")
            if not data.empty:
                for our_sym, yf_sym in FOREX_SYMBOLS.items():
                    try:
                        if isinstance(data.columns, pd.MultiIndex) and yf_sym in data.columns.get_level_values(0):
                            last = data[yf_sym]['Close'].iloc[-1]
                            price = float(last)
                            self._push_price(our_sym, price, price)
                    except Exception:
                        pass
        except Exception as e:
            logger.debug(f"Forex fetch error: {e}")

    def _run(self):
        logger.info(f"Price bridge started: {len(STOCK_SYMBOLS)} stocks + {len(FOREX_SYMBOLS)} forex")
        while self._running:
            try:
                self._fetch_and_push()
            except Exception as e:
                logger.warning(f"Bridge error: {e}")
            time.sleep(10)  # 10-second poll for yfinance (free tier)

    def start(self):
        if self._running:
            return
        self._running = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        logger.info("Price bridge started")

    def stop(self):
        self._running = False
        if self._thread:
            self._thread.join(timeout=3)
        logger.info("Price bridge stopped")


price_bridge = PriceBridge()
