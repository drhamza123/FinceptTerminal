"""Alert management API endpoints."""
from fastapi import APIRouter, Body
from app.services.alert_engine import alert_engine

router = APIRouter(tags=["alerts"])


@router.get("/alerts")
async def list_alerts():
    """List all alert rules."""
    return {"success": True, "data": alert_engine.list_rules()}


@router.post("/alerts/create")
async def create_alert(body: dict = Body(...)):
    """Create a new alert rule."""
    rule_id = alert_engine.add_rule(
        symbol=body.get("symbol", "AAPL"),
        condition=body.get("condition", "above"),
        threshold=float(body.get("threshold", 100)),
        webhook_url=body.get("webhook_url", ""),
        user_id=body.get("user_id", ""),
    )
    return {"success": True, "data": {"id": rule_id}}


@router.post("/alerts/delete")
async def delete_alert(body: dict = Body(...)):
    """Delete an alert rule."""
    rule_id = body.get("id", "")
    ok = alert_engine.remove_rule(rule_id)
    return {"success": ok, "message": "Deleted" if ok else "Not found"}
