from fastapi import APIRouter, Header

router = APIRouter(tags=["billing"])

PLANS = [
    {
        "plan_id": "free",
        "name": "Free",
        "description": "Basic access",
        "price_usd": 0.0,
        "currency": "USD",
        "credits": 100,
        "support_type": "community",
        "validity_days": 30,
        "features": ["Basic market data", "AI chat (limited)"],
        "is_free": True,
        "display_order": 1,
    },
    {
        "plan_id": "pro",
        "name": "Pro",
        "description": "Professional access",
        "price_usd": 29.99,
        "currency": "USD",
        "credits": 5000,
        "support_type": "priority",
        "validity_days": 30,
        "features": ["Advanced market data", "AI chat (unlimited)", "QuantLib access", "Portfolio analytics"],
        "is_free": False,
        "display_order": 2,
    },
]


@router.get("/cashfree/plans")
async def get_plans():
    return {"success": True, "message": "OK", "data": PLANS}


@router.post("/user/generate-checkout-token")
async def generate_checkout(body: dict):
    return {"success": True, "message": "Checkout URL generated", "data": {"checkout_url": "https://example.com/checkout"}}
