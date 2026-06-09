import logging
import os
import time

from fastapi import APIRouter, Query
import yfinance as yf
from app.services.massive_data import massive_service

FINNHUB_API_KEY = os.environ.get("FINNHUB_API_KEY", "")

def finnhub_quote(symbol):
    """Fetch quote from Finnhub API (real-time)."""
    if not FINNHUB_API_KEY:
        return None
    try:
        import requests as req
        resp = req.get(
            f"https://finnhub.io/api/v1/quote?symbol={symbol}&token={FINNHUB_API_KEY}",
            timeout=5)
        if resp.status_code == 200:
            d = resp.json()
            if d.get("c", 0) > 0:
                return {
                    "symbol": symbol.upper(),
                    "price": d["c"],
                    "change": round(float(d.get("d", 0)), 2),
                    "change_pct": round(float(d.get("dp", 0)), 2),
                    "high": round(float(d.get("h", 0)), 2),
                    "low": round(float(d.get("l", 0)), 2),
                    "open": round(float(d.get("o", 0)), 2),
                    "prev_close": round(float(d.get("pc", 0)), 2),
                }
    except Exception:
        pass
    return None

router = APIRouter(tags=["market"])
logger = logging.getLogger("guardian.market")

FALLBACK_SYMBOLS = [
    {"symbol": "AAPL", "name": "Apple Inc.", "exchange": "NASDAQ", "type": "stock", "country": "US"},
    {"symbol": "MSFT", "name": "Microsoft Corporation", "exchange": "NASDAQ", "type": "stock", "country": "US"},
    {"symbol": "GOOGL", "name": "Alphabet Inc.", "exchange": "NASDAQ", "type": "stock", "country": "US"},
    {"symbol": "AMZN", "name": "Amazon.com Inc.", "exchange": "NASDAQ", "type": "stock", "country": "US"},
    {"symbol": "TSLA", "name": "Tesla Inc.", "exchange": "NASDAQ", "type": "stock", "country": "US"},
    {"symbol": "META", "name": "Meta Platforms Inc.", "exchange": "NASDAQ", "type": "stock", "country": "US"},
    {"symbol": "NVDA", "name": "NVIDIA Corporation", "exchange": "NASDAQ", "type": "stock", "country": "US"},
    {"symbol": "JPM", "name": "JPMorgan Chase & Co.", "exchange": "NYSE", "type": "stock", "country": "US"},
    {"symbol": "V", "name": "Visa Inc.", "exchange": "NYSE", "type": "stock", "country": "US"},
    {"symbol": "JNJ", "name": "Johnson & Johnson", "exchange": "NYSE", "type": "stock", "country": "US"},
    {"symbol": "WMT", "name": "Walmart Inc.", "exchange": "NYSE", "type": "stock", "country": "US"},
    {"symbol": "MA", "name": "Mastercard Inc.", "exchange": "NYSE", "type": "stock", "country": "US"},
    {"symbol": "PG", "name": "Procter & Gamble Co.", "exchange": "NYSE", "type": "stock", "country": "US"},
    {"symbol": "UNH", "name": "UnitedHealth Group Inc.", "exchange": "NYSE", "type": "stock", "country": "US"},
    {"symbol": "HD", "name": "The Home Depot Inc.", "exchange": "NYSE", "type": "stock", "country": "US"},
    {"symbol": "BAC", "name": "Bank of America Corporation", "exchange": "NYSE", "type": "stock", "country": "US"},
    {"symbol": "DIS", "name": "The Walt Disney Company", "exchange": "NYSE", "type": "stock", "country": "US"},
    {"symbol": "ADBE", "name": "Adobe Inc.", "exchange": "NASDAQ", "type": "stock", "country": "US"},
    {"symbol": "CRM", "name": "Salesforce Inc.", "exchange": "NYSE", "type": "stock", "country": "US"},
    {"symbol": "NFLX", "name": "Netflix Inc.", "exchange": "NASDAQ", "type": "stock", "country": "US"},
    {"symbol": "INTC", "name": "Intel Corporation", "exchange": "NASDAQ", "type": "stock", "country": "US"},
    {"symbol": "AMD", "name": "Advanced Micro Devices Inc.", "exchange": "NASDAQ", "type": "stock", "country": "US"},
    {"symbol": "PYPL", "name": "PayPal Holdings Inc.", "exchange": "NASDAQ", "type": "stock", "country": "US"},
    {"symbol": "KO", "name": "The Coca-Cola Company", "exchange": "NYSE", "type": "stock", "country": "US"},
    {"symbol": "PEP", "name": "PepsiCo Inc.", "exchange": "NASDAQ", "type": "stock", "country": "US"},
    {"symbol": "BTC-USD", "name": "Bitcoin USD", "exchange": "CRYPTO", "type": "crypto", "country": ""},
    {"symbol": "ETH-USD", "name": "Ethereum USD", "exchange": "CRYPTO", "type": "crypto", "country": ""},
    {"symbol": "SOL-USD", "name": "Solana USD", "exchange": "CRYPTO", "type": "crypto", "country": ""},
    {"symbol": "XRP-USD", "name": "XRP USD", "exchange": "CRYPTO", "type": "crypto", "country": ""},
    {"symbol": "DOGE-USD", "name": "Dogecoin USD", "exchange": "CRYPTO", "type": "crypto", "country": ""},
    {"symbol": "ADA-USD", "name": "Cardano USD", "exchange": "CRYPTO", "type": "crypto", "country": ""},
    {"symbol": "AVAX-USD", "name": "Avalanche USD", "exchange": "CRYPTO", "type": "crypto", "country": ""},
    {"symbol": "DOT-USD", "name": "Polkadot USD", "exchange": "CRYPTO", "type": "crypto", "country": ""},
    {"symbol": "MATIC-USD", "name": "Polygon USD", "exchange": "CRYPTO", "type": "crypto", "country": ""},
    {"symbol": "LINK-USD", "name": "Chainlink USD", "exchange": "CRYPTO", "type": "crypto", "country": ""},
    {"symbol": "^GSPC", "name": "S&P 500 Index", "exchange": "INDEX", "type": "index", "country": "US"},
    {"symbol": "^DJI", "name": "Dow Jones Industrial Average", "exchange": "INDEX", "type": "index", "country": "US"},
    {"symbol": "^IXIC", "name": "NASDAQ Composite Index", "exchange": "INDEX", "type": "index", "country": "US"},
    {"symbol": "^FTSE", "name": "FTSE 100 Index", "exchange": "INDEX", "type": "index", "country": "UK"},
    {"symbol": "^N225", "name": "Nikkei 225 Index", "exchange": "INDEX", "type": "index", "country": "JP"},
    {"symbol": "^HSI", "name": "Hang Seng Index", "exchange": "INDEX", "type": "index", "country": "HK"},
    {"symbol": "EURUSD=X", "name": "EUR/USD Currency Pair", "exchange": "FOREX", "type": "forex", "country": ""},
    {"symbol": "GBPUSD=X", "name": "GBP/USD Currency Pair", "exchange": "FOREX", "type": "forex", "country": ""},
    {"symbol": "USDJPY=X", "name": "USD/JPY Currency Pair", "exchange": "FOREX", "type": "forex", "country": ""},
    {"symbol": "USDCAD=X", "name": "USD/CAD Currency Pair", "exchange": "FOREX", "type": "forex", "country": ""},
    {"symbol": "AUDUSD=X", "name": "AUD/USD Currency Pair", "exchange": "FOREX", "type": "forex", "country": ""},
    {"symbol": "USDCHF=X", "name": "USD/CHF Currency Pair", "exchange": "FOREX", "type": "forex", "country": ""},
    {"symbol": "SPY", "name": "SPDR S&P 500 ETF Trust", "exchange": "NYSE", "type": "etf", "country": "US"},
    {"symbol": "QQQ", "name": "Invesco QQQ Trust", "exchange": "NASDAQ", "type": "etf", "country": "US"},
    {"symbol": "DIA", "name": "SPDR Dow Jones Industrial Average ETF Trust", "exchange": "NYSE", "type": "etf", "country": "US"},
    {"symbol": "IWM", "name": "iShares Russell 2000 ETF", "exchange": "NYSE", "type": "etf", "country": "US"},
    {"symbol": "VTI", "name": "Vanguard Total Stock Market ETF", "exchange": "NYSE", "type": "etf", "country": "US"},
    {"symbol": "BND", "name": "Vanguard Total Bond Market ETF", "exchange": "NASDAQ", "type": "etf", "country": "US"},
    {"symbol": "GLD", "name": "SPDR Gold Shares", "exchange": "NYSE", "type": "etf", "country": "US"},
    {"symbol": "SLV", "name": "iShares Silver Trust", "exchange": "NYSE", "type": "etf", "country": "US"},
    {"symbol": "TLT", "name": "iShares 20+ Year Treasury Bond ETF", "exchange": "NASDAQ", "type": "etf", "country": "US"},
    {"symbol": "EEM", "name": "iShares MSCI Emerging Markets ETF", "exchange": "NYSE", "type": "etf", "country": "US"},
]


@router.get("/market/search")
async def market_search(
    q: str = Query("", description="Search query"),
    limit: int = Query(20, description="Max results"),
    type: str = Query("", description="Filter by type (stock, crypto, index, forex, etf)"),
):
    query = q.strip()
    if not query:
        return {"success": True, "message": "OK", "data": []}

    results = []
    tried_yfinance = False
    try:
        ticker = yf.Ticker(query)
        info = ticker.info or {}
        if info and info.get("symbol"):
            tried_yfinance = True
            symbol_str = info.get("symbol", query).replace(" ", "")
            asset_type = "stock"
            exchange = info.get("exchange", "NASDAQ")
            country = info.get("country", "US")
            if any(c in symbol_str for c in ("USD", "EUR", "GBP", "JPY")) and len(symbol_str) > 6:
                asset_type = "crypto" if symbol_str.endswith("-USD") else "forex"
            elif symbol_str.startswith("^"):
                asset_type = "index"
            elif "ETF" in (info.get("longName", "") or "") or "Trust" in (info.get("longName", "") or ""):
                asset_type = "etf"
            results.append({
                "symbol": symbol_str,
                "name": info.get("longName", info.get("shortName", query)),
                "exchange": exchange,
                "type": asset_type,
                "country": country,
            })
    except Exception as e:
        logger.debug(f"yfinance Ticker lookup failed for '{query}': {e}")

    if not tried_yfinance:
        try:
            search_results = yf.Search(query, max_results=min(limit, 50))
            if hasattr(search_results, 'quotes') and search_results.quotes:
                tried_yfinance = True
                for qr in search_results.quotes:
                    sym = qr.get("symbol", "")
                    if not sym:
                        continue
                    exchange = qr.get("exchange", "")
                    quote_type = qr.get("quoteType", "")
                    atype = {
                        "EQUITY": "stock",
                        "ETF": "etf",
                        "INDEX": "index",
                        "CRYPTOCURRENCY": "crypto",
                        "CURRENCY": "forex",
                        "MUTUALFUND": "fund",
                        "FUTURE": "future",
                    }.get(quote_type, "stock")
                    results.append({
                        "symbol": sym,
                        "name": qr.get("longName", qr.get("shortName", qr.get("symbol", ""))),
                        "exchange": exchange,
                        "type": atype,
                        "country": qr.get("country", "") or "",
                    })
        except Exception as e:
            logger.debug(f"yfinance Search failed for '{query}': {e}")

    if not tried_yfinance or len(results) < 2:
        query_upper = query.upper()
        for entry in FALLBACK_SYMBOLS:
            if query_upper in entry["symbol"].upper() or query_upper in entry["name"].upper():
                if entry not in results:
                    results.append(entry)

    if type:
        type_filter = type.lower()
        results = [r for r in results if r["type"] == type_filter]

    return {"success": True, "message": "OK", "data": results[:limit]}


@router.get("/market/quotes")
async def market_quotes(symbols: str = "AAPL,MSFT,SPY"):
    """Get live quotes for any symbols via yfinance on VPS. Uses fast_info for speed, batch download for details."""
    rows, errors = [], []
    tickers_list = [s.strip() for s in symbols.split(",") if s.strip()]
    
    # Phase 1: Finnhub API (real-time data, used first)
    finnhub_data = {}
    if FINNHUB_API_KEY:
        for sym in tickers_list:
            fq = finnhub_quote(sym)
            if fq:
                finnhub_data[sym] = fq

    # Phase 2: Fast path - get current price via fast_info (sub-second per symbol, fallback)
    fast_prices = {}
    for sym in tickers_list:
        if sym in finnhub_data:
            fast_prices[sym] = finnhub_data[sym]["price"]
            continue
        try:
            ticker = yf.Ticker(sym)
            fi = ticker.fast_info
            price = float(getattr(fi, "last_price", 0) or 0)
            if price:
                fast_prices[sym] = price
        except Exception:
            pass
    
    # Phase 2: Batch download history for all symbols at once (change, high, low, volume)
    batch_data = {}
    if tickers_list:
        try:
            batch = yf.download(tickers_list, period="2d", interval="1d", progress=False,
                               auto_adjust=True, group_by="ticker", threads=True)
            if batch is not None and not batch.empty:
                for sym in tickers_list:
                    try:
                        if len(tickers_list) == 1:
                            df = batch
                        else:
                            df = batch.get(sym, None)
                        if df is not None and not df.empty:
                            batch_data[sym] = df
                    except Exception:
                        pass
        except Exception:
            pass
    
    # Phase 3: Finnhub data (preferred) then yfinance fallback
    for sym in tickers_list:
        try:
            # Use Finnhub data if available (real-time)
            if sym in finnhub_data:
                fq = finnhub_data[sym]
                rows.append({
                    "symbol": fq["symbol"], "price": fq["price"],
                    "change": fq["change"], "change_pct": fq["change_pct"],
                    "high": fq["high"], "low": fq["low"],
                    "open": fq["open"], "prev_close": fq["prev_close"],
                    "volume": 0, "timestamp": int(time.time()),
                })
                continue

            price = fast_prices.get(sym, 0)
            df = batch_data.get(sym, None)
            change = 0
            change_pct = 0
            high = 0
            low = 0
            volume = 0
            ts = int(time.time())
            
            if df is not None and not df.empty:
                latest = df.iloc[-1]
                prev = df.iloc[-2] if len(df) > 1 else latest
                if price == 0:
                    price = float(latest.get("Close", latest.get("Adj Close", 0)) or 0)
                prev_close = float(prev.get("Close", prev.get("Adj Close", 0)) or 0)
                if price and prev_close:
                    change = round(price - prev_close, 2)
                    change_pct = round(change / prev_close * 100, 2)
                high = round(float(latest.get("High", 0) or 0), 2)
                low = round(float(latest.get("Low", 0) or 0), 2)
                volume = int(latest.get("Volume", 0) or 0)
                if hasattr(latest.name, "timestamp"):
                    ts = int(latest.name.timestamp())
            
            if price > 0:
                rows.append({
                    "symbol": sym, "name": sym, "price": price,
                    "change": change, "change_percent": change_pct,
                    "high": high, "low": low, "volume": volume,
                    "time": ts, "source": "yfinance_vps",
                })
            else:
                errors.append(sym)
        except Exception:
            errors.append(sym)
    
    return {"success": bool(rows), "data": rows, "errors": errors, "source": "yfinance_vps"}


@router.get("/market/ohlc")
async def market_ohlc(symbol: str = "AAPL", timeframe: str = "1d", count: int = 100):
    """Get OHLC data for any symbol via yfinance on VPS, with indicators."""
    tf_map = {"1m": "1d", "2m": "1d", "3m": "1d", "4m": "1d", "5m": "5d", "6m": "5d",
               "10m": "5d", "15m": "5d", "20m": "5d", "30m": "1mo",
               "1h": "1mo", "2h": "2mo", "3h": "3mo", "4h": "3mo", "6h": "6mo",
               "8h": "6mo", "12h": "1y",
               "1d": "1y", "2d": "2y", "3d": "2y", "5d": "5y",
               "1wk": "2y", "2wk": "5y", "3wk": "5y",
               "1mo": "5y", "2mo": "10y", "3mo": "10y", "4mo": "10y", "6mo": "10y",
               "1y": "20y", "2y": "20y", "5y": "max"}
    interval = {"M1": "1m", "M2": "2m", "M3": "3m", "M4": "4m", "M5": "5m", "M6": "6m",
                 "M10": "10m", "M15": "15m", "M20": "20m", "M30": "30m",
                 "H1": "1h", "H2": "2h", "H3": "3h", "H4": "4h", "H6": "6h",
                 "H8": "8h", "H12": "12h",
                 "D1": "1d", "D2": "2d", "D3": "3d", "D5": "5d",
                 "W1": "1wk", "W2": "2wk", "W3": "3wk",
                 "MN1": "1mo", "MN2": "2mo", "MN3": "3mo", "MN4": "4mo", "MN6": "6mo",
                 "Y1": "1y", "Y2": "2y", "Y5": "5y"}.get(timeframe, timeframe)
    period = tf_map.get(interval, "1mo")
    try:
        ticker = yf.Ticker(symbol)
        df = ticker.history(period=period, interval=interval)
        if df.empty:
            return {"success": False, "error": f"No data for {symbol}", "data": []}
        
        # Calculate indicators using dedicated module
        from app.services.indicators import compute_all
        
        import numpy as np
        closes = df["Close"].values.astype(float)
        highs = df["High"].values.astype(float)
        lows = df["Low"].values.astype(float)
        volumes = df["Volume"].values.astype(float)
        
        ind = compute_all(closes, highs, lows, volumes)
        n = len(closes)
        
        import math
        
        def clean_val(v):
            """Replace NaN/Inf with None for JSON safety."""
            if v is None or (isinstance(v, float) and (math.isnan(v) or math.isinf(v))):
                return None
            return v
        
        candles = []
        for i, (idx, row) in enumerate(df.iterrows()):
            ts = int(idx.timestamp()) if hasattr(idx, "timestamp") else int(idx.tz_localize(None).timestamp())
            candle = {
                "time": ts,
                "open": clean_val(round(float(row["Open"]), 4)),
                "high": clean_val(round(float(row["High"]), 4)),
                "low": clean_val(round(float(row["Low"]), 4)),
                "close": clean_val(round(float(row["Close"]), 4)),
                "volume": int(row["Volume"]),
            }
            # Add all indicators that have valid values
            for ind_name, ind_values in ind.items():
                if i < len(ind_values):
                    val = float(ind_values[i])
                    if not math.isnan(val) and not math.isinf(val) and abs(val) < 1e15:
                        candle[ind_name] = round(val, 4)
            
            candles.append(candle)
        
        return {"success": True, "data": candles[-count:], "source": "yfinance_vps"}
    except Exception as e:
        return {"success": False, "error": str(e), "data": []}


@router.get("/market/info")
async def market_info(symbol: str = "AAPL"):
    """Get company info and fundamentals for a symbol via yfinance on VPS."""
    try:
        ticker = yf.Ticker(symbol)
        info = ticker.info or {}
        return {"success": True, "data": {
            "symbol": info.get("symbol", symbol),
            "name": info.get("longName", info.get("shortName", "")),
            "sector": info.get("sector", ""),
            "industry": info.get("industry", ""),
            "market_cap": info.get("marketCap", 0),
            "pe_ratio": info.get("trailingPE", 0),
            "dividend_yield": info.get("dividendYield", 0),
            "beta": info.get("beta", 0),
            "52w_high": info.get("fiftyTwoWeekHigh", 0),
            "52w_low": info.get("fiftyTwoWeekLow", 0),
            "avg_volume": info.get("averageVolume", 0),
            "source": "yfinance_vps",
        }}
    except Exception as e:
        return {"success": False, "error": str(e), "data": {}}


@router.get("/market/news")
async def market_news(symbol: str = "AAPL", count: int = 10):
    """Get news for a symbol via yfinance on VPS."""
    try:
        ticker = yf.Ticker(symbol)
        news = ticker.news or []
        items = []
        for article in news[:count]:
            items.append({
                "title": article.get("title", ""),
                "published": article.get("providerPublishTime", 0),
                "link": article.get("link", ""),
                "source": article.get("publisher", ""),
                "summary": article.get("summary", ""),
            })
        return {"success": True, "data": items, "source": "yfinance_vps"}
    except Exception as e:
        return {"success": False, "error": str(e), "data": []}


@router.get("/market/financials")
async def market_financials(symbol: str = "AAPL"):
    """Get financial ratios for a symbol via yfinance on VPS."""
    try:
        ticker = yf.Ticker(symbol)
        info = ticker.info or {}
        return {"success": True, "data": {
            "symbol": info.get("symbol", symbol),
            "pe_ratio": info.get("trailingPE", 0),
            "forward_pe": info.get("forwardPE", 0),
            "pb_ratio": info.get("priceToBook", 0),
            "ps_ratio": info.get("priceToSalesTrailing12Months", 0),
            "dividend_yield": info.get("dividendYield", 0),
            "roe": info.get("returnOnEquity", 0),
            "debt_to_equity": info.get("debtToEquity", 0),
            "profit_margins": info.get("profitMargins", 0),
            "source": "yfinance_vps",
        }}
    except Exception as e:
        return {"success": False, "error": str(e), "data": {}}



# ── Polygon.io Real-Time Data ─────────────────────────────────────

from fastapi import WebSocket, WebSocketDisconnect
from app.services.polygon_ws import polygon_streamer
import asyncio

polygon_ui_clients: set = set()

@router.websocket("/ws/polygon")
async def polygon_ws(ws: WebSocket):
    await ws.accept()
    polygon_ui_clients.add(ws)
    polygon_streamer.on_data(lambda msg: asyncio.ensure_future(
        _bcast_polygon(msg)
    ))
    try:
        while True:
            data = await ws.receive_json()
            action = data.get("action", "")
            symbols = data.get("symbols", [])
            channel = data.get("channel", "T")
            if action == "subscribe":
                await polygon_streamer.subscribe(symbols, channel)
                await ws.send_json({"status": "subscribed", "symbols": symbols, "channel": channel})
            elif action == "unsubscribe":
                await polygon_streamer.unsubscribe(symbols, channel)
    except WebSocketDisconnect:
        pass
    finally:
        polygon_ui_clients.discard(ws)

async def _bcast_polygon(msg: dict):
    dead = set()
    for c in polygon_ui_clients:
        try:
            await c.send_json(msg)
        except Exception:
            dead.add(c)
    polygon_ui_clients.difference_update(dead)


@router.post("/polygon/subscribe")
async def polygon_subscribe(body: dict):
    symbols = body.get("symbols", ["AAPL", "MSFT"])
    channel = body.get("channel", "T")
    await polygon_streamer.subscribe(symbols, channel)
    return {"status": "subscribed", "symbols": symbols, "channel": channel}

@router.post("/polygon/unsubscribe")
async def polygon_unsubscribe(body: dict):
    symbols = body.get("symbols", [])
    await polygon_streamer.unsubscribe(symbols)
    return {"status": "unsubscribed"}


# ── Massive.com real-time market data ──────────────────────────

@router.get("/market/massive/snapshot")
async def massive_snapshot(symbol: str = Query(...), asset_class: str = "stocks"):
    """Get real-time market snapshot from Massive.com."""
    if not massive_service.api_key:
        return {"success": False, "error": "MASSIVE_API_KEY not configured"}
    try:
        data = massive_service.get_snapshot(symbol, asset_class)
        if data:
            return {"success": True, "data": data, "source": "massive"}
        return {"success": False, "error": "Symbol not found"}
    except Exception as e:
        return {"success": False, "error": str(e)}


@router.get("/market/massive/quotes")
async def massive_quotes(symbols: str = Query(...), asset_class: str = "stocks"):
    """Get real-time quotes from Massive.com."""
    if not massive_service.api_key:
        return {"success": False, "error": "MASSIVE_API_KEY not configured"}
    try:
        ticker_list = [s.strip() for s in symbols.split(",") if s.strip()]
        data = massive_service.get_quotes(ticker_list, asset_class)
        if data:
            return {"success": True, "data": data, "source": "massive"}
        return {"success": False, "error": "No data returned"}
    except Exception as e:
        return {"success": False, "error": str(e)}
