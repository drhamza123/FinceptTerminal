from fastapi import APIRouter

router = APIRouter(tags=["health"])


@router.get("/health")
async def health():
    return {"success": True, "message": "OK", "data": {"status": "healthy"}}
