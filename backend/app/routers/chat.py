import uuid
from datetime import datetime, timezone
from typing import Any

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select, func, or_
from sqlalchemy.ext.asyncio import AsyncSession

from app.database import get_db
from app.models.chat import ChatSession, ChatMessage, ChatMemory
from app.routers.auth import resolve_user
from app.models.user import User

router = APIRouter(tags=["chat"])


def _now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _session_not_found(uuid_str: str):
    raise HTTPException(
        status_code=404,
        detail={"success": False, "message": f"Session '{uuid_str}' not found."},
    )


def _session_to_dict(session: ChatSession) -> dict:
    return {
        "session_uuid": session.uuid,
        "title": session.title,
        "created_at": session.created_at.isoformat() if session.created_at else _now(),
        "updated_at": session.updated_at.isoformat() if session.updated_at else _now(),
        "active": session.active,
    }


def _message_to_dict(msg: ChatMessage) -> dict:
    return {
        "message_uuid": msg.uuid,
        "role": msg.role,
        "content": msg.content,
        "provider": msg.provider,
        "model": msg.model,
        "tokens_used": msg.tokens_used,
        "response_time_ms": msg.response_time_ms,
        "created_at": msg.created_at.isoformat() if msg.created_at else _now(),
    }


@router.post("/chat/sessions")
async def create_session(body: dict, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    now = datetime.now(timezone.utc)
    session = ChatSession(
        user_id=user.id,
        title=body.get("title", "New Chat"),
        created_at=now,
        updated_at=now,
    )
    db.add(session)
    await db.commit()
    await db.refresh(session)
    return {"success": True, "data": {"session": _session_to_dict(session)}}


@router.get("/chat/sessions")
async def list_sessions(user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    stmt = (
        select(ChatSession)
        .where(ChatSession.user_id == user.id, ChatSession.active == True)
        .order_by(ChatSession.updated_at.desc())
    )
    result = await db.execute(stmt)
    sessions = result.scalars().all()
    return {"success": True, "data": {"sessions": [_session_to_dict(s) for s in sessions]}}


@router.get("/chat/sessions/{session_uuid}")
async def get_session(session_uuid: str, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    stmt = select(ChatSession).where(ChatSession.uuid == session_uuid, ChatSession.user_id == user.id)
    result = await db.execute(stmt)
    session = result.scalar_one_or_none()
    if not session:
        _session_not_found(session_uuid)

    msg_stmt = (
        select(ChatMessage)
        .where(ChatMessage.session_id == session.id)
        .order_by(ChatMessage.created_at.asc())
    )
    msg_result = await db.execute(msg_stmt)
    messages = msg_result.scalars().all()

    return {
        "success": True,
        "data": {
            "session": _session_to_dict(session),
            "messages": [_message_to_dict(m) for m in messages],
        },
    }


@router.delete("/chat/sessions/{session_uuid}")
async def delete_session(session_uuid: str, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    stmt = select(ChatSession).where(ChatSession.uuid == session_uuid, ChatSession.user_id == user.id)
    result = await db.execute(stmt)
    session = result.scalar_one_or_none()
    if not session:
        _session_not_found(session_uuid)
    await db.delete(session)
    await db.commit()
    return {"success": True, "message": "Session deleted."}


@router.put("/chat/sessions/{session_uuid}/title")
async def update_session_title(session_uuid: str, body: dict, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    stmt = select(ChatSession).where(ChatSession.uuid == session_uuid, ChatSession.user_id == user.id)
    result = await db.execute(stmt)
    session = result.scalar_one_or_none()
    if not session:
        _session_not_found(session_uuid)
    session.title = body.get("title", session.title)
    session.updated_at = datetime.now(timezone.utc)
    await db.commit()
    await db.refresh(session)
    return {"success": True, "data": {"session": _session_to_dict(session)}}


@router.put("/chat/sessions/{session_uuid}/activate")
async def activate_session(session_uuid: str, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    stmt = select(ChatSession).where(ChatSession.uuid == session_uuid, ChatSession.user_id == user.id)
    result = await db.execute(stmt)
    session = result.scalar_one_or_none()
    if not session:
        _session_not_found(session_uuid)
    session.active = True
    session.updated_at = datetime.now(timezone.utc)
    await db.commit()
    await db.refresh(session)
    return {"success": True, "data": {"session": _session_to_dict(session)}}


@router.post("/chat/sessions/{session_uuid}/save-message")
async def save_message(session_uuid: str, body: dict, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    stmt = select(ChatSession).where(ChatSession.uuid == session_uuid, ChatSession.user_id == user.id)
    result = await db.execute(stmt)
    session = result.scalar_one_or_none()
    if not session:
        _session_not_found(session_uuid)

    msg = ChatMessage(
        session_id=session.id,
        role=body.get("role", "user"),
        content=body.get("content", ""),
        provider=body.get("provider", ""),
        model=body.get("model", ""),
        tokens_used=body.get("tokens_used", 0),
        response_time_ms=body.get("response_time_ms", 0),
    )
    db.add(msg)
    session.updated_at = datetime.now(timezone.utc)
    await db.commit()
    await db.refresh(msg)

    msg_count_stmt = select(func.count(ChatMessage.id)).where(ChatMessage.session_id == session.id)
    count_result = await db.execute(msg_count_stmt)
    msg_count = count_result.scalar() or 0

    auto_title = None
    if msg_count == 1:
        raw = msg.content[:60]
        auto_title = raw + ("..." if len(msg.content) > 60 else "")

    return {
        "success": True,
        "data": {
            "message": _message_to_dict(msg),
            "new_title": auto_title,
        },
    }


@router.get("/chat/stats")
async def chat_stats(user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    session_count_stmt = select(func.count(ChatSession.id)).where(ChatSession.user_id == user.id)
    session_count_result = await db.execute(session_count_stmt)
    total_sessions = session_count_result.scalar() or 0

    message_count_stmt = select(func.count(ChatMessage.id)).where(
        ChatMessage.session_id.in_(select(ChatSession.id).where(ChatSession.user_id == user.id))
    )
    message_count_result = await db.execute(message_count_stmt)
    total_messages = message_count_result.scalar() or 0

    return {
        "success": True,
        "data": {
            "total_sessions": total_sessions,
            "total_messages": total_messages,
        },
    }


@router.get("/chat/search")
async def search_messages(query: str = "", user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    if not query:
        return {"success": True, "data": {"results": []}}
    q = query.lower()
    msg_stmt = (
        select(ChatMessage)
        .join(ChatSession, ChatMessage.session_id == ChatSession.id)
        .where(
            ChatSession.user_id == user.id,
            ChatSession.active == True,
            func.lower(ChatMessage.content).contains(q),
        )
        .order_by(ChatMessage.created_at.desc())
    )
    result = await db.execute(msg_stmt)
    messages = result.scalars().all()

    session_ids = {m.session_id for m in messages}
    if session_ids:
        sess_stmt = select(ChatSession).where(ChatSession.id.in_(session_ids))
        sess_result = await db.execute(sess_stmt)
        sessions_map = {s.id: s for s in sess_result.scalars().all()}
    else:
        sessions_map = {}

    results = []
    for msg in messages:
        s = sessions_map.get(msg.session_id)
        results.append({
            "session_uuid": s.uuid if s else "",
            "session_title": s.title if s else "",
            "message": _message_to_dict(msg),
        })
    return {"success": True, "data": {"results": results}}


@router.post("/chat/export")
async def export_sessions(user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    sess_stmt = select(ChatSession).where(ChatSession.user_id == user.id).order_by(ChatSession.updated_at.desc())
    sess_result = await db.execute(sess_stmt)
    sessions = sess_result.scalars().all()

    exported = []
    for s in sessions:
        msg_stmt = select(ChatMessage).where(ChatMessage.session_id == s.id).order_by(ChatMessage.created_at.asc())
        msg_result = await db.execute(msg_stmt)
        messages = msg_result.scalars().all()
        exported.append({
            **_session_to_dict(s),
            "messages": [_message_to_dict(m) for m in messages],
        })

    return {
        "success": True,
        "data": {
            "exported_at": _now(),
            "sessions": exported,
        },
    }


@router.post("/chat/optimize-prompt")
async def optimize_prompt(body: dict):
    prompt = body.get("prompt", "")
    return {
        "success": True,
        "data": {
            "optimized_prompt": prompt,
        },
    }
