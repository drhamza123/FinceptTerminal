import logging

logger = logging.getLogger("guardian.email")

DEV_OTP = "000000"


def send_otp(email: str, otp: str) -> None:
    print(f"[EMAIL] To: {email} | Subject: Your OTP Code | Body: Your verification OTP is {otp}")
    logger.info("OTP sent to %s: %s", email, otp)


def send_password_reset(email: str, otp: str) -> None:
    print(f"[EMAIL] To: {email} | Subject: Password Reset Code | Body: Your password reset OTP is {otp}")
    logger.info("Password reset OTP sent to %s: %s", email, otp)
