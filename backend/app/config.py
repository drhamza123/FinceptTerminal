from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    APP_NAME: str = "AI Stock Guardian API"
    VERSION: str = "1.0.0"

    DATABASE_URL: str = "sqlite+aiosqlite:///./data/guardian.db"

    JWT_SECRET_KEY: str = "change-me-in-production-guardian-v1"
    JWT_ALGORITHM: str = "HS256"
    ACCESS_TOKEN_EXPIRE_DAYS: int = 365

    LLM_PROVIDER_API_KEY: str = ""
    LLM_PROVIDER_BASE_URL: str = "https://api.openai.com/v1"
    LLM_DEFAULT_MODEL: str = "gpt-4o-mini"

    FRED_API_KEY: str = ""

    CORS_ORIGINS: list[str] = ["*"]

    class Config:
        env_file = ".env"
        extra = "allow"


settings = Settings()
