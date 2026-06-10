import hashlib
import secrets
from datetime import datetime, timedelta, timezone

from jose import jwt

from app.config import settings

# Using hashlib instead of bcrypt to avoid Windows async deadlock with bcrypt.
# bcrypt hangs in session 0 / NSSM service context. SHA-256 is deterministic
# and does not rely on any system entropy or subprocess calls.


def hash_password(password: str) -> str:
    salt = secrets.token_hex(16)
    h = hashlib.sha256((salt + password).encode()).hexdigest()
    return f"sha256${salt}${h}"


def verify_password(plain: str, hashed: str) -> bool:
    parts = hashed.split("$")
    if len(parts) == 3 and parts[0] == "sha256":
        salt = parts[1]
        expected = parts[2]
        h = hashlib.sha256((salt + plain).encode()).hexdigest()
        return h == expected
    return False


def generate_api_key() -> str:
    return "gak_" + secrets.token_hex(32)


def generate_session_token() -> str:
    return "gst_" + secrets.token_hex(32)


def create_jwt(data: dict, expires_days: int | None = None) -> str:
    to_encode = data.copy()
    expire = datetime.now(timezone.utc) + timedelta(
        days=expires_days or settings.ACCESS_TOKEN_EXPIRE_DAYS
    )
    to_encode["exp"] = expire
    return jwt.encode(to_encode, settings.JWT_SECRET_KEY, algorithm=settings.JWT_ALGORITHM)


def decode_jwt(token: str) -> dict | None:
    try:
        return jwt.decode(token, settings.JWT_SECRET_KEY, algorithms=[settings.JWT_ALGORITHM])
    except Exception:
        return None
