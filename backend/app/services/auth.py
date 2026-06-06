from datetime import datetime, timezone

from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.user import User
from app.schemas.auth import ProfileResponseData
from app.utils.security import (
    hash_password,
    verify_password,
    generate_api_key,
    generate_session_token,
)


class AuthError(Exception):
    def __init__(self, message: str, status_code: int = 400):
        self.message = message
        self.status_code = status_code


async def register_user(db: AsyncSession, username: str, email: str, password: str,
                         phone: str, country_code: str, country: str | None = None) -> User:
    existing = await db.execute(
        select(User).where((User.email == email) | (User.username == username))
    )
    if existing.scalar_one_or_none():
        raise AuthError("An account with this email or username already exists.", 409)

    user = User(
        username=username,
        email=email,
        password_hash=hash_password(password),
        phone=phone,
        country_code=country_code,
        country=country or "",
        api_key=generate_api_key(),
        # session_token deliberately not set — user must log in to get one
        last_login_at=datetime.now(timezone.utc),
    )
    db.add(user)
    await db.commit()
    await db.refresh(user)
    return user


async def login_user(db: AsyncSession, email: str, password: str, force_login: bool = False) -> User:
    result = await db.execute(select(User).where(User.email == email))
    user = result.scalar_one_or_none()
    if not user or not verify_password(password, user.password_hash):
        raise AuthError("Invalid email or password.", 401)

    # Keep api_key stable so other sessions can coexist.
    # Only the session_token changes — old tokens become invalid.
    if force_login or not user.session_token:
        user.session_token = generate_session_token()
    user.last_login_at = datetime.now(timezone.utc)
    await db.commit()
    await db.refresh(user)
    return user


async def get_user_by_api_key(db: AsyncSession, api_key: str) -> User | None:
    result = await db.execute(select(User).where(User.api_key == api_key))
    return result.scalar_one_or_none()


async def get_user_by_session(db: AsyncSession, api_key: str, session_token: str) -> User | None:
    result = await db.execute(
        select(User).where(User.api_key == api_key, User.session_token == session_token)
    )
    return result.scalar_one_or_none()


async def verify_otp(db: AsyncSession, email: str, otp: str) -> User:
    result = await db.execute(select(User).where(User.email == email))
    user = result.scalar_one_or_none()
    if not user:
        raise AuthError("User not found.", 404)
    if otp != "000000":
        raise AuthError("Invalid OTP.", 401)
    user.is_verified = True
    user.api_key = generate_api_key()
    user.session_token = generate_session_token()
    await db.commit()
    await db.refresh(user)
    return user


def user_to_profile(user: User) -> ProfileResponseData:
    return ProfileResponseData(
        id=user.id,
        username=user.username,
        email=user.email,
        account_type=user.account_type,
        credit_balance=user.credit_balance,
        credits_expire_at=user.credits_expire_at.isoformat() if user.credits_expire_at else None,
        is_verified=user.is_verified,
        is_admin=user.is_admin,
        mfa_enabled=user.mfa_enabled,
        phone=user.phone,
        country=user.country,
        country_code=user.country_code,
        created_at=user.created_at.isoformat() if user.created_at else None,
        last_login_at=user.last_login_at.isoformat() if user.last_login_at else None,
        rate_limit={"limit": 1000, "remaining": 999, "reset_at": None, "window_seconds": 3600, "concurrent_limit": 10},
    )
