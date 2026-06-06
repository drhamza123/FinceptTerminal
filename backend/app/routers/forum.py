from datetime import datetime, timezone

from fastapi import APIRouter, Depends
from sqlalchemy import select, func, desc
from sqlalchemy.ext.asyncio import AsyncSession

from app.database import get_db
from app.models.forum import ForumCategory, ForumPost, ForumComment, ForumVote
from app.models.user import User
from app.routers.auth import resolve_user

router = APIRouter(tags=["forum"])

DEFAULT_CATEGORIES = [
    {"name": "General", "description": "General discussion", "color": "#22c55e", "display_order": 1},
    {"name": "Market Analysis", "description": "Market trends and analysis", "color": "#3b82f6", "display_order": 2},
    {"name": "Trading Strategies", "description": "Share and discuss strategies", "color": "#f59e0b", "display_order": 3},
    {"name": "Technical Support", "description": "Get help with the platform", "color": "#8b5cf6", "display_order": 4},
]


async def _ensure_categories(db: AsyncSession):
    stmt = select(func.count(ForumCategory.id))
    result = await db.execute(stmt)
    count = result.scalar() or 0
    if count == 0:
        now = datetime.now(timezone.utc)
        for cat in DEFAULT_CATEGORIES:
            db.add(ForumCategory(**cat, created_at=now))
        await db.commit()


def _category_to_dict(cat: ForumCategory) -> dict:
    return {
        "id": cat.id,
        "name": cat.name,
        "description": cat.description,
        "color": cat.color,
        "post_count": cat.post_count,
        "display_order": cat.display_order,
        "created_at": cat.created_at.isoformat() if cat.created_at else None,
    }


def _post_to_dict(post: ForumPost) -> dict:
    return {
        "uuid": post.uuid,
        "category_id": post.category_id,
        "author_id": post.author_id,
        "title": post.title,
        "content": post.content,
        "created_at": post.created_at.isoformat() if post.created_at else None,
        "updated_at": post.updated_at.isoformat() if post.updated_at else None,
    }


def _comment_to_dict(comment: ForumComment) -> dict:
    return {
        "uuid": comment.uuid,
        "post_id": comment.post_id,
        "author_id": comment.author_id,
        "content": comment.content,
        "created_at": comment.created_at.isoformat() if comment.created_at else None,
    }


@router.get("/forum/categories")
async def list_categories(db: AsyncSession = Depends(get_db)):
    await _ensure_categories(db)
    stmt = select(ForumCategory).order_by(ForumCategory.display_order)
    result = await db.execute(stmt)
    categories = result.scalars().all()
    return {
        "success": True,
        "data": {
            "categories": [_category_to_dict(c) for c in categories],
            "permissions": {"can_create_posts": True, "can_vote": True, "can_comment": True},
        },
    }


@router.get("/forum/stats")
async def forum_stats(db: AsyncSession = Depends(get_db)):
    await _ensure_categories(db)
    cat_count = (await db.execute(select(func.count(ForumCategory.id)))).scalar() or 0
    post_count = (await db.execute(select(func.count(ForumPost.id)))).scalar() or 0
    comment_count = (await db.execute(select(func.count(ForumComment.id)))).scalar() or 0
    vote_count = (await db.execute(select(func.count(ForumVote.id)))).scalar() or 0

    recent_stmt = select(ForumPost).where(
        ForumPost.created_at >= datetime.now(timezone.utc).replace(hour=0, minute=0, second=0)
    )
    recent_result = await db.execute(recent_stmt)
    recent_posts_24h = len(recent_result.scalars().all())

    popular_stmt = (
        select(ForumCategory)
        .order_by(ForumCategory.post_count.desc())
        .limit(5)
    )
    popular_result = await db.execute(popular_stmt)
    popular_categories = [_category_to_dict(c) for c in popular_result.scalars().all()]

    return {
        "success": True,
        "data": {
            "total_categories": cat_count,
            "total_posts": post_count,
            "total_comments": comment_count,
            "total_votes": vote_count,
            "recent_posts_24h": recent_posts_24h,
            "popular_categories": popular_categories,
            "top_contributors": [],
        },
    }


@router.get("/forum/categories/{category_id}/posts")
async def list_posts(category_id: int, db: AsyncSession = Depends(get_db)):
    await _ensure_categories(db)
    cat_stmt = select(ForumCategory).where(ForumCategory.id == category_id)
    cat_result = await db.execute(cat_stmt)
    category = cat_result.scalar_one_or_none()
    if not category:
        return {"success": False, "error": "Category not found"}

    post_stmt = (
        select(ForumPost)
        .where(ForumPost.category_id == category_id)
        .order_by(desc(ForumPost.created_at))
    )
    post_result = await db.execute(post_stmt)
    posts = post_result.scalars().all()

    return {
        "success": True,
        "data": {
            "posts": [_post_to_dict(p) for p in posts],
            "pagination": {"page": 1, "limit": 20, "total": len(posts), "pages": 1},
            "sort_by": "latest",
            "category": _category_to_dict(category),
        },
    }


@router.get("/forum/posts/trending")
async def trending_posts(db: AsyncSession = Depends(get_db)):
    stmt = select(ForumPost).order_by(desc(ForumPost.created_at)).limit(10)
    result = await db.execute(stmt)
    posts = result.scalars().all()
    return {
        "success": True,
        "data": {
            "trending_posts": [_post_to_dict(p) for p in posts],
            "total": len(posts),
        },
    }


@router.get("/forum/posts/{post_uuid}")
async def get_post(post_uuid: str, db: AsyncSession = Depends(get_db)):
    post_stmt = select(ForumPost).where(ForumPost.uuid == post_uuid)
    post_result = await db.execute(post_stmt)
    post = post_result.scalar_one_or_none()
    if not post:
        return {"success": False, "error": "Post not found"}

    comment_stmt = select(ForumComment).where(ForumComment.post_id == post.id).order_by(ForumComment.created_at)
    comment_result = await db.execute(comment_stmt)
    comments = comment_result.scalars().all()

    return {
        "success": True,
        "data": {
            "post": _post_to_dict(post),
            "comments": [_comment_to_dict(c) for c in comments],
            "total_comments": len(comments),
            "permissions": {"can_comment": True, "can_vote": True},
        },
    }


@router.post("/forum/posts")
async def create_post(body: dict, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    post = ForumPost(
        category_id=body.get("category_id", 1),
        author_id=user.id,
        title=body.get("title", ""),
        content=body.get("content", ""),
    )
    db.add(post)

    cat_stmt = select(ForumCategory).where(ForumCategory.id == post.category_id)
    cat_result = await db.execute(cat_stmt)
    category = cat_result.scalar_one_or_none()
    if category:
        category.post_count = (category.post_count or 0) + 1

    await db.commit()
    await db.refresh(post)

    return {"success": True, "data": {"post": _post_to_dict(post)}}


@router.post("/forum/posts/{post_uuid}/comments")
async def create_comment(post_uuid: str, body: dict, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    post_stmt = select(ForumPost).where(ForumPost.uuid == post_uuid)
    post_result = await db.execute(post_stmt)
    post = post_result.scalar_one_or_none()
    if not post:
        return {"success": False, "error": "Post not found"}

    comment = ForumComment(
        post_id=post.id,
        author_id=user.id,
        content=body.get("content", ""),
    )
    db.add(comment)
    await db.commit()
    await db.refresh(comment)

    return {"success": True, "data": {"comment": _comment_to_dict(comment)}}


@router.post("/forum/posts/{post_uuid}/vote")
async def vote_post(post_uuid: str, body: dict, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    post_stmt = select(ForumPost).where(ForumPost.uuid == post_uuid)
    post_result = await db.execute(post_stmt)
    post = post_result.scalar_one_or_none()
    if not post:
        return {"success": False, "error": "Post not found"}

    vote = ForumVote(
        post_id=post.id,
        user_id=user.id,
        vote_type=body.get("vote_type", "up"),
    )
    db.add(vote)
    await db.commit()
    await db.refresh(vote)

    return {"success": True, "data": {"vote": {"id": vote.id, "vote_type": vote.vote_type}}}


@router.get("/forum/search")
async def forum_search(q: str = "", db: AsyncSession = Depends(get_db)):
    if not q:
        return {"success": True, "data": {"results": {"posts": [], "total_results": 0}, "pagination": {"page": 1, "limit": 20}}}
    stmt = (
        select(ForumPost)
        .where(
            func.lower(ForumPost.title).contains(q.lower()) |
            func.lower(ForumPost.content).contains(q.lower())
        )
        .order_by(desc(ForumPost.created_at))
        .limit(20)
    )
    result = await db.execute(stmt)
    posts = result.scalars().all()

    return {
        "success": True,
        "data": {
            "results": {"posts": [_post_to_dict(p) for p in posts], "total_results": len(posts)},
            "pagination": {"page": 1, "limit": 20},
        },
    }


@router.get("/forum/profile")
async def forum_profile(user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    post_count = (await db.execute(select(func.count(ForumPost.id)).where(ForumPost.author_id == user.id))).scalar() or 0
    comment_count = (await db.execute(select(func.count(ForumComment.id)).where(ForumComment.author_id == user.id))).scalar() or 0

    recent_stmt = select(ForumPost).where(ForumPost.author_id == user.id).order_by(desc(ForumPost.created_at)).limit(10)
    recent_result = await db.execute(recent_stmt)
    recent_posts = recent_result.scalars().all()

    return {
        "success": True,
        "data": {
            "profile": {
                "username": user.username,
                "display_name": user.username,
                "reputation": 0,
                "posts_count": post_count,
                "comments_count": comment_count,
                "created_at": user.created_at.isoformat() if user.created_at else None,
            },
            "is_own_profile": True,
            "recent_posts": [_post_to_dict(p) for p in recent_posts],
        },
    }
