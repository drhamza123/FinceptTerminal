import uuid
from datetime import datetime, timezone

from sqlalchemy import String, DateTime, Integer, Text, JSON
from sqlalchemy.orm import Mapped, mapped_column

from app.database import Base


class AgentMemory(Base):
    __tablename__ = "agent_memories"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    key: Mapped[str] = mapped_column(String(255), nullable=False, unique=True)
    value: Mapped[str] = mapped_column(Text, default="")
    memory_type: Mapped[str] = mapped_column(String(64), default="general")
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    updated_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))


class AgentSchedule(Base):
    __tablename__ = "agent_schedules"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    sid: Mapped[str] = mapped_column(String(36), default=lambda: str(uuid.uuid4()), unique=True)
    query: Mapped[str] = mapped_column(Text, default="")
    cron_expression: Mapped[str] = mapped_column(String(64), default="0 0 * * *")
    session_id: Mapped[str] = mapped_column(String(128), default="")
    status: Mapped[str] = mapped_column(String(32), default="active")
    last_run: Mapped[str | None] = mapped_column(String(32), nullable=True)
    next_run: Mapped[str | None] = mapped_column(String(32), nullable=True)
    run_count: Mapped[int] = mapped_column(Integer, default=0)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    updated_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))


class AgentTask(Base):
    __tablename__ = "agent_tasks"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    tid: Mapped[str] = mapped_column(String(36), default=lambda: str(uuid.uuid4()), unique=True)
    query: Mapped[str] = mapped_column(Text, default="")
    session_id: Mapped[str] = mapped_column(String(128), default="")
    status: Mapped[str] = mapped_column(String(32), default="pending")
    result: Mapped[str | None] = mapped_column(Text, nullable=True)
    activity: Mapped[list | None] = mapped_column(JSON, default=list)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    updated_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    completed_at: Mapped[str | None] = mapped_column(String(32), nullable=True)


class AgentMonitor(Base):
    __tablename__ = "agent_monitors"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    mid: Mapped[str] = mapped_column(String(36), default=lambda: str(uuid.uuid4()), unique=True)
    name: Mapped[str] = mapped_column(String(255), default="Unnamed Monitor")
    source_type: Mapped[str] = mapped_column(String(64), default="api")
    source_config: Mapped[dict | None] = mapped_column(JSON, default=dict)
    trigger_config: Mapped[dict | None] = mapped_column(JSON, default=dict)
    analysis_query: Mapped[str] = mapped_column(Text, default="")
    check_interval_seconds: Mapped[int] = mapped_column(Integer, default=3600)
    status: Mapped[str] = mapped_column(String(32), default="active")
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    updated_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    last_check: Mapped[str | None] = mapped_column(String(32), nullable=True)
    last_triggered: Mapped[str | None] = mapped_column(String(32), nullable=True)
    check_count: Mapped[int] = mapped_column(Integer, default=0)
