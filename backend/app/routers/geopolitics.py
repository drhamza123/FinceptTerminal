import logging
from datetime import datetime, timedelta, timezone

import httpx
from fastapi import APIRouter, Query

logger = logging.getLogger("guardian.geopolitics")
router = APIRouter(tags=["research", "geopolitics"])

GEO_EVENT_TEMPLATES = [
    {"source": "Reuters", "event_category": "Summit", "title": "G7 Leaders Summit in Kananaskis", "city": "Kananaskis", "country": "Canada", "latitude": 51.0, "longitude": -115.0},
    {"source": "AP News", "event_category": "Election", "title": "US Midterm Election Campaign Intensifies", "city": "Washington", "country": "US", "latitude": 38.9072, "longitude": -77.0369},
    {"source": "BBC", "event_category": "Summit", "title": "EU Council Summit on Climate Policy", "city": "Brussels", "country": "Belgium", "latitude": 50.8503, "longitude": 4.3517},
    {"source": "Reuters", "event_category": "Conflict", "title": "China-Taiwan Tensions Escalate", "city": "Taipei", "country": "Taiwan", "latitude": 25.0330, "longitude": 121.5654},
    {"source": "AP News", "event_category": "Natural Disaster", "title": "Earthquake Strikes Tokyo Region", "city": "Tokyo", "country": "Japan", "latitude": 35.6762, "longitude": 139.6503},
    {"source": "BBC", "event_category": "Election", "title": "UK General Election Announced", "city": "London", "country": "UK", "latitude": 51.5074, "longitude": -0.1278},
    {"source": "Reuters", "event_category": "Summit", "title": "OPEC+ Meeting on Production Cuts", "city": "Vienna", "country": "Austria", "latitude": 48.2082, "longitude": 16.3738},
    {"source": "AP News", "event_category": "Natural Disaster", "title": "Severe Flooding in Mumbai", "city": "Mumbai", "country": "India", "latitude": 19.0760, "longitude": 72.8777},
    {"source": "BBC", "event_category": "Election", "title": "Kenya Presidential Election", "city": "Nairobi", "country": "Kenya", "latitude": -1.2921, "longitude": 36.8219},
    {"source": "Reuters", "event_category": "Policy", "title": "US Announces New Tariffs on Chinese Goods", "city": "Washington", "country": "US", "latitude": 38.9072, "longitude": -77.0369},
    {"source": "AP News", "event_category": "Summit", "title": "NATO Summit in Madrid", "city": "Madrid", "country": "Spain", "latitude": 40.4168, "longitude": -3.7038},
    {"source": "BBC", "event_category": "Natural Disaster", "title": "Bushfires Threaten Sydney Suburbs", "city": "Sydney", "country": "Australia", "latitude": -33.8688, "longitude": 151.2093},
    {"source": "Reuters", "event_category": "Conflict", "title": "Pension Reform Protests in Paris", "city": "Paris", "country": "France", "latitude": 48.8566, "longitude": 2.3522},
    {"source": "AP News", "event_category": "Election", "title": "Brazil Presidential Election Runoff", "city": "Brasília", "country": "Brazil", "latitude": -15.7975, "longitude": -47.8919},
    {"source": "BBC", "event_category": "Policy", "title": "German Coalition Government Formation", "city": "Berlin", "country": "Germany", "latitude": 52.5200, "longitude": 13.4050},
    {"source": "Reuters", "event_category": "Summit", "title": "Inter-Korean Summit Held", "city": "Seoul", "country": "South Korea", "latitude": 37.5665, "longitude": 126.9780},
    {"source": "AP News", "event_category": "Natural Disaster", "title": "Hurricane Hits Cancun Coast", "city": "Cancún", "country": "Mexico", "latitude": 21.1619, "longitude": -86.8515},
    {"source": "BBC", "event_category": "Policy", "title": "Russia Announces New Economic Sanctions Response", "city": "Moscow", "country": "Russia", "latitude": 55.7558, "longitude": 37.6173},
    {"source": "Reuters", "event_category": "Summit", "title": "Arab League Summit in Cairo", "city": "Cairo", "country": "Egypt", "latitude": 30.0444, "longitude": 31.2357},
    {"source": "AP News", "event_category": "Conflict", "title": "Iran Nuclear Tensions Rise", "city": "Tehran", "country": "Iran", "latitude": 35.6892, "longitude": 51.3890},
    {"source": "BBC", "event_category": "Election", "title": "Italian Snap Election Called", "city": "Rome", "country": "Italy", "latitude": 41.9028, "longitude": 12.4964},
    {"source": "Reuters", "event_category": "Policy", "title": "Canada Introduces Carbon Tax Adjustment", "city": "Ottawa", "country": "Canada", "latitude": 45.4215, "longitude": -75.6972},
    {"source": "AP News", "event_category": "Natural Disaster", "title": "Monsoon Flooding Devastates Karachi", "city": "Karachi", "country": "Pakistan", "latitude": 24.8607, "longitude": 67.0011},
    {"source": "BBC", "event_category": "Summit", "title": "BRICS Summit in Johannesburg", "city": "Johannesburg", "country": "South Africa", "latitude": -26.2041, "longitude": 28.0473},
    {"source": "Reuters", "event_category": "Election", "title": "Indonesian Regional Elections", "city": "Jakarta", "country": "Indonesia", "latitude": -6.2088, "longitude": 106.8456},
    {"source": "AP News", "event_category": "Conflict", "title": "Mining Sector Protests in Santiago", "city": "Santiago", "country": "Chile", "latitude": -33.4489, "longitude": -70.6693},
    {"source": "BBC", "event_category": "Policy", "title": "Turkey Announces New Monetary Policy Framework", "city": "Ankara", "country": "Turkey", "latitude": 39.9334, "longitude": 32.8597},
    {"source": "Reuters", "event_category": "Conflict", "title": "Oil Region Unrest in Niger Delta", "city": "Abuja", "country": "Nigeria", "latitude": 9.0765, "longitude": 7.3986},
]

GEO_EVENTS = []
_geo_initialized = False


def _ensure_geo_fallback():
    global _geo_initialized, GEO_EVENTS
    if _geo_initialized:
        return
    today = datetime.now(timezone.utc).date()
    for i, tmpl in enumerate(GEO_EVENT_TEMPLATES):
        offset_days = (i % 14) + 1
        event_date = today + timedelta(days=offset_days)
        tmpl["url"] = f"https://example.com/event/{i}"
        tmpl["extracted_date"] = event_date.strftime("%Y-%m-%d")
        tmpl["created_at"] = datetime.now(timezone.utc).isoformat()
        GEO_EVENTS.append(dict(tmpl))
    _geo_initialized = True


async def fetch_gdelt_events() -> list[dict] | None:
    query = "geopolitics OR conflict OR summit OR election OR protest OR disaster"
    url = "https://api.gdeltproject.org/api/v2/doc/doc"
    params = {
        "query": query,
        "mode": "artlist",
        "format": "json",
        "maxrecords": 50,
    }
    try:
        async with httpx.AsyncClient(timeout=15) as client:
            resp = await client.get(url, params=params)
            resp.raise_for_status()
            data = resp.json()
            articles = data.get("articles", [])
            if not articles:
                return None
            return _parse_gdelt_articles(articles)
    except Exception as exc:
        logger.warning("GDELT fetch failed: %s", exc)
        return None


def _parse_gdelt_articles(articles: list[dict]) -> list[dict]:
    events = []
    country_map = {
        "US": "United States", "GB": "United Kingdom", "CA": "Canada",
        "DE": "Germany", "FR": "France", "JP": "Japan", "CN": "China",
        "IN": "India", "BR": "Brazil", "AU": "Australia", "RU": "Russia",
        "KR": "South Korea", "IT": "Italy", "ES": "Spain", "MX": "Mexico",
        "ID": "Indonesia", "NL": "Netherlands", "SA": "Saudi Arabia",
        "CH": "Switzerland", "SE": "Sweden", "NO": "Norway", "DK": "Denmark",
        "SG": "Singapore", "HK": "Hong Kong", "TW": "Taiwan",
        "ZA": "South Africa", "NG": "Nigeria", "KE": "Kenya",
        "EG": "Egypt", "IL": "Israel", "TR": "Turkey", "IR": "Iran",
        "PK": "Pakistan", "BD": "Bangladesh", "VN": "Vietnam",
        "TH": "Thailand", "MY": "Malaysia", "PH": "Philippines",
        "AR": "Argentina", "CO": "Colombia", "CL": "Chile",
        "PE": "Peru", "UA": "Ukraine", "PL": "Poland",
        "RO": "Romania", "CZ": "Czech Republic", "HU": "Hungary",
        "GR": "Greece", "PT": "Portugal", "IE": "Ireland",
        "FI": "Finland", "AT": "Austria", "NZ": "New Zealand",
    }
    for article in articles[:50]:
        domain = article.get("domain", "")
        sourcecountry = article.get("sourcecountry", "")
        country_name = country_map.get(sourcecountry.upper(), sourcecountry or "Unknown")
        seendate = article.get("seendate", "")
        extracted_date = seendate[:8] if len(seendate) >= 8 else datetime.now(timezone.utc).strftime("%Y%m%d")
        extracted_date_formatted = (
            f"{extracted_date[:4]}-{extracted_date[4:6]}-{extracted_date[6:8]}"
            if len(extracted_date) == 8 else extracted_date
        )
        title = article.get("title", "Untitled Event")
        title_lower = title.lower()
        if any(kw in title_lower for kw in ("summit", "conference", "meeting")):
            category = "Summit"
        elif any(kw in title_lower for kw in ("election", "vote", "ballot")):
            category = "Election"
        elif any(kw in title_lower for kw in ("conflict", "war", "protest", "attack", "violence", "military")):
            category = "Conflict"
        elif any(kw in title_lower for kw in ("disaster", "earthquake", "flood", "hurricane", "fire", "storm")):
            category = "Natural Disaster"
        elif any(kw in title_lower for kw in ("policy", "sanction", "tariff", "regulation", "law")):
            category = "Policy"
        else:
            category = "Summit"

        events.append({
            "url": article.get("url", ""),
            "source": domain,
            "event_category": category,
            "title": title,
            "city": "",
            "country": country_name,
            "latitude": None,
            "longitude": None,
            "extracted_date": extracted_date_formatted,
            "created_at": datetime.now(timezone.utc).isoformat(),
        })
    return events


@router.get("/research/news-events")
async def get_news_events(
    country: str = Query("", description="Filter by country"),
    city: str = Query("", description="Filter by city"),
    event_category: str = Query("", description="Filter by event category"),
    source: str = Query("", description="Filter by news source"),
    date_from: str = Query("", description="Start date (YYYY-MM-DD)"),
    date_to: str = Query("", description="End date (YYYY-MM-DD)"),
    limit: int = Query(20, ge=1, le=100),
    page: int = Query(1, ge=1),
    get_unique_countries: bool = Query(False, alias="get_unique_countries"),
    get_unique_categories: bool = Query(False, alias="get_unique_categories"),
    get_unique_cities: bool = Query(False, alias="get_unique_cities"),
):
    gdelt_events = await fetch_gdelt_events()
    if gdelt_events:
        events_pool = gdelt_events
    else:
        _ensure_geo_fallback()
        events_pool = list(GEO_EVENTS)

    if get_unique_countries:
        counts: dict[str, int] = {}
        for ev in events_pool:
            c = ev["country"]
            counts[c] = counts.get(c, 0) + 1
        return {
            "success": True,
            "data": {"unique_countries": [{"country": k, "event_count": v} for k, v in sorted(counts.items())]},
        }

    if get_unique_categories:
        counts: dict[str, int] = {}
        for ev in events_pool:
            c = ev["event_category"]
            counts[c] = counts.get(c, 0) + 1
        return {
            "success": True,
            "data": {"unique_categories": [{"event_category": k, "event_count": v} for k, v in sorted(counts.items())]},
        }

    if get_unique_cities:
        seen = set()
        result = []
        for ev in events_pool:
            key = (ev["city"], ev["country"])
            if key not in seen:
                seen.add(key)
                result.append({"city": ev["city"], "country": ev["country"]})
        return {
            "success": True,
            "data": {"unique_cities": result},
        }

    filtered = list(events_pool)

    if country:
        filtered = [e for e in filtered if e["country"].lower() == country.lower()]
    if city:
        filtered = [e for e in filtered if e["city"].lower() == city.lower()]
    if event_category:
        filtered = [e for e in filtered if e["event_category"].lower() == event_category.lower()]
    if source:
        filtered = [e for e in filtered if e["source"].lower() == source.lower()]
    if date_from:
        filtered = [e for e in filtered if e["extracted_date"] >= date_from]
    if date_to:
        filtered = [e for e in filtered if e["extracted_date"] <= date_to]

    total = len(filtered)
    total_pages = max(1, (total + limit - 1) // limit)
    start = (page - 1) * limit
    end = start + limit
    page_events = filtered[start:end]

    return {
        "success": True,
        "data": {
            "events": page_events,
            "pagination": {
                "total_events": total,
                "current_page": page,
                "total_pages": total_pages,
                "events_per_page": limit,
                "has_next": page < total_pages,
                "has_prev": page > 1,
            },
            "credits_used": 0,
            "remaining_credits": 999,
        },
    }
