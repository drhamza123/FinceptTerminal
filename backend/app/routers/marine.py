from fastapi import APIRouter

router = APIRouter(tags=["marine"])

EMPTY_VESSEL_RESPONSE = {"success": True, "data": {"vessels": [], "vessel_count": 0, "credits_used": 0, "remaining_credits": 999}}


@router.post("/marine/vessel/area-search")
async def vessel_area_search(body: dict):
    return EMPTY_VESSEL_RESPONSE


@router.post("/marine/vessel/position")
async def vessel_position(body: dict):
    return {"success": True, "data": {"vessel": None}}


@router.post("/marine/vessel/multi")
async def vessel_multi(body: dict):
    return {"success": True, "data": {"vessels": [], "found_count": 0, "not_found": body.get("imos", [])}}


@router.post("/marine/vessel/history")
async def vessel_history(body: dict):
    return {"success": True, "data": {"history": [], "imo": body.get("imo", ""), "total_records": 0, "credits_used": 0, "remaining_credits": 999}}


@router.get("/marine/health")
async def marine_health():
    return {"success": True, "message": "Marine service running (no data feed configured)", "data": {"status": "degraded"}}
