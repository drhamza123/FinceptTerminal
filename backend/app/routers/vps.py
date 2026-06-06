# backend/app/routers/vps.py — Virtual Hosting (VPS) Management for MT5
import logging
import random
from datetime import datetime, timezone
from typing import Optional

from fastapi import APIRouter, Depends, Query
from sqlalchemy.ext.asyncio import AsyncSession

from app.database import get_db
from app.models.user import User
from app.routers.auth import resolve_user

logger = logging.getLogger("guardian.vps")
router = APIRouter(tags=["vps"])

VPS_PLANS = [
    {
        "id": 1, "name": "Starter",
        "cpu": 1, "ram_gb": 2, "storage_gb": 40,
        "bandwidth_tb": 1, "price_monthly": 15,
        "price_yearly": 150, "recommended": False
    },
    {
        "id": 2, "name": "Trader",
        "cpu": 2, "ram_gb": 4, "storage_gb": 80,
        "bandwidth_tb": 2, "price_monthly": 30,
        "price_yearly": 300, "recommended": True
    },
    {
        "id": 3, "name": "Pro Trader",
        "cpu": 4, "ram_gb": 8, "storage_gb": 160,
        "bandwidth_tb": 4, "price_monthly": 60,
        "price_yearly": 600, "recommended": False
    },
    {
        "id": 4, "name": "Institutional",
        "cpu": 8, "ram_gb": 16, "storage_gb": 320,
        "bandwidth_tb": 8, "price_monthly": 120,
        "price_yearly": 1200, "recommended": False
    },
]

VIRTUAL_INSTANCES = [
    {
        "id": 1, "user_email": "demo@guardian.ai",
        "plan": "Trader", "status": "running",
        "ip": "198.51.100.42", "region": "New York (US-EAST)",
        "uptime_days": 47,
        "mt5_installed": True, "mt5_version": "5.00 Build 4880",
        "cpu_usage": 23, "ram_usage": 1.8, "storage_usage": 34,
        "created_at": "2026-04-20T10:30:00Z",
        "expires_at": "2026-07-20T10:30:00Z",
    }
]


@router.get("/vps/plans")
async def list_plans(user: User = Depends(resolve_user)):
    """List available VPS plans."""
    return {"success": True, "data": VPS_PLANS}


@router.get("/vps/status")
async def get_vps_status(user: User = Depends(resolve_user)):
    """Get current VPS instance status."""
    instances = [v for v in VIRTUAL_INSTANCES if v["user_email"] == user.email]
    if not instances:
        return {"success": True, "data": {"has_vps": False, "message": "No VPS deployed"}}
    return {"success": True, "data": {"has_vps": True, "instance": instances[0]}}


@router.post("/vps/deploy")
async def deploy_vps(body: dict, user: User = Depends(resolve_user)):
    """Deploy a new VPS instance."""
    plan_id = body.get("plan_id", 2)
    plan = next((p for p in VPS_PLANS if p["id"] == plan_id), None)
    if not plan:
        return {"success": False, "error": "Invalid plan"}

    instance = {
        "id": len(VIRTUAL_INSTANCES) + 1,
        "user_email": user.email,
        "plan": plan["name"],
        "status": "provisioning",
        "ip": f"198.51.100.{random.randint(50, 200)}",
        "region": body.get("region", "New York (US-EAST)"),
        "uptime_days": 0,
        "mt5_installed": True,
        "mt5_version": "5.00 Build 4880",
        "cpu_usage": 0,
        "ram_usage": 0,
        "storage_usage": 0,
        "created_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "expires_at": datetime.now(timezone.utc).replace(
            month=datetime.now(timezone.utc).month + 1
        ).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }
    VIRTUAL_INSTANCES.append(instance)
    return {"success": True, "data": instance}


@router.post("/vps/action")
async def vps_action(body: dict, user: User = Depends(resolve_user)):
    """Perform action on VPS: restart, stop, start, reboot."""
    action = body.get("action", "restart")
    valid_actions = {"restart", "stop", "start", "reboot"}
    if action not in valid_actions:
        return {"success": False, "error": f"Invalid action. Must be one of: {', '.join(valid_actions)}"}

    instances = [v for v in VIRTUAL_INSTANCES if v["user_email"] == user.email]
    if not instances:
        return {"success": False, "error": "No VPS deployed"}

    instance = instances[0]
    if action == "stop":
        instance["status"] = "stopped"
    elif action in ("start", "reboot"):
        instance["status"] = "running"

    return {"success": True, "data": {
        "message": f"VPS {action} successful",
        "instance": instance
    }}
