import logging

from fastapi import APIRouter, Query
import yfinance as yf

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
