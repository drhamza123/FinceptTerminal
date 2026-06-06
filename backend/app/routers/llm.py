import asyncio
import json
import logging
import uuid
from datetime import datetime, timezone

import httpx
from fastapi import APIRouter, Header, HTTPException

from app.config import settings

logger = logging.getLogger("guardian.llm")
router = APIRouter(tags=["llm"])

_pending_tasks: dict[str, dict] = {}

FINCEPT_MODELS = [
    {"id": "MiniMax-M2.7"},
    {"id": "MiniMax-M2.7-highspeed"},
    {"id": "MiniMax-M2.5"},
    {"id": "MiniMax-M2.5-highspeed"},
    {"id": "guardian-default"},
]


async def _proxy_to_llm(messages: list, model: str | None = None, max_tokens: int = 4096, temperature: float = 0.7):
    api_key = settings.LLM_PROVIDER_API_KEY
    base_url = settings.LLM_PROVIDER_BASE_URL
    model = model or settings.LLM_DEFAULT_MODEL

    if not api_key:
        return {
            "choices": [{
                "message": {
                    "content": "LLM proxy not configured. Set LLM_PROVIDER_API_KEY in your .env file.",
                }
            }]
        }

    async with httpx.AsyncClient(timeout=120.0) as client:
        try:
            resp = await client.post(
                f"{base_url}/chat/completions",
                headers={"Authorization": f"Bearer {api_key}", "Content-Type": "application/json"},
                json={"model": model, "messages": messages, "max_tokens": max_tokens, "temperature": temperature},
            )
            resp.raise_for_status()
            return resp.json()
        except Exception as e:
            logger.error(f"LLM proxy error: {e}")
            return {
                "choices": [{
                    "message": {"content": f"Guardian AI error: {e}"}
                }]
            }


@router.post("/research/chat")
async def chat_sync(body: dict):
    messages = body.get("messages", [])
    model = body.get("model")
    max_tokens = body.get("max_tokens", 4096)
    temperature = body.get("temperature", 0.7)

    result = await _proxy_to_llm(messages, model, max_tokens, temperature)
    return {"success": True, "message": "OK", "data": result}


@router.post("/research/llm/async")
async def chat_async(body: dict, x_api_key: str = Header(default=None)):
    prompt = body.get("prompt", "")
    max_tokens = body.get("max_tokens", 4096)
    task_id = str(uuid.uuid4())

    _pending_tasks[task_id] = {
        "status": "processing",
        "created_at": datetime.now(timezone.utc).isoformat(),
    }

    asyncio.create_task(_process_async_task(task_id, prompt, max_tokens))

    return {"success": True, "message": "Task submitted", "data": {"task_id": task_id}}


async def _process_async_task(task_id: str, prompt: str, max_tokens: int):
    try:
        result = await _proxy_to_llm(
            [{"role": "user", "content": prompt}],
            max_tokens=max_tokens,
        )
        content = result.get("choices", [{}])[0].get("message", {}).get("content", "")
        usage = result.get("usage", {})
        _pending_tasks[task_id] = {
            "status": "completed",
            "data": {
                "response": content,
                "usage": {
                    "input_tokens": usage.get("prompt_tokens", 0),
                    "output_tokens": usage.get("completion_tokens", 0),
                    "total_tokens": usage.get("total_tokens", 0),
                },
            },
        }
    except Exception as e:
        _pending_tasks[task_id] = {"status": "failed", "error": str(e)}


@router.get("/research/llm/status/{task_id}")
async def chat_status(task_id: str):
    task = _pending_tasks.get(task_id)
    if not task:
        raise HTTPException(status_code=404, detail={"success": False, "message": "Task not found."})
    return {"success": True, "message": "OK", "data": task}


@router.get("/research/llm/models")
async def list_models():
    return {"success": True, "message": "OK", "data": {"models": FINCEPT_MODELS}}
