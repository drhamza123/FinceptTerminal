import uuid
import logging
from datetime import datetime, timezone
from typing import Optional

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select, desc
from sqlalchemy.ext.asyncio import AsyncSession

from app.database import get_db
from app.models.agent import AgentMemory, AgentSchedule, AgentTask, AgentMonitor

logger = logging.getLogger("guardian.agent")
router = APIRouter(prefix="/chat/agent", tags=["agent"])


def _ts():
    return datetime.now(timezone.utc).isoformat()


def _memory_to_dict(m: AgentMemory) -> dict:
    return {
        "key": m.key,
        "value": m.value,
        "memory_type": m.memory_type,
        "created_at": m.created_at.isoformat() if m.created_at else _ts(),
        "updated_at": m.updated_at.isoformat() if m.updated_at else _ts(),
    }


def _schedule_to_dict(s: AgentSchedule) -> dict:
    return {
        "id": s.sid,
        "query": s.query,
        "cron_expression": s.cron_expression,
        "session_id": s.session_id,
        "status": s.status,
        "created_at": s.created_at.isoformat() if s.created_at else _ts(),
        "updated_at": s.updated_at.isoformat() if s.updated_at else _ts(),
        "last_run": s.last_run,
        "next_run": s.next_run,
        "run_count": s.run_count,
    }


def _task_to_dict(t: AgentTask) -> dict:
    return {
        "id": t.tid,
        "query": t.query,
        "session_id": t.session_id,
        "status": t.status,
        "result": t.result,
        "activity": t.activity or [],
        "created_at": t.created_at.isoformat() if t.created_at else _ts(),
        "updated_at": t.updated_at.isoformat() if t.updated_at else _ts(),
        "completed_at": t.completed_at,
    }


def _monitor_to_dict(m: AgentMonitor) -> dict:
    return {
        "id": m.mid,
        "name": m.name,
        "source_type": m.source_type,
        "source_config": m.source_config or {},
        "trigger_config": m.trigger_config or {},
        "analysis_query": m.analysis_query,
        "check_interval_seconds": m.check_interval_seconds,
        "status": m.status,
        "created_at": m.created_at.isoformat() if m.created_at else _ts(),
        "updated_at": m.updated_at.isoformat() if m.updated_at else _ts(),
        "last_check": m.last_check,
        "last_triggered": m.last_triggered,
        "check_count": m.check_count,
    }


# ── Memory ──────────────────────────────────────────────────────────────────

@router.post("/memory")
async def create_memory(body: dict, db: AsyncSession = Depends(get_db)):
    key = body.get("key", str(uuid.uuid4()))
    memory = AgentMemory(
        key=key,
        value=body.get("value", ""),
        memory_type=body.get("memory_type", "general"),
    )
    db.add(memory)
    await db.commit()
    await db.refresh(memory)
    return {"success": True, "data": {"memory": _memory_to_dict(memory)}}


@router.get("/memory")
async def list_memories(memory_type: Optional[str] = None, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentMemory)
    if memory_type:
        stmt = stmt.where(AgentMemory.memory_type == memory_type)
    result = await db.execute(stmt)
    memories = result.scalars().all()
    return {"success": True, "data": {"memories": [_memory_to_dict(m) for m in memories], "count": len(memories)}}


@router.delete("/memory/{key}")
async def delete_memory(key: str, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentMemory).where(AgentMemory.key == key)
    result = await db.execute(stmt)
    memory = result.scalar_one_or_none()
    if not memory:
        return {"success": False, "error": f"Memory key '{key}' not found"}
    await db.delete(memory)
    await db.commit()
    return {"success": True, "data": {"deleted": True, "key": key}}


# ── Schedules ───────────────────────────────────────────────────────────────

@router.post("/schedules")
async def create_schedule(body: dict, db: AsyncSession = Depends(get_db)):
    schedule = AgentSchedule(
        query=body.get("query", ""),
        cron_expression=body.get("cron_expression", body.get("cron", "0 0 * * *")),
        session_id=body.get("session_id", ""),
    )
    db.add(schedule)
    await db.commit()
    await db.refresh(schedule)
    return {"success": True, "data": {"schedule": _schedule_to_dict(schedule)}}


@router.get("/schedules")
async def list_schedules(db: AsyncSession = Depends(get_db)):
    stmt = select(AgentSchedule).order_by(desc(AgentSchedule.created_at))
    result = await db.execute(stmt)
    schedules = result.scalars().all()
    return {"success": True, "data": {"schedules": [_schedule_to_dict(s) for s in schedules], "count": len(schedules)}}


@router.get("/schedules/{sid}")
async def get_schedule(sid: str, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentSchedule).where(AgentSchedule.sid == sid)
    result = await db.execute(stmt)
    schedule = result.scalar_one_or_none()
    if not schedule:
        return {"success": False, "error": f"Schedule '{sid}' not found"}
    return {"success": True, "data": {"schedule": _schedule_to_dict(schedule)}}


@router.put("/schedules/{sid}")
async def update_schedule(sid: str, body: dict, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentSchedule).where(AgentSchedule.sid == sid)
    result = await db.execute(stmt)
    schedule = result.scalar_one_or_none()
    if not schedule:
        return {"success": False, "error": f"Schedule '{sid}' not found"}
    for key in ("query", "cron_expression", "session_id", "status"):
        if key in body:
            setattr(schedule, key, body[key])
    schedule.updated_at = datetime.now(timezone.utc)
    await db.commit()
    await db.refresh(schedule)
    return {"success": True, "data": {"schedule": _schedule_to_dict(schedule)}}


@router.delete("/schedules/{sid}")
async def delete_schedule(sid: str, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentSchedule).where(AgentSchedule.sid == sid)
    result = await db.execute(stmt)
    schedule = result.scalar_one_or_none()
    if not schedule:
        return {"success": False, "error": f"Schedule '{sid}' not found"}
    await db.delete(schedule)
    await db.commit()
    return {"success": True, "data": {"deleted": True, "id": sid}}


@router.post("/schedules/{sid}/pause")
async def pause_schedule(sid: str, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentSchedule).where(AgentSchedule.sid == sid)
    result = await db.execute(stmt)
    schedule = result.scalar_one_or_none()
    if not schedule:
        return {"success": False, "error": f"Schedule '{sid}' not found"}
    schedule.status = "paused"
    schedule.updated_at = datetime.now(timezone.utc)
    await db.commit()
    await db.refresh(schedule)
    return {"success": True, "data": {"schedule": _schedule_to_dict(schedule)}}


@router.post("/schedules/{sid}/resume")
async def resume_schedule(sid: str, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentSchedule).where(AgentSchedule.sid == sid)
    result = await db.execute(stmt)
    schedule = result.scalar_one_or_none()
    if not schedule:
        return {"success": False, "error": f"Schedule '{sid}' not found"}
    schedule.status = "active"
    schedule.updated_at = datetime.now(timezone.utc)
    await db.commit()
    await db.refresh(schedule)
    return {"success": True, "data": {"schedule": _schedule_to_dict(schedule)}}


# ── Tasks ───────────────────────────────────────────────────────────────────

@router.post("/tasks")
async def create_task(body: dict, db: AsyncSession = Depends(get_db)):
    task = AgentTask(
        query=body.get("query", ""),
        session_id=body.get("session_id", ""),
        status=body.get("status", "pending"),
        result=body.get("result"),
    )
    db.add(task)
    await db.commit()
    await db.refresh(task)
    return {"success": True, "data": {"task": _task_to_dict(task)}}


@router.get("/tasks")
async def list_tasks(status: Optional[str] = None, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentTask).order_by(desc(AgentTask.created_at))
    if status:
        stmt = stmt.where(AgentTask.status == status)
    result = await db.execute(stmt)
    tasks = result.scalars().all()
    return {"success": True, "data": {"tasks": [_task_to_dict(t) for t in tasks], "count": len(tasks)}}


@router.get("/tasks/history")
async def task_history(limit: int = 50, db: AsyncSession = Depends(get_db)):
    stmt = (
        select(AgentTask)
        .where(AgentTask.status.in_(["completed", "failed"]))
        .order_by(desc(AgentTask.created_at))
        .limit(limit)
    )
    result = await db.execute(stmt)
    tasks = result.scalars().all()
    return {"success": True, "data": {"history": [_task_to_dict(t) for t in tasks], "count": len(tasks)}}


@router.get("/tasks/{tid}")
async def get_task(tid: str, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentTask).where(AgentTask.tid == tid)
    result = await db.execute(stmt)
    task = result.scalar_one_or_none()
    if not task:
        return {"success": False, "error": f"Task '{tid}' not found"}
    return {"success": True, "data": {"task": _task_to_dict(task)}}


@router.delete("/tasks/{tid}")
async def delete_task(tid: str, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentTask).where(AgentTask.tid == tid)
    result = await db.execute(stmt)
    task = result.scalar_one_or_none()
    if not task:
        return {"success": False, "error": f"Task '{tid}' not found"}
    await db.delete(task)
    await db.commit()
    return {"success": True, "data": {"deleted": True, "id": tid}}


@router.post("/tasks/{tid}/activity")
async def add_task_activity(tid: str, body: dict, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentTask).where(AgentTask.tid == tid)
    result = await db.execute(stmt)
    task = result.scalar_one_or_none()
    if not task:
        return {"success": False, "error": f"Task '{tid}' not found"}
    activity_list = task.activity or []
    activity_entry = {**body, "timestamp": _ts()}
    activity_list.append(activity_entry)
    task.activity = activity_list
    if body.get("status"):
        task.status = body["status"]
        if body["status"] in ("completed", "failed"):
            task.completed_at = _ts()
    task.updated_at = datetime.now(timezone.utc)
    await db.commit()
    await db.refresh(task)
    return {"success": True, "data": {"task": _task_to_dict(task)}}


# ── Monitors ────────────────────────────────────────────────────────────────

@router.post("/monitors")
async def create_monitor(body: dict, db: AsyncSession = Depends(get_db)):
    monitor = AgentMonitor(
        name=body.get("name", "Unnamed Monitor"),
        source_type=body.get("source_type", "api"),
        source_config=body.get("source_config", {}),
        trigger_config=body.get("trigger_config", {}),
        analysis_query=body.get("analysis_query", ""),
        check_interval_seconds=body.get("check_interval_seconds", 3600),
    )
    db.add(monitor)
    await db.commit()
    await db.refresh(monitor)
    return {"success": True, "data": {"monitor": _monitor_to_dict(monitor)}}


@router.get("/monitors")
async def list_monitors(status: Optional[str] = None, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentMonitor).order_by(desc(AgentMonitor.created_at))
    if status:
        stmt = stmt.where(AgentMonitor.status == status)
    result = await db.execute(stmt)
    monitors = result.scalars().all()
    return {"success": True, "data": {"monitors": [_monitor_to_dict(m) for m in monitors], "count": len(monitors)}}


@router.get("/monitors/{mid}")
async def get_monitor(mid: str, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentMonitor).where(AgentMonitor.mid == mid)
    result = await db.execute(stmt)
    monitor = result.scalar_one_or_none()
    if not monitor:
        return {"success": False, "error": f"Monitor '{mid}' not found"}
    return {"success": True, "data": {"monitor": _monitor_to_dict(monitor)}}


@router.put("/monitors/{mid}")
async def update_monitor(mid: str, body: dict, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentMonitor).where(AgentMonitor.mid == mid)
    result = await db.execute(stmt)
    monitor = result.scalar_one_or_none()
    if not monitor:
        return {"success": False, "error": f"Monitor '{mid}' not found"}
    for key in ("name", "source_type", "source_config", "trigger_config", "analysis_query", "check_interval_seconds", "status"):
        if key in body:
            setattr(monitor, key, body[key])
    monitor.updated_at = datetime.now(timezone.utc)
    await db.commit()
    await db.refresh(monitor)
    return {"success": True, "data": {"monitor": _monitor_to_dict(monitor)}}


@router.delete("/monitors/{mid}")
async def delete_monitor(mid: str, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentMonitor).where(AgentMonitor.mid == mid)
    result = await db.execute(stmt)
    monitor = result.scalar_one_or_none()
    if not monitor:
        return {"success": False, "error": f"Monitor '{mid}' not found"}
    await db.delete(monitor)
    await db.commit()
    return {"success": True, "data": {"deleted": True, "id": mid}}


@router.post("/monitors/{mid}/pause")
async def pause_monitor(mid: str, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentMonitor).where(AgentMonitor.mid == mid)
    result = await db.execute(stmt)
    monitor = result.scalar_one_or_none()
    if not monitor:
        return {"success": False, "error": f"Monitor '{mid}' not found"}
    monitor.status = "paused"
    monitor.updated_at = datetime.now(timezone.utc)
    await db.commit()
    await db.refresh(monitor)
    return {"success": True, "data": {"monitor": _monitor_to_dict(monitor)}}


@router.post("/monitors/{mid}/resume")
async def resume_monitor(mid: str, db: AsyncSession = Depends(get_db)):
    stmt = select(AgentMonitor).where(AgentMonitor.mid == mid)
    result = await db.execute(stmt)
    monitor = result.scalar_one_or_none()
    if not monitor:
        return {"success": False, "error": f"Monitor '{mid}' not found"}
    monitor.status = "active"
    monitor.updated_at = datetime.now(timezone.utc)
    await db.commit()
    await db.refresh(monitor)
    return {"success": True, "data": {"monitor": _monitor_to_dict(monitor)}}


# ── Terminal Tools & MCP (remain in-memory) ────────────────────────────────

_terminal_tools: dict = {"tools": [], "versions": []}
_mcp_servers: dict[str, dict] = {}


@router.post("/terminal-tools/register")
async def register_tools(body: dict):
    tools = body.get("tools", [])
    version = body.get("version", "1.0.0")
    tool_count = body.get("tool_count", len(tools))
    registration = {
        "tools": tools,
        "version": version,
        "tool_count": tool_count,
        "registered_at": _ts(),
    }
    _terminal_tools["tools"] = tools
    _terminal_tools["current_version"] = version
    _terminal_tools["versions"].append({"version": version, "tool_count": tool_count, "registered_at": _ts()})
    return {"success": True, "data": {"registration": registration, "registered": len(tools)}}


@router.get("/terminal-tools")
async def get_terminal_tools():
    return {
        "success": True,
        "data": {
            "tools": _terminal_tools.get("tools", []),
            "current_version": _terminal_tools.get("current_version", ""),
        },
    }


@router.get("/terminal-tools/versions")
async def get_tool_versions():
    return {"success": True, "data": {"versions": _terminal_tools.get("versions", [])}}


@router.post("/mcp/servers")
async def create_mcp_server(body: dict):
    mid = str(uuid.uuid4())
    entry = {
        "id": mid,
        "name": body.get("name", "Unnamed MCP Server"),
        "config": body.get("config", {}),
        "tools": body.get("tools", []),
        "status": body.get("status", "disconnected"),
        "created_at": _ts(),
        "updated_at": _ts(),
        "last_connected": None,
    }
    _mcp_servers[mid] = entry
    return {"success": True, "data": {"server": entry}}


@router.get("/mcp/servers")
async def list_mcp_servers():
    servers = list(_mcp_servers.values())
    return {"success": True, "data": {"servers": servers, "count": len(servers)}}


@router.get("/mcp/servers/{mid}")
async def get_mcp_server(mid: str):
    entry = _mcp_servers.get(mid)
    if not entry:
        return {"success": False, "error": f"MCP server '{mid}' not found"}
    return {"success": True, "data": {"server": entry}}


@router.put("/mcp/servers/{mid}")
async def update_mcp_server(mid: str, body: dict):
    entry = _mcp_servers.get(mid)
    if not entry:
        return {"success": False, "error": f"MCP server '{mid}' not found"}
    for key in ("name", "config", "tools", "status"):
        if key in body:
            entry[key] = body[key]
    entry["updated_at"] = _ts()
    return {"success": True, "data": {"server": entry}}


@router.delete("/mcp/servers/{mid}")
async def delete_mcp_server(mid: str):
    if mid not in _mcp_servers:
        return {"success": False, "error": f"MCP server '{mid}' not found"}
    del _mcp_servers[mid]
    return {"success": True, "data": {"deleted": True, "id": mid}}


# ── Chat endpoint (remains as-is) ──────────────────────────────────────────

@router.post("/chat")
async def agent_chat(body: dict):
    query = body.get("query", "")
    messages = body.get("messages", [])
    session_id = body.get("session_id", "")
    logger.info(f"Agent chat: {query[:100] if query else 'msg count: ' + str(len(messages))}")

    if not messages and query:
        messages = [{"role": "user", "content": query}]

    from app.config import settings
    import httpx

    try:
        async with httpx.AsyncClient(timeout=120) as client:
            headers = {"Content-Type": "application/json"}
            api_key = settings.LLM_PROVIDER_API_KEY or ""
            base_url = settings.LLM_PROVIDER_BASE_URL or "http://localhost:11434/v1"
            if api_key and api_key != "ollama":
                headers["Authorization"] = f"Bearer {api_key}"

            resp = await client.post(
                f"{base_url}/chat/completions",
                headers=headers,
                json={
                    "model": settings.LLM_DEFAULT_MODEL or "llama3.2:1b",
                    "messages": messages,
                    "max_tokens": 4096,
                    "temperature": 0.7,
                },
            )
            data = resp.json()
            content = data.get("choices", [{}])[0].get("message", {}).get("content", "")
            return {"success": True, "data": {"response": content, "session_id": session_id}}
    except Exception as e:
        logger.error(f"Agent chat error: {e}")
        return {"success": True, "data": {"response": f"AI error: {e}", "session_id": session_id}}


from fastapi.responses import StreamingResponse

@router.post("/stream")
async def agent_stream(body: dict):
    query = body.get("query", "")
    messages = body.get("messages", [])
    session_id = body.get("session_id", "")

    if not messages and query:
        messages = [{"role": "user", "content": query}]

    from app.config import settings
    import httpx

    async def stream():
        try:
            async with httpx.AsyncClient(timeout=300) as client:
                headers = {"Content-Type": "application/json"}
                api_key = settings.LLM_PROVIDER_API_KEY or ""
                base_url = settings.LLM_PROVIDER_BASE_URL or "http://localhost:11434/v1"
                if api_key and api_key != "ollama":
                    headers["Authorization"] = f"Bearer {api_key}"

                async with client.stream(
                    "POST",
                    f"{base_url}/chat/completions",
                    headers=headers,
                    json={
                        "model": settings.LLM_DEFAULT_MODEL or "llama3.2:1b",
                        "messages": messages,
                        "max_tokens": 4096,
                        "temperature": 0.7,
                        "stream": True,
                    },
                ) as resp:
                    async for line in resp.aiter_lines():
                        if line.startswith("data: "):
                            yield line + "\n\n"
        except Exception as e:
            yield f"data: {{\"error\": \"{e}\"}}\n\n"

    return StreamingResponse(stream(), media_type="text/event-stream")
