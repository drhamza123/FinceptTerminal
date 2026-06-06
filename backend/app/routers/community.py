import asyncio
import logging
import os
import shutil
from typing import Optional

from fastapi import APIRouter, BackgroundTasks, HTTPException, WebSocket, WebSocketDisconnect
from app.schemas.community import McpToolPackage, AgentConfigPackage

logger = logging.getLogger("guardian.community")

router = APIRouter(prefix="/community", tags=["community"])

TOOLS_DIR = os.path.expanduser("~/.fincept/mcp_tools")
AGENTS_DIR = os.path.expanduser("~/.fincept/agents")
os.makedirs(TOOLS_DIR, exist_ok=True)
os.makedirs(AGENTS_DIR, exist_ok=True)

# In-memory registries (replace with SQLite in production)
_tool_registry: dict[str, McpToolPackage] = {}
_agent_registry: dict[str, AgentConfigPackage] = {}
_next_id = 1

# ── Tools ─────────────────────────────────────────────────────

@router.post("/tools", response_model=McpToolPackage)
async def publish_tool(tool: McpToolPackage):
    global _next_id
    tool.id = f"tool_{_next_id}"; _next_id += 1
    tool.created_at = __import__("datetime").datetime.utcnow().isoformat()
    _tool_registry[tool.id] = tool
    logger.info("Tool published: %s v%s by %s", tool.name, tool.version, tool.author)
    return tool

@router.get("/tools")
async def list_tools(category: Optional[str] = None, search: Optional[str] = None):
    tools = list(_tool_registry.values())
    if category:
        tools = [t for t in tools if t.category.lower() == category.lower()]
    if search:
        q = search.lower()
        tools = [t for t in tools if q in t.name.lower() or q in t.description.lower()]
    return tools

@router.post("/tools/{tool_id}/install")
async def install_tool(tool_id: str, bg: BackgroundTasks):
    tool = _tool_registry.get(tool_id)
    if not tool:
        raise HTTPException(404, "Tool not found")
    tool.downloads += 1
    bg.add_task(_do_install, tool)
    return {"status": "queued", "tool_id": tool_id, "name": tool.name}

async def _do_install(tool: McpToolPackage):
    tool_path = os.path.join(TOOLS_DIR, f"{tool.name}.py")
    try:
        with open(tool_path, "w") as f:
            f.write(tool.python_code)
        if tool.dependencies:
            proc = await asyncio.create_subprocess_exec(
                *["pip3", "install", *tool.dependencies],
                stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE,
            )
            await asyncio.wait_for(proc.communicate(), timeout=120)
        logger.info("Tool installed: %s → %s", tool.name, tool_path)
        await _notify_mcp_reload(tool.name)
    except Exception as e:
        logger.error("Install failed for %s: %s", tool.name, e)

# ── Agents ────────────────────────────────────────────────────

@router.post("/agents", response_model=AgentConfigPackage)
async def publish_agent(agent: AgentConfigPackage):
    global _next_id
    agent.id = f"agent_{_next_id}"; _next_id += 1
    _agent_registry[agent.id] = agent
    return agent

@router.get("/agents")
async def list_agents():
    return list(_agent_registry.values())

@router.post("/agents/{agent_id}/install")
async def install_agent(agent_id: str, bg: BackgroundTasks):
    agent = _agent_registry.get(agent_id)
    if not agent:
        raise HTTPException(404, "Agent not found")
    agent.downloads += 1
    path = os.path.join(AGENTS_DIR, f"{agent.name}.json")
    import json
    with open(path, "w") as f:
        json.dump(agent.model_dump(), f, indent=2)
    return {"status": "installed", "name": agent.name}

# ── Hot-Reload Notification ───────────────────────────────────

MCP_WS_CLIENTS: set = set()

@router.websocket("/ws/mcp-reload")
async def mcp_reload_ws(ws: WebSocket):
    await ws.accept()
    MCP_WS_CLIENTS.add(ws)
    try:
        while True:
            await ws.receive_text()
    except WebSocketDisconnect:
        pass
    finally:
        MCP_WS_CLIENTS.discard(ws)

async def _notify_mcp_reload(tool_name: str):
    import json
    from fastapi import WebSocket
    msg = json.dumps({"type": "tool_installed", "name": tool_name, "path": os.path.join(TOOLS_DIR, f"{tool_name}.py")})
    dead = set()
    for ws in MCP_WS_CLIENTS:
        try:
            await ws.send_text(msg)
        except Exception:
            dead.add(ws)
    MCP_WS_CLIENTS.difference_update(dead)
