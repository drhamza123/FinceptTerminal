from datetime import datetime
from pydantic import BaseModel, EmailStr


class LoginRequest(BaseModel):
    email: str
    password: str
    force_login: bool | None = None


class RegisterRequest(BaseModel):
    username: str
    email: str
    password: str
    phone: str
    country_code: str
    country: str | None = None


class OtpVerifyRequest(BaseModel):
    email: str
    otp: str


class ForgotPasswordRequest(BaseModel):
    email: str


class ResetPasswordRequest(BaseModel):
    email: str
    otp: str
    new_password: str


class LoginResponseData(BaseModel):
    api_key: str
    session_token: str
    active_session: bool = True
    mfa_required: bool = False
    message: str = "Login successful"


class ProfileResponseData(BaseModel):
    id: int
    username: str
    email: str
    account_type: str = "free"
    credit_balance: int = 0
    credits_expire_at: str | None = None
    is_verified: bool = False
    is_admin: bool = False
    mfa_enabled: bool = False
    phone: str | None = None
    country: str | None = None
    country_code: str | None = None
    created_at: str | None = None
    last_login_at: str | None = None
    rate_limit: dict | None = None
