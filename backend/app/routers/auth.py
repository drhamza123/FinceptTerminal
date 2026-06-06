from fastapi import APIRouter, Depends, Header, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.database import get_db
from app.schemas.auth import (
    LoginRequest, RegisterRequest, OtpVerifyRequest,
    ForgotPasswordRequest, ResetPasswordRequest,
    LoginResponseData, ProfileResponseData,
)
from app.services.auth import (
    AuthError, register_user, login_user, get_user_by_session,
    verify_otp, user_to_profile,
)

router = APIRouter(tags=["auth"])


def ok(data=None, message="OK"):
    return {"success": True, "message": message, "data": data}


async def resolve_user(
    x_api_key: str = Header(...),
    x_session_token: str = Header(default=None),
    db: AsyncSession = Depends(get_db),
):
    user = await get_user_by_session(db, x_api_key, x_session_token or "")
    if not user:
        raise HTTPException(status_code=401, detail={"success": False, "message": "Invalid or expired session."})
    return user


@router.post("/user/register")
async def register(body: RegisterRequest, db: AsyncSession = Depends(get_db)):
    try:
        await register_user(db, body.username, body.email, body.password,
                             body.phone, body.country_code, body.country)
        return ok(message="Registration successful. Please verify your email.")
    except AuthError as e:
        raise HTTPException(status_code=e.status_code, detail={"success": False, "message": e.message})


@router.post("/user/login")
async def login(body: LoginRequest, db: AsyncSession = Depends(get_db)):
    try:
        # Check if user already has a session BEFORE login (conflict detection)
        from sqlalchemy import select
        from app.models import User
        stmt = select(User).where(User.email == body.email)
        existing = (await db.execute(stmt)).scalar_one_or_none()
        had_session = bool(existing and existing.session_token) and not body.force_login

        user = await login_user(db, body.email, body.password, force_login=body.force_login or False)
        return ok(data=LoginResponseData(
            api_key=user.api_key,
            session_token=user.session_token or "",
            active_session=had_session,
            mfa_required=False,
            message="Login successful",
        ).model_dump())
    except AuthError as e:
        raise HTTPException(status_code=e.status_code, detail={"success": False, "message": e.message})


@router.post("/user/verify-otp")
async def verify_otp_endpoint(body: OtpVerifyRequest, db: AsyncSession = Depends(get_db)):
    try:
        user = await verify_otp(db, body.email, body.otp)
        return ok(data={"api_key": user.api_key, "session_token": user.session_token})
    except AuthError as e:
        raise HTTPException(status_code=e.status_code, detail={"success": False, "message": e.message})


@router.post("/user/verify-mfa")
async def verify_mfa(body: OtpVerifyRequest, db: AsyncSession = Depends(get_db)):
    try:
        user = await verify_otp(db, body.email, body.otp)
        return ok(data={"api_key": user.api_key, "session_token": user.session_token})
    except AuthError as e:
        raise HTTPException(status_code=e.status_code, detail={"success": False, "message": e.message})


@router.post("/user/forgot-password")
async def forgot_password(body: ForgotPasswordRequest, db: AsyncSession = Depends(get_db)):
    return ok(message="If the email exists, a reset code has been sent.")


@router.post("/user/reset-password")
async def reset_password(body: ResetPasswordRequest, db: AsyncSession = Depends(get_db)):
    return ok(message="Password has been reset successfully.")


@router.post("/user/logout")
async def logout(user=Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    user.session_token = None
    await db.commit()
    return ok(message="Logged out.")


@router.get("/user/session-pulse")
async def session_pulse(user=Depends(resolve_user)):
    return ok(message="Session active")


@router.get("/user/profile")
async def get_profile(user=Depends(resolve_user)):
    return ok(data=user_to_profile(user).model_dump())


@router.put("/user/profile")
async def update_profile(user=Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    return ok(message="Profile updated.")


@router.post("/user/regenerate-api-key")
async def regenerate_api_key(user=Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    from app.utils.security import generate_api_key
    user.api_key = generate_api_key()
    await db.commit()
    return ok(message="API key regenerated.")


@router.get("/user/credits")
async def get_credits(user=Depends(resolve_user)):
    return ok(data={"credits": user.credit_balance})


@router.delete("/user/account")
async def delete_account(user=Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    await db.delete(user)
    await db.commit()
    return ok(message="Account deleted.")


@router.get("/user/usage")
async def get_usage(user=Depends(resolve_user)):
    return ok(data={"days": []})


@router.get("/user/login-history")
async def login_history(user=Depends(resolve_user)):
    return ok(data={"history": []})


@router.post("/user/mfa/enable")
async def enable_mfa(user=Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    user.mfa_enabled = True
    await db.commit()
    return ok(message="MFA enabled.")


@router.post("/user/mfa/disable")
async def disable_mfa(user=Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    user.mfa_enabled = False
    await db.commit()
    return ok(message="MFA disabled.")


@router.get("/user/subscriptions")
async def get_subscriptions(user=Depends(resolve_user)):
    return ok(data={
        "user_id": user.id,
        "account_type": user.account_type,
        "credit_balance": user.credit_balance,
        "credits_expire_at": user.credits_expire_at.isoformat() if user.credits_expire_at else None,
        "support_type": "community",
        "last_credit_purchase_at": None,
        "created_at": user.created_at.isoformat() if user.created_at else None,
    })


@router.get("/user/transactions")
async def get_transactions(user=Depends(resolve_user)):
    return ok(data={"transactions": []})
