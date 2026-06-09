"""Expose all 81 backend indicators via API for the chart."""
from fastapi import APIRouter, Query
from app.services.indicators import compute_all

router = APIRouter(tags=["indicators"])

@router.get("/indicators/list")
async def list_indicators():
    """List all available indicators."""
    from app.services.indicators import compute_all
    import inspect
    src = inspect.getsource(compute_all)
    names = []
    for line in src.split('\n'):
        line = line.strip()
        if line.startswith('I[') and '=' in line:
            name = line.split('=')[0].split('[')[1].split(']')[0].strip("'\"")
            if name not in names:
                names.append(name)
    return {"success": True, "data": {"indicators": names, "count": len(names)}}


@router.get("/indicators/compute")
async def compute_indicators(symbol: str = Query(...), timeframe: str = Query("1d")):
    """Compute all indicators for a symbol."""
    import yfinance as yf
    ticker = yf.Ticker(symbol)
    df = ticker.history(period="6mo", interval=timeframe)
    if df.empty:
        return {"success": False, "error": "No data"}
    
    closes = df['Close'].tolist()
    highs = df['High'].tolist()
    lows = df['Low'].tolist()
    opens = df['Open'].tolist()
    volumes = df['Volume'].tolist()
    
    result = compute_all(closes, highs, lows, opens, volumes)
    # Return only latest values
    latest = {k: (v[-1] if isinstance(v, list) and v else v) for k, v in result.items()}
    return {"success": True, "data": {"symbol": symbol, "latest": latest, "count": len(latest)}}
