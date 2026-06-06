from fastapi import APIRouter, Depends
from sqlalchemy import select, desc
from sqlalchemy.ext.asyncio import AsyncSession

from app.database import get_db
from app.models.support import SupportTicket, SupportMessage
from app.models.user import User
from app.routers.auth import resolve_user

router = APIRouter(tags=["support"])

SUPPORT_CATEGORIES = [
    {"id": "general", "name": "General Inquiry"},
    {"id": "bug", "name": "Bug Report"},
    {"id": "feature", "name": "Feature Request"},
]


def _ticket_to_dict(ticket: SupportTicket) -> dict:
    return {
        "id": ticket.uuid,
        "subject": ticket.subject,
        "category": ticket.category,
        "status": ticket.status,
        "created_at": ticket.created_at.isoformat() if ticket.created_at else None,
        "updated_at": ticket.updated_at.isoformat() if ticket.updated_at else None,
    }


def _message_to_dict(msg: SupportMessage) -> dict:
    return {
        "uuid": msg.uuid,
        "author_id": msg.author_id,
        "content": msg.content,
        "created_at": msg.created_at.isoformat() if msg.created_at else None,
    }


@router.get("/support/tickets")
async def list_tickets(user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    stmt = select(SupportTicket).where(SupportTicket.user_id == user.id).order_by(desc(SupportTicket.created_at))
    result = await db.execute(stmt)
    tickets = result.scalars().all()
    return {"success": True, "data": [_ticket_to_dict(t) for t in tickets]}


@router.post("/support/tickets")
async def create_ticket(body: dict, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    ticket = SupportTicket(
        user_id=user.id,
        subject=body.get("subject", ""),
        category=body.get("category", "general"),
    )
    db.add(ticket)
    await db.commit()
    await db.refresh(ticket)
    return {"success": True, "message": "Ticket created", "data": _ticket_to_dict(ticket)}


@router.get("/support/tickets/{ticket_uuid}")
async def get_ticket(ticket_uuid: str, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    stmt = select(SupportTicket).where(SupportTicket.uuid == ticket_uuid, SupportTicket.user_id == user.id)
    result = await db.execute(stmt)
    ticket = result.scalar_one_or_none()
    if not ticket:
        return {"success": False, "error": "Ticket not found"}

    msg_stmt = select(SupportMessage).where(SupportMessage.ticket_id == ticket.id).order_by(SupportMessage.created_at)
    msg_result = await db.execute(msg_stmt)
    messages = msg_result.scalars().all()

    return {
        "success": True,
        "data": {
            "ticket": _ticket_to_dict(ticket),
            "messages": [_message_to_dict(m) for m in messages],
        },
    }


@router.post("/support/tickets/{ticket_uuid}/messages")
async def add_ticket_message(ticket_uuid: str, body: dict, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    stmt = select(SupportTicket).where(SupportTicket.uuid == ticket_uuid, SupportTicket.user_id == user.id)
    result = await db.execute(stmt)
    ticket = result.scalar_one_or_none()
    if not ticket:
        return {"success": False, "error": "Ticket not found"}

    msg = SupportMessage(
        ticket_id=ticket.id,
        author_id=user.id,
        content=body.get("content", ""),
    )
    db.add(msg)
    ticket.updated_at = msg.created_at
    await db.commit()
    await db.refresh(msg)

    return {"success": True, "data": {"message": _message_to_dict(msg)}}


@router.get("/support/categories")
async def list_support_categories():
    return {"success": True, "data": SUPPORT_CATEGORIES}
