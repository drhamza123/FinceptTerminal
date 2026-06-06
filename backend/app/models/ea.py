import uuid
from datetime import datetime, timezone

from sqlalchemy import String, DateTime, Integer, Float, Text, JSON
from sqlalchemy.orm import Mapped, mapped_column

from app.database import Base


class EATemplate(Base):
    __tablename__ = "ea_templates"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    tid: Mapped[str] = mapped_column(String(36), default=lambda: str(uuid.uuid4()), unique=True)
    name: Mapped[str] = mapped_column(String(255), nullable=False)
    description: Mapped[str] = mapped_column(Text, default="")
    category: Mapped[str] = mapped_column(String(64), default="custom")
    mql5_code: Mapped[str] = mapped_column(Text, default="")
    compiled_ex5_path: Mapped[str | None] = mapped_column(String(512), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    updated_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))


class EAInstance(Base):
    __tablename__ = "ea_instances"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    eid: Mapped[str] = mapped_column(String(36), default=lambda: str(uuid.uuid4()), unique=True)
    template_id: Mapped[str | None] = mapped_column(String(36), nullable=True)
    ea_name: Mapped[str] = mapped_column(String(255), default="GuardianBridge")
    magic_number: Mapped[int] = mapped_column(Integer, default=2024001)
    symbol: Mapped[str] = mapped_column(String(64), default="")
    timeframe: Mapped[str] = mapped_column(String(16), default="H1")
    status: Mapped[str] = mapped_column(String(32), default="disconnected")
    params_json: Mapped[dict] = mapped_column(JSON, default=dict)
    balance: Mapped[float] = mapped_column(Float, default=0.0)
    equity: Mapped[float] = mapped_column(Float, default=0.0)
    pnl: Mapped[float] = mapped_column(Float, default=0.0)
    connected_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    last_heartbeat_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    updated_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))


class EADeployment(Base):
    __tablename__ = "ea_deployments"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    ea_instance_id: Mapped[str] = mapped_column(String(36), nullable=False)
    chart_id: Mapped[int] = mapped_column(Integer, default=0)
    mt5_account: Mapped[str] = mapped_column(String(128), default="")
    deployed_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    removed_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)


def instance_to_dict(inst: EAInstance) -> dict:
    return {
        "eid": inst.eid,
        "ea_name": inst.ea_name,
        "magic_number": inst.magic_number,
        "symbol": inst.symbol,
        "timeframe": inst.timeframe,
        "status": inst.status,
        "params": inst.params_json,
        "balance": inst.balance,
        "equity": inst.equity,
        "pnl": inst.pnl,
        "connected_at": inst.connected_at.isoformat() if inst.connected_at else None,
        "last_heartbeat_at": inst.last_heartbeat_at.isoformat() if inst.last_heartbeat_at else None,
        "created_at": inst.created_at.isoformat() if inst.created_at else None,
    }
