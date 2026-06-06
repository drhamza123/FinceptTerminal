import logging
from datetime import datetime, timedelta, timezone

import httpx
from fastapi import APIRouter

from app.config import settings

logger = logging.getLogger("guardian.macro")
router = APIRouter(tags=["macro"])

FRED_SERIES_MAP = {
    "PAYEMS": {"title": "Non-Farm Payrolls", "category": "Employment", "country": "US", "importance": "high"},
    "CPIAUCSL": {"title": "CPI YoY", "category": "Inflation", "country": "US", "importance": "high"},
    "FEDFUNDS": {"title": "Fed Funds Rate", "category": "Central Bank", "country": "US", "importance": "high"},
    "GDP": {"title": "GDP QoQ", "category": "GDP", "country": "US", "importance": "high"},
    "UNRATE": {"title": "Unemployment Rate", "category": "Employment", "country": "US", "importance": "high"},
    "UMCSENT": {"title": "Consumer Sentiment", "category": "Consumer Confidence", "country": "US", "importance": "medium"},
    "HOUST": {"title": "Housing Starts", "category": "Housing", "country": "US", "importance": "medium"},
    "INDPRO": {"title": "Industrial Production", "category": "Manufacturing", "country": "US", "importance": "medium"},
    "RSXFS": {"title": "Retail Sales", "category": "Consumption", "country": "US", "importance": "medium"},
}

FALLBACK_EVENT_TEMPLATES = [
    {"event_id": "eco-001", "title": "US Non-Farm Payrolls", "category": "Employment", "country": "US", "importance": "high", "previous": "175K", "forecast": "190K", "actual": None},
    {"event_id": "eco-002", "title": "FOMC Interest Rate Decision", "category": "Central Bank", "country": "US", "importance": "high", "previous": "4.75%", "forecast": "4.50%", "actual": None},
    {"event_id": "eco-003", "title": "Eurozone CPI YoY", "category": "Inflation", "country": "EU", "importance": "high", "previous": "2.4%", "forecast": "2.3%", "actual": None},
    {"event_id": "eco-004", "title": "US GDP QoQ", "category": "GDP", "country": "US", "importance": "high", "previous": "2.8%", "forecast": "2.5%", "actual": None},
    {"event_id": "eco-005", "title": "Bank of England Rate Decision", "category": "Central Bank", "country": "UK", "importance": "high", "previous": "4.50%", "forecast": "4.25%", "actual": None},
    {"event_id": "eco-006", "title": "US Initial Jobless Claims", "category": "Employment", "country": "US", "importance": "medium", "previous": "233K", "forecast": "228K", "actual": None},
    {"event_id": "eco-007", "title": "China GDP YoY", "category": "GDP", "country": "CN", "importance": "high", "previous": "5.1%", "forecast": "4.9%", "actual": None},
    {"event_id": "eco-008", "title": "Japan CPI YoY", "category": "Inflation", "country": "JP", "importance": "medium", "previous": "2.8%", "forecast": "2.7%", "actual": None},
    {"event_id": "eco-009", "title": "EU Manufacturing PMI", "category": "Manufacturing", "country": "EU", "importance": "medium", "previous": "48.2", "forecast": "48.5", "actual": None},
    {"event_id": "eco-010", "title": "US ISM Manufacturing PMI", "category": "Manufacturing", "country": "US", "importance": "high", "previous": "49.8", "forecast": "50.1", "actual": None},
    {"event_id": "eco-011", "title": "US Trade Balance", "category": "Trade", "country": "US", "importance": "medium", "previous": "-78.6B", "forecast": "-76.2B", "actual": None},
    {"event_id": "eco-012", "title": "Australia RBA Rate Decision", "category": "Central Bank", "country": "AU", "importance": "high", "previous": "4.10%", "forecast": "4.10%", "actual": None},
    {"event_id": "eco-013", "title": "US ISM Services PMI", "category": "Services", "country": "US", "importance": "medium", "previous": "52.3", "forecast": "52.6", "actual": None},
    {"event_id": "eco-014", "title": "EU Services PMI", "category": "Services", "country": "EU", "importance": "medium", "previous": "51.0", "forecast": "51.2", "actual": None},
    {"event_id": "eco-015", "title": "UK CPI YoY", "category": "Inflation", "country": "UK", "importance": "high", "previous": "3.2%", "forecast": "3.0%", "actual": None},
    {"event_id": "eco-016", "title": "US Consumer Confidence", "category": "Consumer Confidence", "country": "US", "importance": "medium", "previous": "101.2", "forecast": "102.4", "actual": None},
    {"event_id": "eco-017", "title": "EU GDP QoQ", "category": "GDP", "country": "EU", "importance": "high", "previous": "0.3%", "forecast": "0.4%", "actual": None},
    {"event_id": "eco-018", "title": "Japan BoJ Rate Decision", "category": "Central Bank", "country": "JP", "importance": "high", "previous": "0.50%", "forecast": "0.50%", "actual": None},
    {"event_id": "eco-019", "title": "US Housing Starts", "category": "Housing", "country": "US", "importance": "medium", "previous": "1.42M", "forecast": "1.40M", "actual": None},
    {"event_id": "eco-020", "title": "China Caixin Manufacturing PMI", "category": "Manufacturing", "country": "CN", "importance": "medium", "previous": "50.6", "forecast": "50.8", "actual": None},
    {"event_id": "eco-021", "title": "US Retail Sales MoM", "category": "Consumer Confidence", "country": "US", "importance": "high", "previous": "0.4%", "forecast": "0.3%", "actual": None},
    {"event_id": "eco-022", "title": "UK GDP MoM", "category": "GDP", "country": "UK", "importance": "medium", "previous": "0.1%", "forecast": "0.1%", "actual": None},
    {"event_id": "eco-023", "title": "EU Trade Balance", "category": "Trade", "country": "EU", "importance": "medium", "previous": "18.5B", "forecast": "19.2B", "actual": None},
    {"event_id": "eco-024", "title": "US PPI MoM", "category": "Inflation", "country": "US", "importance": "medium", "previous": "0.2%", "forecast": "0.2%", "actual": None},
    {"event_id": "eco-025", "title": "Australia CPI YoY", "category": "Inflation", "country": "AU", "importance": "high", "previous": "3.1%", "forecast": "2.9%", "actual": None},
    {"event_id": "eco-026", "title": "US Existing Home Sales", "category": "Housing", "country": "US", "importance": "medium", "previous": "4.12M", "forecast": "4.18M", "actual": None},
    {"event_id": "eco-027", "title": "China CPI YoY", "category": "Inflation", "country": "CN", "importance": "medium", "previous": "0.8%", "forecast": "0.9%", "actual": None},
    {"event_id": "eco-028", "title": "UK Manufacturing PMI", "category": "Manufacturing", "country": "UK", "importance": "medium", "previous": "49.4", "forecast": "49.6", "actual": None},
    {"event_id": "eco-029", "title": "EU Consumer Confidence", "category": "Consumer Confidence", "country": "EU", "importance": "low", "previous": "-13.2", "forecast": "-12.8", "actual": None},
    {"event_id": "eco-030", "title": "Japan Industrial Production MoM", "category": "Manufacturing", "country": "JP", "importance": "medium", "previous": "0.8%", "forecast": "0.5%", "actual": None},
    {"event_id": "eco-031", "title": "US New Home Sales", "category": "Housing", "country": "US", "importance": "medium", "previous": "698K", "forecast": "705K", "actual": None},
    {"event_id": "eco-032", "title": "Australia Trade Balance", "category": "Trade", "country": "AU", "importance": "low", "previous": "5.8B", "forecast": "5.5B", "actual": None},
    {"event_id": "eco-033", "title": "Japan GDP QoQ", "category": "GDP", "country": "JP", "importance": "medium", "previous": "0.6%", "forecast": "0.5%", "actual": None},
    {"event_id": "eco-034", "title": "China Trade Balance", "category": "Trade", "country": "CN", "importance": "medium", "previous": "78.4B", "forecast": "80.1B", "actual": None},
    {"event_id": "eco-035", "title": "US Michigan Consumer Sentiment", "category": "Consumer Confidence", "country": "US", "importance": "medium", "previous": "72.5", "forecast": "73.1", "actual": None},
    {"event_id": "eco-036", "title": "Bank of Japan Summary of Opinions", "category": "Central Bank", "country": "JP", "importance": "low", "previous": "—", "forecast": "—", "actual": None},
]

FALLBACK_EVENTS = []
_events_initialized = False


def _ensure_fallback_events():
    global _events_initialized, FALLBACK_EVENTS
    if _events_initialized:
        return
    today = datetime.now(timezone.utc).date()
    for i, tmpl in enumerate(FALLBACK_EVENT_TEMPLATES):
        offset_days = (i % 28) + 1
        event_date = today + timedelta(days=offset_days)
        tmpl["date"] = event_date.strftime("%Y-%m-%d")
        tmpl["time"] = "08:30 ET" if i % 2 == 0 else "10:00 ET"
        FALLBACK_EVENTS.append(dict(tmpl))
    _events_initialized = True


async def fetch_fred_series(series_id: str) -> list[dict]:
    api_key = settings.FRED_API_KEY
    if not api_key:
        return []
    url = "https://api.stlouisfed.org/fred/series/observations"
    params = {
        "series_id": series_id,
        "api_key": api_key,
        "file_type": "json",
        "sort_order": "desc",
        "limit": 1,
    }
    try:
        async with httpx.AsyncClient(timeout=10) as client:
            resp = await client.get(url, params=params)
            resp.raise_for_status()
            data = resp.json()
            return data.get("observations", [])
    except Exception as exc:
        logger.warning("FRED fetch failed for %s: %s", series_id, exc)
        return []


def _format_series_value(val: str | None) -> str:
    if val is None or val == ".":
        return "—"
    try:
        return f"{float(val):.2f}"
    except (ValueError, TypeError):
        return val or "—"


@router.get("/macro/upcoming-events")
async def upcoming_events(limit: int = 25):
    api_key = settings.FRED_API_KEY
    events = []

    if api_key:
        today = datetime.now(timezone.utc).date()
        for idx, (series_id, meta) in enumerate(FRED_SERIES_MAP.items()):
            obs = await fetch_fred_series(series_id)
            actual = _format_series_value(obs[0]["value"]) if obs else None
            offset_days = (idx % 28) + 1
            event_date = today + timedelta(days=offset_days)
            events.append({
                "event_id": f"fred-{series_id.lower()}",
                "title": meta["title"],
                "date": event_date.strftime("%Y-%m-%d"),
                "time": "08:30 ET",
                "country": meta["country"],
                "category": meta["category"],
                "importance": meta["importance"],
                "previous": actual or "—",
                "forecast": "—",
                "actual": actual,
            })
    else:
        _ensure_fallback_events()
        events = list(FALLBACK_EVENTS)

    events = events[:limit]
    return {"success": True, "message": "OK", "data": {"events": events}}
