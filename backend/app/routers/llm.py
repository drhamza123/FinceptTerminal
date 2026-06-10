import json
import logging
import threading
import uuid
from datetime import datetime, timezone

import httpx
from fastapi import APIRouter, Header, HTTPException

from app.config import settings

logger = logging.getLogger("guardian.llm")
router = APIRouter(tags=["llm"])

_pending_tasks: dict[str, dict] = {}
_tasks_lock = threading.Lock()

FINCEPT_MODELS = [
    {"id": "MiniMax-M2.7"},
    {"id": "MiniMax-M2.7-highspeed"},
    {"id": "MiniMax-M2.5"},
    {"id": "MiniMax-M2.5-highspeed"},
    {"id": "guardian-default"},
]


def _proxy_to_llm(messages: list, model: str | None = None, max_tokens: int = 4096, temperature: float = 0.7):
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

    try:
        headers = {"Content-Type": "application/json"}
        if api_key and api_key != "ollama":
            headers["Authorization"] = f"Bearer {api_key}"
        with httpx.Client(timeout=120.0) as client:
            resp = client.post(
                f"{base_url}/chat/completions",
                headers=headers,
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


@router.post("/v1/chat/completions")
@router.post("/chat/completions")
def openai_proxy(body: dict):
    """OpenAI-compatible proxy that forwards to Ollama."""
    return _proxy_to_llm(
        body.get("messages", []),
        body.get("model", settings.LLM_DEFAULT_MODEL),
        body.get("max_tokens", 4096),
        body.get("temperature", 0.7),
    )


@router.post("/research/chat")
@router.post("/research/llm/chat")
def chat_sync(body: dict):
    messages = body.get("messages", [])
    model = body.get("model")
    max_tokens = body.get("max_tokens", 4096)
    temperature = body.get("temperature", 0.7)

    result = _proxy_to_llm(messages, model, max_tokens, temperature)
    return {"success": True, "message": "OK", "data": result}


@router.post("/research/llm/async")
def chat_async(body: dict):
    prompt = body.get("prompt", "")
    max_tokens = body.get("max_tokens", 4096)
    task_id = str(uuid.uuid4())

    with _tasks_lock:
        _pending_tasks[task_id] = {
            "status": "processing",
            "created_at": datetime.now(timezone.utc).isoformat(),
        }

    t = threading.Thread(target=_process_async_task, args=(task_id, prompt, max_tokens), daemon=True)
    t.start()

    return {"success": True, "message": "Task submitted", "data": {"task_id": task_id}}


def _process_async_task(task_id: str, prompt: str, max_tokens: int):
    try:
        result = _proxy_to_llm(
            [{"role": "user", "content": prompt}],
            max_tokens=max_tokens,
        )
        content = result.get("choices", [{}])[0].get("message", {}).get("content", "")
        usage = result.get("usage", {})
        with _tasks_lock:
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
        with _tasks_lock:
            _pending_tasks[task_id] = {"status": "failed", "error": str(e)}


@router.get("/research/llm/status/{task_id}")
def chat_status(task_id: str):
    with _tasks_lock:
        task = _pending_tasks.get(task_id)
    if not task:
        raise HTTPException(status_code=404, detail={"success": False, "message": "Task not found."})
    return {"success": True, "message": "OK", "data": task}


@router.get("/research/llm/tags")
@router.get("/llm/tags")
def ollama_tags():
    """Proxy for Ollama's /api/tags — the app calls this to list models."""
    try:
        with httpx.Client(timeout=5) as c:
            r = c.get(f"{settings.LLM_PROVIDER_BASE_URL.rstrip('/v1')}/api/tags")
            if r.status_code == 200:
                return r.json()
            return {"success": False, "message": f"Ollama returned {r.status_code}", "data": {"models": []}}
    except Exception as e:
        logger.warning(f"Failed to get Ollama tags: {e}")
        return {"success": False, "message": str(e), "data": {"models": []}}


@router.get("/research/llm/health")
def llm_health():
    try:
        with httpx.Client(timeout=5) as c:
            r = c.get(f"{settings.LLM_PROVIDER_BASE_URL.rstrip('/v1')}/api/tags")
            if r.status_code == 200:
                models = r.json().get("models", [])
                return {"model": settings.LLM_DEFAULT_MODEL, "url": settings.LLM_PROVIDER_BASE_URL, "models": [m["name"] for m in models]}
            return {"model": settings.LLM_DEFAULT_MODEL, "url": settings.LLM_PROVIDER_BASE_URL, "error": f"Ollama returned {r.status_code}"}
    except Exception as e:
        return {"model": settings.LLM_DEFAULT_MODEL, "url": settings.LLM_PROVIDER_BASE_URL, "error": str(e)}


@router.get("/research/llm/models")
@router.get("/llm/models")
def list_models(apikey: str | None = Header(None, alias="X-API-Key")):
    try:
        with httpx.Client(timeout=5) as c:
            r = c.get(f"{settings.LLM_PROVIDER_BASE_URL.rstrip('/v1')}/api/tags")
            if r.status_code == 200:
                data = r.json()
                ollama_models = [{"id": m["name"]} for m in data.get("models", [])]
                all_models = FINCEPT_MODELS + ollama_models
                return {"success": True, "data": {"models": all_models}}
            return {"success": False, "data": {"models": FINCEPT_MODELS}}
    except Exception as e:
        logger.warning(f"Failed to get Ollama models: {e}")
        return {"success": False, "data": {"models": FINCEPT_MODELS}}
