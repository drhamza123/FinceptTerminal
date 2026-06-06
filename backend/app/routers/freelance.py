# backend/app/routers/freelance.py — Freelance Marketplace for MQL5/EA Development
import logging
from datetime import datetime, timezone
from typing import Optional

from fastapi import APIRouter, Depends, Query
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.database import get_db
from app.models.user import User
from app.routers.auth import resolve_user

logger = logging.getLogger("guardian.freelance")
router = APIRouter(tags=["freelance"])

FREELANCE_PROJECTS = [
    {
        "id": 1, "title": "MT5 Grid Trading EA",
        "description": "Need a grid trading EA with martingale money management for XAUUSD",
        "budget": "$500-$1000", "category": "Expert Advisor",
        "skills": ["MQL5", "Grid Trading", "Risk Management"],
        "posted_by": "ICMarkets_Pro", "posted_at": "2026-06-01",
        "status": "open", "bids": 5
    },
    {
        "id": 2, "title": "Custom Indicator — Order Block Detector",
        "description": "Indicator that detects smart money concepts order blocks on multiple timeframes",
        "budget": "$300-$700", "category": "Indicator",
        "skills": ["MQL5", "SMC", "ICT Concepts"],
        "posted_by": "ForexTrader42", "posted_at": "2026-06-02",
        "status": "open", "bids": 3
    },
    {
        "id": 3, "title": "MT5 to Telegram Signal Forwarder",
        "description": "Forward trade signals from MT5 to Telegram channel with screenshots",
        "budget": "$200-$400", "category": "Utility",
        "skills": ["MQL5", "Telegram API", "WebRequest"],
        "posted_by": "SignalProvider", "posted_at": "2026-06-03",
        "status": "open", "bids": 7
    },
    {
        "id": 4, "title": "High-Frequency Scalping Bot for EURUSD",
        "description": "Low-latency scalping EA using order flow and DOM imbalances",
        "budget": "$1500-$3000", "category": "Expert Advisor",
        "skills": ["MQL5", "HFT", "DOM", "Latency Optimization"],
        "posted_by": "ScalperPrime", "posted_at": "2026-06-01",
        "status": "open", "bids": 2
    },
    {
        "id": 5, "title": "Multi-Timeframe Trend Analyzer Dashboard",
        "description": "Dashboard showing trend direction across 9 timeframes with confluence scoring",
        "budget": "$400-$800", "category": "Indicator",
        "skills": ["MQL5", "Dashboard", "Multi-Timeframe"],
        "posted_by": "TrendFollower", "posted_at": "2026-06-04",
        "status": "open", "bids": 4
    },
]

FREELANCE_DEVELOPERS = [
    {
        "id": 1, "name": "MQL5_Wizard",
        "rating": 4.9, "completed_jobs": 147,
        "skills": ["MQL5", "MQL4", "Python", "C#"],
        "hourly_rate": "$45", "availability": "Full-time",
        "verified": True
    },
    {
        "id": 2, "name": "AlgoTrader_Dev",
        "rating": 4.8, "completed_jobs": 89,
        "skills": ["MQL5", "Pine Script", "C++", "Rust"],
        "hourly_rate": "$60", "availability": "Part-time",
        "verified": True
    },
    {
        "id": 3, "name": "IndicatorMaster",
        "rating": 4.7, "completed_jobs": 203,
        "skills": ["MQL5", "MQL4", "Python", "JavaScript"],
        "hourly_rate": "$35", "availability": "Full-time",
        "verified": True
    },
    {
        "id": 4, "name": "QuantDeveloper",
        "rating": 5.0, "completed_jobs": 56,
        "skills": ["MQL5", "Python", "R", "Machine Learning"],
        "hourly_rate": "$75", "availability": "Weekends",
        "verified": False
    },
]


@router.get("/freelance/projects")
async def list_projects(
    category: Optional[str] = Query(None),
    skill: Optional[str] = Query(None),
    search: Optional[str] = Query(None),
    user: User = Depends(resolve_user),
):
    """List freelance projects with optional filters."""
    projects = FREELANCE_PROJECTS
    if category:
        projects = [p for p in projects if p["category"].lower() == category.lower()]
    if skill:
        projects = [p for p in projects if any(s.lower() == skill.lower() for s in p["skills"])]
    if search:
        search_lower = search.lower()
        projects = [p for p in projects if search_lower in p["title"].lower() or search_lower in p["description"].lower()]
    return {"success": True, "data": projects}


@router.get("/freelance/projects/{project_id}")
async def get_project(project_id: int, user: User = Depends(resolve_user)):
    """Get a specific freelance project."""
    for p in FREELANCE_PROJECTS:
        if p["id"] == project_id:
            return {"success": True, "data": p}
    return {"success": False, "error": "Project not found"}


@router.post("/freelance/projects")
async def create_project(body: dict, user: User = Depends(resolve_user)):
    """Create a new freelance project."""
    project = {
        "id": len(FREELANCE_PROJECTS) + 1,
        "title": body.get("title", ""),
        "description": body.get("description", ""),
        "budget": body.get("budget", "Negotiable"),
        "category": body.get("category", "Other"),
        "skills": body.get("skills", []),
        "posted_by": user.email,
        "posted_at": datetime.now(timezone.utc).strftime("%Y-%m-%d"),
        "status": "open",
        "bids": 0,
    }
    FREELANCE_PROJECTS.append(project)
    return {"success": True, "data": project}


@router.get("/freelance/developers")
async def list_developers(
    skill: Optional[str] = Query(None),
    min_rating: Optional[float] = Query(None),
    user: User = Depends(resolve_user),
):
    """Browse freelance developers."""
    devs = FREELANCE_DEVELOPERS
    if skill:
        devs = [d for d in devs if any(s.lower() == skill.lower() for s in d["skills"])]
    if min_rating:
        devs = [d for d in devs if d["rating"] >= min_rating]
    devs.sort(key=lambda x: x["rating"], reverse=True)
    return {"success": True, "data": devs}


@router.get("/freelance/categories")
async def list_categories(user: User = Depends(resolve_user)):
    """List freelance categories."""
    categories = list(set(p["category"] for p in FREELANCE_PROJECTS))
    return {"success": True, "data": sorted(categories)}
