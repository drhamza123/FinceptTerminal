import json
import logging

from fastapi import APIRouter, Header

from app.config import settings

logger = logging.getLogger("guardian.news")
router = APIRouter(tags=["news"])


@router.post("/news/analyze")
async def news_analyze(body: dict):
    url = body.get("url", "")
    return {
        "success": True,
        "message": "OK",
        "data": {
            "analysis": {
                "sentiment": {"score": 0.0, "intensity": "neutral", "confidence": 0.5},
                "market_impact": {"urgency": "low", "prediction": "neutral"},
                "summary": f"Analysis of {url}",
                "keywords": [],
                "topics": [],
                "key_points": [],
                "risk_signals": {
                    "regulatory": {"level": "low", "details": ""},
                    "geopolitical": {"level": "low", "details": ""},
                    "operational": {"level": "low", "details": ""},
                    "market": {"level": "low", "details": ""},
                },
                "entities": {
                    "organizations": [],
                    "people": [],
                    "locations": [],
                },
            },
            "content": {"headline": "", "word_count": 0, "fetch_note": "AI Stock Guardian analysis"},
            "credits_used": 1,
            "credits_remaining": 999,
        },
    }


@router.post("/news/summarize")
async def news_summarize(body: dict):
    headlines = body.get("headlines", [])
    return {"success": True, "message": "OK", "data": {"summary": " ".join(headlines[:3]) + "..."}}
