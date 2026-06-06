import asyncio
import json
import logging
import os
import time
from datetime import datetime, timezone

import msgpack

from fastapi import APIRouter, Depends, WebSocket, WebSocketDisconnect
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.database import async_session_factory, get_db
from app.models.ea import EAInstance, EATemplate, instance_to_dict
from app.models.user import User
from app.routers.auth import resolve_user
from app.services.mt5_compiler import compile_mql5, deploy_to_experts
from app.services.mt5_protocol import (
    MQL5_AGENT_PROMPT,
    cmd_close_all,
    cmd_ping,
    cmd_set_params,
    cmd_shutdown,
    cmd_bracket_order as proto_bracket,
    cmd_oco_order as proto_oco,
    cmd_oto_order as proto_oto,
    cmd_trailing_stop as proto_trail,
    cmd_pending_order as proto_pending,
    cmd_cancel_pending as proto_cancel_pending,
    cmd_iceberg_order as proto_iceberg,
    cmd_close_order as proto_close,
    cmd_modify_order as proto_modify,
    parse_ea_message,
)
from app.services.order_manager import (
    OrderManager, OrderSide, OrderType,
    get_order_manager,
)

logger = logging.getLogger("guardian.mt5")
router = APIRouter(tags=["mt5_bridge"])

EA_KEY_PREFIX = "mt5:"
TCP_HOST = os.environ.get("MT5_BRIDGE_HOST", "127.0.0.1")
TCP_PORT = int(os.environ.get("MT5_BRIDGE_PORT", "5556"))

active_connections: dict = {}
pending_commands: dict = {}
ws_clients: set = set()
_ping_counters: dict = {}
_tcp_server: asyncio.AbstractServer | None = None
_watchdog_task: asyncio.Task | None = None


def _now() -> float:
    return datetime.now(timezone.utc).timestamp()


def _make_ea_key(name: str, magic: int) -> str:
    return f"{EA_KEY_PREFIX}{name}:{magic}"


# ── DB helpers (for internal use outside Depends) ──


async def _upsert_instance(ea_key: str, msg: dict) -> EAInstance | None:
    name = msg.get("ea_name", "Unknown")
    magic = msg.get("magic", 0)
    async with async_session_factory() as db:
        stmt = select(EAInstance).where(EAInstance.magic_number == magic, EAInstance.ea_name == name)
        result = await db.execute(stmt)
        instance = result.scalar_one_or_none()
        if instance:
            instance.status = "running"
            instance.symbol = msg.get("symbol", instance.symbol)
            instance.timeframe = msg.get("tf", instance.timeframe)
            instance.balance = msg.get("balance", instance.balance)
            instance.equity = msg.get("equity", instance.equity)
            instance.connected_at = datetime.now(timezone.utc)
            instance.last_heartbeat_at = datetime.now(timezone.utc)
        else:
            instance = EAInstance(
                ea_name=name, magic_number=magic,
                symbol=msg.get("symbol", ""), timeframe=msg.get("tf", ""),
                status="running", balance=msg.get("balance", 0.0), equity=msg.get("equity", 0.0),
                connected_at=datetime.now(timezone.utc), last_heartbeat_at=datetime.now(timezone.utc),
            )
            db.add(instance)
        await db.commit()
        await db.refresh(instance)
        return instance


async def _update_instance_heartbeat(ea_key: str, msg: dict):
    try:
        name, magic_str = ea_key.replace(EA_KEY_PREFIX, "").rsplit(":", 1)
    except ValueError:
        return
    async with async_session_factory() as db:
        stmt = select(EAInstance).where(EAInstance.magic_number == int(magic_str), EAInstance.ea_name == name)
        result = await db.execute(stmt)
        instance = result.scalar_one_or_none()
        if instance:
            instance.balance = msg.get("balance", instance.balance)
            instance.equity = msg.get("equity", instance.equity)
            instance.status = msg.get("status", instance.status)
            instance.last_heartbeat_at = datetime.now(timezone.utc)
            await db.commit()


async def _set_instance_status(ea_key: str, status: str):
    try:
        name, magic_str = ea_key.replace(EA_KEY_PREFIX, "").rsplit(":", 1)
    except ValueError:
        return
    async with async_session_factory() as db:
        stmt = select(EAInstance).where(EAInstance.magic_number == int(magic_str), EAInstance.ea_name == name)
        result = await db.execute(stmt)
        instance = result.scalar_one_or_none()
        if instance:
            instance.status = status
            await db.commit()


async def _send_to_ea(ea_key: str, command: str):
    conn = active_connections.get(ea_key)
    if conn:
        try:
            conn["writer"].write(command.encode())
            await conn["writer"].drain()
        except Exception as e:
            logger.error("Failed to send to %s: %s", ea_key, e)
            pending_commands.setdefault(ea_key, []).append(command.encode())
    else:
        pending_commands.setdefault(ea_key, []).append(command.encode())


# ── WebSocket ──


@router.websocket("/ws/mt5")
async def mt5_websocket(ws: WebSocket):
    await ws.accept()
    ws_clients.add(ws)
    logger.info("MT5 WS client connected")
    try:
        while True:
            await ws.receive_text()
    except WebSocketDisconnect:
        pass
    finally:
        ws_clients.discard(ws)


# ── Low-Latency Binary Order WS ───────────────────────────────

INSTRUMENT_CACHE = {
    "XAUUSD": {"lot_size": 100, "tick_value": 0.1, "tick_size": 0.01, "min_lot": 0.01, "max_lot": 100},
    "XAGUSD": {"lot_size": 5000, "tick_value": 0.1, "tick_size": 0.001, "min_lot": 0.01, "max_lot": 100},
    "EURUSD": {"lot_size": 100000, "tick_value": 1.0, "tick_size": 0.00001, "min_lot": 0.01, "max_lot": 100},
    "BTCUSD": {"lot_size": 1, "tick_value": 1.0, "tick_size": 0.1, "min_lot": 0.001, "max_lot": 100},
    "SPY": {"lot_size": 100, "tick_value": 1.0, "tick_size": 0.01, "min_lot": 1, "max_lot": 10000},
}


@router.websocket("/ws/orders")
async def order_ws(ws: WebSocket):
    await ws.accept()
    logger.info("Low-latency order WS connected")
    try:
        while True:
            raw = await ws.receive_bytes()
            order = msgpack.unpackb(raw, raw=False)
            start = time.perf_counter()
            symbol = order.get("symbol", "")
            side = order.get("side", "BUY")
            volume = order.get("volume", 0.01)
            sl = order.get("sl", 0)
            tp = order.get("tp", 0)

            if symbol not in INSTRUMENT_CACHE:
                await ws.send_bytes(msgpack.packb({"error": "Unknown symbol"}))
                continue

            # Validate
            inst = INSTRUMENT_CACHE[symbol]
            vol = max(inst["min_lot"], min(inst["max_lot"], volume))

            # Execute via mocked MT5 or forward to bridge
            ticket = int(time.time() * 1000) % 1000000
            latency_ms = (time.perf_counter() - start) * 1000

            response = {
                "ticket": ticket,
                "symbol": symbol,
                "side": side,
                "volume": vol,
                "status": "FILLED",
                "fill_price": 0,
                "latency_ms": round(latency_ms, 2),
                "ts": time.time(),
            }
            await ws.send_bytes(msgpack.packb(response))

    except WebSocketDisconnect:
        pass
    except Exception as e:
        logger.error("Order WS error: %s", e)


def _broadcast_ws(data: dict):
    msg = json.dumps(data)
    dead = set()
    for ws in ws_clients:
        try:
            asyncio.create_task(_ws_send(ws, msg))
        except Exception:
            dead.add(ws)
    ws_clients.difference_update(dead)


async def _ws_send(ws: WebSocket, msg: str):
    try:
        await ws.send_text(msg)
    except Exception:
        ws_clients.discard(ws)


# ── TCP server ──


async def handle_ea_client(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
    peername = writer.get_extra_info("peername")
    logger.info("EA connected from %s", peername)
    ea_key = None
    buffer = b""
    try:
        while True:
            chunk = await reader.read(4096)
            if not chunk:
                logger.info("EA %s disconnected (connection closed)", ea_key or peername)
                break
            buffer += chunk
            if ea_key and ea_key in active_connections:
                active_connections[ea_key]["last_seen"] = _now()
            while b"\n" in buffer:
                line, buffer = buffer.split(b"\n", 1)
                raw = line.decode("utf-8", errors="replace").strip()
                if not raw:
                    continue
                msg = parse_ea_message(raw)
                if msg is None:
                    continue
                ea_key = await _handle_ea_message(msg, reader, writer, ea_key)
    except asyncio.CancelledError:
        pass
    except Exception as e:
        logger.error("EA handler error %s: %s", ea_key or peername, e)
    finally:
        if ea_key and ea_key in active_connections:
            active_connections.pop(ea_key, None)
            await _set_instance_status(ea_key, "disconnected")
            _broadcast_ws({"type": "ea_status", "ea_key": ea_key, "status": "disconnected"})
        try:
            writer.close()
            await writer.wait_closed()
        except Exception:
            pass


async def _handle_ea_message(msg: dict, reader, writer, ea_key: str | None) -> str | None:
    msg_type = msg["type"]
    if msg_type == "hello":
        ea_key = _make_ea_key(msg.get("ea_name", "Unknown"), msg.get("magic", 0))
        logger.info("EA hello: %s on %s %s", ea_key, msg.get("symbol"), msg.get("tf"))
        active_connections[ea_key] = {"reader": reader, "writer": writer, "last_seen": _now()}
        instance = await _upsert_instance(ea_key, msg)
        if instance:
            active_connections[ea_key]["instance_id"] = instance.id
            _broadcast_ws({"type": "ea_status", "ea_key": ea_key, "status": "running", "data": instance_to_dict(instance)})
        if ea_key in pending_commands:
            for cmd_bytes in pending_commands.pop(ea_key, []):
                writer.write(cmd_bytes)
                await writer.drain()
    elif msg_type == "heartbeat" and ea_key:
        if ea_key in active_connections:
            active_connections[ea_key]["last_seen"] = _now()
        await _update_instance_heartbeat(ea_key, msg)
        _broadcast_ws({"type": "ea_heartbeat", "ea_key": ea_key, "balance": msg.get("balance"),
                        "equity": msg.get("equity"), "status": msg.get("status")})
    elif msg_type == "trade" and ea_key:
        _broadcast_ws({"type": "ea_trade", "ea_key": ea_key, "data": msg})
    elif msg_type == "error" and ea_key:
        logger.error("EA error %s: %s", ea_key, msg.get("message"))
        _broadcast_ws({"type": "ea_error", "ea_key": ea_key, "error": msg.get("message")})
    elif msg_type == "goodbye" and ea_key:
        logger.info("EA goodbye: %s", ea_key)
    return ea_key


# ── Ping watchdog ──


async def _ping_watchdog():
    while True:
        await asyncio.sleep(5)
        now = _now()
        stale_keys = []
        for ea_key, conn in list(active_connections.items()):
            last = conn.get("last_seen", 0)
            if now - last > 10:
                try:
                    writer = conn["writer"]
                    writer.write(cmd_ping(ea_key).encode())
                    await writer.drain()
                    missed = _ping_counters.get(ea_key, 0) + 1
                    _ping_counters[ea_key] = missed
                    if missed >= 3:
                        logger.warning("EA %s unresponsive — disconnecting", ea_key)
                        stale_keys.append(ea_key)
                except Exception:
                    stale_keys.append(ea_key)
            else:
                _ping_counters[ea_key] = 0
        for key in stale_keys:
            conn = active_connections.pop(key, None)
            if conn:
                try:
                    conn["writer"].close()
                except Exception:
                    pass
                await _set_instance_status(key, "disconnected")
                _broadcast_ws({"type": "ea_status", "ea_key": key, "status": "disconnected"})


# ── TCP lifecycle ──


async def start_tcp_server():
    global _tcp_server, _watchdog_task
    try:
        _tcp_server = await asyncio.start_server(handle_ea_client, host=TCP_HOST, port=TCP_PORT)
        _watchdog_task = asyncio.create_task(_ping_watchdog())
        logger.info("MT5 TCP server started on %s:%s", TCP_HOST, TCP_PORT)
    except Exception as e:
        logger.error("Failed to start MT5 TCP server: %s", e)


async def stop_tcp_server():
    global _tcp_server, _watchdog_task
    if _watchdog_task:
        _watchdog_task.cancel()
        _watchdog_task = None
    if _tcp_server:
        _tcp_server.close()
        await _tcp_server.wait_closed()
        _tcp_server = None
        logger.info("MT5 TCP server stopped")


# ── REST endpoints ──


@router.get("/mt5/ea/list")
async def list_instances(user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    stmt = select(EAInstance).order_by(EAInstance.created_at.desc())
    result = await db.execute(stmt)
    instances = result.scalars().all()
    return {"success": True, "data": [instance_to_dict(i) for i in instances]}


@router.get("/mt5/ea/connections")
async def list_connections(user: User = Depends(resolve_user)):
    return {"success": True, "data": {k: {"connected": True, "instance_id": v.get("instance_id")}
                                       for k, v in active_connections.items()}}


@router.get("/mt5/ea/{eid}")
async def get_instance(eid: str, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    stmt = select(EAInstance).where(EAInstance.eid == eid)
    result = await db.execute(stmt)
    inst = result.scalar_one_or_none()
    if not inst:
        return {"success": False, "error": "Instance not found"}
    return {"success": True, "data": instance_to_dict(inst)}


@router.post("/mt5/ea/{eid}/params")
async def update_params(eid: str, body: dict, user: User = Depends(resolve_user),
                        db: AsyncSession = Depends(get_db)):
    stmt = select(EAInstance).where(EAInstance.eid == eid)
    result = await db.execute(stmt)
    inst = result.scalar_one_or_none()
    if not inst:
        return {"success": False, "error": "Instance not found"}
    new_params = body.get("params", {})
    inst.params_json = new_params
    await db.commit()
    ea_key = _make_ea_key(inst.ea_name, inst.magic_number)
    await _send_to_ea(ea_key, cmd_set_params(ea_key, new_params))
    _broadcast_ws({"type": "ea_params", "ea_key": ea_key, "params": new_params})
    return {"success": True, "data": {"params": new_params}}


@router.post("/mt5/ea/{eid}/close-all")
async def close_all_positions(eid: str, user: User = Depends(resolve_user),
                              db: AsyncSession = Depends(get_db)):
    stmt = select(EAInstance).where(EAInstance.eid == eid)
    result = await db.execute(stmt)
    inst = result.scalar_one_or_none()
    if not inst:
        return {"success": False, "error": "Instance not found"}
    ea_key = _make_ea_key(inst.ea_name, inst.magic_number)
    await _send_to_ea(ea_key, cmd_close_all(ea_key))
    return {"success": True, "message": "Close-all command sent"}


@router.post("/mt5/ea/{eid}/stop")
async def stop_ea(eid: str, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    stmt = select(EAInstance).where(EAInstance.eid == eid)
    result = await db.execute(stmt)
    inst = result.scalar_one_or_none()
    if not inst:
        return {"success": False, "error": "Instance not found"}
    ea_key = _make_ea_key(inst.ea_name, inst.magic_number)
    await _send_to_ea(ea_key, cmd_shutdown(ea_key))
    inst.status = "stopped"
    await db.commit()
    _broadcast_ws({"type": "ea_status", "ea_key": ea_key, "status": "stopped"})
    return {"success": True, "message": "Stop command sent"}


@router.get("/mt5/ea/{eid}/performance")
async def get_performance(eid: str, user: User = Depends(resolve_user),
                          db: AsyncSession = Depends(get_db)):
    stmt = select(EAInstance).where(EAInstance.eid == eid)
    result = await db.execute(stmt)
    inst = result.scalar_one_or_none()
    if not inst:
        return {"success": False, "error": "Instance not found"}
    return {"success": True, "data": {
        "total_trades": 0, "win_rate": 0.0,
        "pnl": inst.pnl, "balance": inst.balance, "equity": inst.equity,
    }}


@router.get("/mt5/ea/{eid}/trades")
async def get_trades(eid: str, user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    return {"success": True, "data": []}


@router.post("/mt5/ea/generate")
async def generate_ea(body: dict, user: User = Depends(resolve_user)):
    prompt = body.get("prompt", "")
    template_id = body.get("template_id")

    system = """You are a financial algorithm expert. Generate MQL5 code for MetaTrader 5.
Rules:
- Use #property strict
- Include input parameters with sensible defaults
- Use OnInit() for indicator handles, OnTick() for logic
- Include proper error handling
- Return ONLY the MQL5 code, no explanations"""

    if template_id:
        system += f"\n\nUse this template structure: {template_id}"

    try:
        import httpx
        from app.config import settings
        api_key = os.environ.get("LLM_PROVIDER_API_KEY", "")
        base_url = os.environ.get("LLM_PROVIDER_BASE_URL", settings.LLM_PROVIDER_BASE_URL)

        if not api_key:
            return {"success": True, "data": {"mql5_code": MQL5_GENERATED_PLACEHOLDER, "estimated_lines": 30, "warnings": ["No LLM_API_KEY set — using template. Set LLM_PROVIDER_API_KEY for AI generation."]}}

        async with httpx.AsyncClient(timeout=60) as client:
            resp = await client.post(
                f"{base_url}/chat/completions",
                headers={"Authorization": f"Bearer {api_key}", "Content-Type": "application/json"},
                json={"model": "gpt-4", "messages": [
                    {"role": "system", "content": system},
                    {"role": "user", "content": prompt}
                ], "temperature": 0.3}
            )
            if resp.status_code == 200:
                code = resp.json()["choices"][0]["message"]["content"]
                code = code.replace("```mql5", "").replace("```", "").strip()
                lines = len(code.split("\n"))
                return {"success": True, "data": {"mql5_code": code, "estimated_lines": lines, "warnings": []}}
            else:
                return {"success": False, "error": f"LLM API error: {resp.status_code}"}
    except Exception as e:
        return {"success": True, "data": {"mql5_code": MQL5_GENERATED_PLACEHOLDER, "estimated_lines": 30, "warnings": [f"LLM unavailable ({e}) — using template"]}}

MQL5_GENERATED_PLACEHOLDER = """//+------------------------------------------------------------------+
//|                                          Generated by FinceptAI  |
//+------------------------------------------------------------------+
#property strict
input double InpLotSize = 0.1;
int OnInit() { Print("EA initialized"); return INIT_SUCCEEDED; }
void OnDeinit(const int) {}
void OnTick() {}
//+------------------------------------------------------------------+"""


@router.post("/strategy/generate-both")
async def generate_both(body: dict, user: User = Depends(resolve_user)):
    """Generate MQL5 + Pine Script from one natural language prompt."""
    prompt = body.get("prompt", "")
    system = """You are a cross-platform strategy generator. For the given strategy description, output BOTH:
1. MQL5 code (for MetaTrader 5) between ```mql5 and ``` markers
2. Pine Script v5 code (for TradingView) between ```pine and ``` markers

Make both functionally identical: same logic, same parameters, same entry/exit rules."""

    try:
        import httpx
        from app.config import settings
        api_key = os.environ.get("LLM_PROVIDER_API_KEY", "")
        base_url = os.environ.get("LLM_PROVIDER_BASE_URL", settings.LLM_PROVIDER_BASE_URL)

        if not api_key:
            return {"success": False, "error": "Set LLM_PROVIDER_API_KEY in .env"}

        async with httpx.AsyncClient(timeout=60) as client:
            resp = await client.post(
                f"{base_url}/chat/completions",
                headers={"Authorization": f"Bearer {api_key}", "Content-Type": "application/json"},
                json={"model": "gpt-4", "messages": [
                    {"role": "system", "content": system},
                    {"role": "user", "content": prompt}
                ], "temperature": 0.3}
            )
            if resp.status_code == 200:
                content = resp.json()["choices"][0]["message"]["content"]
                mql5 = ""
                pine = ""
                if "```mql5" in content:
                    mql5 = content.split("```mql5")[1].split("```")[0].strip()
                if "```pine" in content:
                    pine = content.split("```pine")[1].split("```")[0].strip()
                return {"success": True, "data": {"mql5_code": mql5, "pine_script": pine, "full_response": content}}
            else:
                return {"success": False, "error": f"LLM error: {resp.status_code}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


@router.post("/mt5/ea/compile")
async def compile_ea(body: dict, user: User = Depends(resolve_user)):
    mql5_code = body.get("mql5_code", "")
    output_name = body.get("output_name")
    if not mql5_code:
        return {"success": False, "error": "mql5_code is required"}
    result = await compile_mql5(mql5_code, output_name)
    if result["success"]:
        return {"success": True, "data": result}
    errors = result.get("errors", ["Compilation failed"])
    return {"success": False, "error": errors[0] if errors else "Compilation failed", "data": result}


@router.post("/mt5/ea/deploy")
async def deploy_ea(body: dict, user: User = Depends(resolve_user)):
    ex5_path = body.get("ex5_path", "")
    target_name = body.get("target_name")
    if not ex5_path:
        return {"success": False, "error": "ex5_path is required"}
    result = await deploy_to_experts(ex5_path, target_name)
    if result["success"]:
        return {"success": True, "data": result}
    return {"success": False, "error": result.get("error", "Deployment failed"), "data": result}


@router.get("/mt5/templates")
async def list_templates(user: User = Depends(resolve_user), db: AsyncSession = Depends(get_db)):
    stmt = select(EATemplate).order_by(EATemplate.created_at.desc())
    result = await db.execute(stmt)
    templates = result.scalars().all()
    return {"success": True, "data": [{
        "tid": t.tid, "name": t.name, "description": t.description,
        "category": t.category, "created_at": t.created_at.isoformat(),
    } for t in templates]}


@router.post("/mt5/templates")
async def save_template(body: dict, user: User = Depends(resolve_user),
                        db: AsyncSession = Depends(get_db)):
    tmpl = EATemplate(
        name=body.get("name", "Untitled"),
        description=body.get("description", ""),
        category=body.get("category", "custom"),
        mql5_code=body.get("mql5_code", ""),
    )
    db.add(tmpl)
    await db.commit()
    await db.refresh(tmpl)
    return {"success": True, "data": {"tid": tmpl.tid, "name": tmpl.name}}


@router.delete("/mt5/templates/{tid}")
async def delete_template(tid: str, user: User = Depends(resolve_user),
                          db: AsyncSession = Depends(get_db)):
    stmt = select(EATemplate).where(EATemplate.tid == tid)
    result = await db.execute(stmt)
    tmpl = result.scalar_one_or_none()
    if not tmpl:
        return {"success": False, "error": "Template not found"}
    await db.delete(tmpl)
    await db.commit()
    return {"success": True, "message": "Template deleted"}


# ── Compiler Setup ───────────────────────────────────────────────

@router.get("/mt5/compiler-status")
async def compiler_status(user: User = Depends(resolve_user)):
    import os, platform, shutil
    from app.services.mt5_compiler import METAEDITOR_PATH, EXPERTS_DIR, DEV_MODE
    
    metaeditor = METAEDITOR_PATH or ""
    wine_available = shutil.which("wine") is not None
    metaeditor_exists = os.path.exists(metaeditor) if metaeditor else False
    
    return {"success": True, "data": {
        "dev_mode": DEV_MODE,
        "metaeditor_path": metaeditor,
        "metaeditor_exists": metaeditor_exists,
        "wine_available": wine_available,
        "experts_dir": EXPERTS_DIR or "",
        "platform": platform.system(),
        "can_compile": DEV_MODE or metaeditor_exists,
    }}


@router.post("/mt5/setup-compiler")
async def setup_mt5_compiler(user: User = Depends(resolve_user)):
    import os, platform, shutil
    from app.services.mt5_compiler import METAEDITOR_PATH, EXPERTS_DIR
    
    wine_available = shutil.which("wine") is not None
    metaeditor = METAEDITOR_PATH or ""
    
    if platform.system() != "Darwin":
        return {"success": False, "error": "Setup script is for macOS only. On Windows, install MT5 normally."}
    if not wine_available:
        return {"success": False, "error": "Wine not found. Install it: brew install --cask wine-stable"}
    if os.path.exists(metaeditor):
        return {"success": True, "data": {"message": "metaeditor64.exe already installed", "path": metaeditor}}
    
    script_path = os.path.join(os.path.dirname(__file__), "../../../scripts/setup_mt5_compiler.sh")
    script_path = os.path.abspath(script_path)
    if not os.path.exists(script_path):
        return {"success": False, "error": "Setup script not found"}
    
    try:
        proc = await asyncio.create_subprocess_exec(
            "bash", script_path,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE
        )
        stdout, stderr = await asyncio.wait_for(proc.communicate(), timeout=600)
        output = stdout.decode() if stdout else ""
        error_output = stderr.decode() if stderr else ""
        
        if proc.returncode == 0:
            return {"success": True, "data": {"output": output}}
        else:
            return {"success": False, "error": error_output[:500] if error_output else "Setup failed", "data": {"output": output}}
    except asyncio.TimeoutError:
        return {"success": False, "error": "Setup timed out (10 min). Install Wine + MT5 manually:\n  brew install --cask wine-stable\n  Then download MT5 from https://www.metatrader5.com/en/download"}
    except FileNotFoundError:
        return {"success": False, "error": "bash not found"}


# ── Order Management ─────────────────────────────────────────────
from fastapi import Body



@router.post("/mt5/order/market")
async def place_market(body: dict, user=Depends(resolve_user)):
    om = get_order_manager()
    side = OrderSide.BUY if body.get("side", "BUY").upper() == "BUY" else OrderSide.SELL
    order = await om.place_market(
        symbol=body.get("symbol", "XAUUSD"), side=side,
        volume=float(body.get("volume", 0.01)),
        sl=float(body.get("sl", 0)), tp=float(body.get("tp", 0)),
        ea_key=body.get("ea_key", ""))
    return {"success": True, "data": order.to_dict()}


@router.post("/mt5/order/limit")
async def place_limit(body: dict, user=Depends(resolve_user)):
    om = get_order_manager()
    side = OrderSide.BUY if body.get("side", "BUY").upper() == "BUY" else OrderSide.SELL
    order = await om.place_limit(
        symbol=body.get("symbol", "XAUUSD"), side=side,
        volume=float(body.get("volume", 0.01)),
        price=float(body.get("price", 0)),
        sl=float(body.get("sl", 0)), tp=float(body.get("tp", 0)),
        ea_key=body.get("ea_key", ""))
    return {"success": True, "data": order.to_dict()}


@router.post("/mt5/order/stop")
async def place_stop(body: dict, user=Depends(resolve_user)):
    om = get_order_manager()
    side = OrderSide.BUY if body.get("side", "BUY").upper() == "BUY" else OrderSide.SELL
    order = await om.place_stop(
        symbol=body.get("symbol", "XAUUSD"), side=side,
        volume=float(body.get("volume", 0.01)),
        stop_price=float(body.get("stop_price", 0)),
        limit_price=float(body.get("limit_price", 0)),
        sl=float(body.get("sl", 0)), tp=float(body.get("tp", 0)),
        ea_key=body.get("ea_key", ""))
    return {"success": True, "data": order.to_dict()}


@router.post("/mt5/order/bracket")
async def place_bracket(body: dict, user=Depends(resolve_user)):
    om = get_order_manager()
    side = OrderSide.BUY if body.get("side", "BUY").upper() == "BUY" else OrderSide.SELL
    order = await om.place_bracket(
        symbol=body.get("symbol", "XAUUSD"), side=side,
        volume=float(body.get("volume", 0.01)),
        entry_price=float(body.get("entry_price", 0)),
        sl=float(body.get("sl", 0)), tp=float(body.get("tp", 0)),
        ea_key=body.get("ea_key", ""))
    return {"success": True, "data": order.to_dict()}


@router.post("/mt5/order/oco")
async def place_oco(body: dict, user=Depends(resolve_user)):
    om = get_order_manager()
    side = OrderSide.BUY if body.get("side", "BUY").upper() == "BUY" else OrderSide.SELL
    stop, lmt = await om.place_oco(
        symbol=body.get("symbol", "XAUUSD"), side=side,
        volume=float(body.get("volume", 0.01)),
        stop_price=float(body.get("stop_price", 0)),
        limit_price=float(body.get("limit_price", 0)),
        ea_key=body.get("ea_key", ""))
    return {"success": True, "data": {"stop": stop.to_dict(), "limit": lmt.to_dict()}}


@router.post("/mt5/order/oto")
async def place_oto(body: dict, user=Depends(resolve_user)):
    om = get_order_manager()
    ts = OrderSide.BUY if body.get("trigger_side", "BUY").upper() == "BUY" else OrderSide.SELL
    tgs = OrderSide.BUY if body.get("target_side", "BUY").upper() == "BUY" else OrderSide.SELL
    trigger, target = await om.place_oto(
        symbol=body.get("symbol", "XAUUSD"),
        trigger_side=ts,
        trigger_price=float(body.get("trigger_price", 0)),
        trigger_direction=body.get("trigger_direction", "above"),
        target_side=tgs,
        target_volume=float(body.get("target_volume", 0.01)),
        target_price=float(body.get("target_price", 0)),
        sl=float(body.get("sl", 0)), tp=float(body.get("tp", 0)),
        ea_key=body.get("ea_key", ""))
    return {"success": True, "data": {"trigger": trigger.to_dict(), "target": target.to_dict()}}


@router.post("/mt5/order/iceberg")
async def place_iceberg(body: dict, user=Depends(resolve_user)):
    om = get_order_manager()
    side = OrderSide.BUY if body.get("side", "BUY").upper() == "BUY" else OrderSide.SELL
    order = await om.place_iceberg(
        symbol=body.get("symbol", "XAUUSD"), side=side,
        volume=float(body.get("volume", 0.01)),
        price=float(body.get("price", 0)),
        visible_volume=float(body.get("visible_volume", 0.01)),
        sl=float(body.get("sl", 0)), tp=float(body.get("tp", 0)),
        ea_key=body.get("ea_key", ""))
    return {"success": True, "data": order.to_dict()}


@router.post("/mt5/order/cancel")
async def cancel_order(body: dict = Body(...), user=Depends(resolve_user)):
    om = get_order_manager()
    ok = await om.cancel_order(body.get("order_id", ""))
    return {"success": ok, "message": "Cancelled" if ok else "Not found or already filled"}


@router.post("/mt5/order/modify")
async def modify_order(body: dict, user=Depends(resolve_user)):
    om = get_order_manager()
    ok = await om.modify_sltp(
        order_id=body.get("order_id", ""),
        sl=float(body.get("sl", 0)),
        tp=float(body.get("tp", 0)))
    return {"success": ok, "message": "Modified" if ok else "Not found"}


@router.post("/mt5/order/trailing-stop")
async def set_trailing_stop(body: dict = Body(...), user=Depends(resolve_user)):
    om = get_order_manager()
    ok = await om.set_trailing_stop(
        ticket=int(body.get("ticket", 0)),
        distance=float(body.get("distance", 50)))
    return {"success": ok, "message": "Trailing stop active" if ok else "Failed"}


@router.get("/mt5/orders")
async def list_orders(symbol: str = "", state: str = "",
                      user=Depends(resolve_user)):
    om = get_order_manager()
    orders = await om.get_orders(symbol=symbol)
    if state:
        orders = [o for o in orders if o.get("state", "").lower() == state.lower()]
    return {"success": True, "data": orders}


@router.get("/mt5/positions")
async def list_positions(user=Depends(resolve_user)):
    om = get_order_manager()
    positions = await om.get_positions()
    return {"success": True, "data": positions}


@router.get("/mt5/account")
async def account_info(user=Depends(resolve_user)):
    om = get_order_manager()
    summary = await om.get_account_summary()
    return {"success": True, "data": summary}

# ── Market data for MT5 Fleet Chart ──

@router.get("/mt5/market/orderbook")
async def get_orderbook(symbol: str = "XAUUSD", user=Depends(resolve_user)):
    """Return synthetic orderbook for DOM display."""
    import random
    base_price = {"XAUUSD": 2350, "XAGUSD": 31.5, "EURUSD": 1.12,
                  "GBPUSD": 1.33, "USDJPY": 155.0, "BTCUSD": 72000,
                  "ETHUSD": 3800, "AAPL": 180, "MSFT": 420, "TSLA": 250}.get(symbol.upper(), 100)
    spread = base_price * 0.0002
    best_bid = base_price - spread / 2
    best_ask = base_price + spread / 2
    bids, asks = [], []
    for i in range(15):
        bids.append({"price": round(best_bid - i * spread * 0.5, 2),
                     "volume": round(random.uniform(0.5, 10), 2)})
        asks.append({"price": round(best_ask + i * spread * 0.5, 2),
                     "volume": round(random.uniform(0.5, 10), 2)})
    return {"success": True, "symbol": symbol.upper(), "last_price": base_price,
            "spread": round(spread, 2), "bids": bids, "asks": asks}


@router.get("/mt5/market/ohlc")
async def get_ohlc(symbol: str = "XAUUSD", timeframe: str = "H1", count: int = 100,
                   user: User = Depends(resolve_user)):
    """Get OHLC price data for MT5 Fleet chart display."""
    try:
        import yfinance as yf
        # Map timeframe to yfinance interval
        tf_map = {"M1": "1m", "M5": "5m", "M15": "15m", "M30": "30m",
                  "H1": "1h", "H4": "4h", "D1": "1d", "W1": "1wk"}
        interval = tf_map.get(timeframe, "1h")

        # Map common symbols to yfinance tickers
        sym_map = {
            "XAUUSD": "GC=F", "XAGUSD": "SI=F", "XPTUSD": "PL=F",
            "WTI": "CL=F", "BRENT": "BZ=F",
            "EURUSD": "EURUSD=X", "GBPUSD": "GBPUSD=X", "USDJPY": "JPY=X",
            "AUDUSD": "AUDUSD=X", "USDCAD": "USDCAD=X", "NZDUSD": "NZDUSD=X",
            "USDCHF": "CHF=X", "GBPJPY": "GBPJPY=X", "EURJPY": "EURJPY=X",
            "BTCUSD": "BTC-USD", "ETHUSD": "ETH-USD",
        }
        ticker = sym_map.get(symbol.upper(), symbol)
        periods = {"1m": "1d", "5m": "5d", "15m": "5d", "30m": "1mo",
                   "1h": "1mo", "4h": "3mo", "1d": "1y", "1wk": "2y"}
        period = periods.get(interval, "1mo")

        t = yf.Ticker(ticker)
        df = t.history(period=period, interval=interval)

        if df.empty:
            # Fallback: generate synthetic data
            import math, random
            base = {"XAUUSD": 2350, "XAGUSD": 31.5, "EURUSD": 1.12,
                    "GBPUSD": 1.33, "USDJPY": 155.0, "BTCUSD": 72000}.get(symbol.upper(), 100)
            candles = []
            price = base
            for i in range(count):
                change = price * (random.uniform(-0.005, 0.005))
                o = price
                c = price + change
                h = max(o, c) + abs(change) * random.uniform(0, 0.5)
                l = min(o, c) - abs(change) * random.uniform(0, 0.5)
                candles.append({
                    "time": (int(datetime.now(timezone.utc).timestamp()) - (count - i) * 3600),
                    "open": round(o, 2), "high": round(h, 2),
                    "low": round(l, 2), "close": round(c, 2)
                })
                price = c
            return {"success": True, "data": candles, "source": "synthetic"}

        # Drop NaN values from OHLC columns
        df = df.fillna(0)

        candles = []
        for idx, row in df.iterrows():
            try:
                ts = int(idx.timestamp())
                open_val  = round(float(row["Open"]), 4)
                high_val  = round(float(row["High"]), 4)
                low_val   = round(float(row["Low"]), 4)
                close_val = round(float(row["Close"]), 4)
                vol_val   = int(float(row["Volume"]))
                candles.append({
                    "time": ts, "open": open_val, "high": high_val,
                    "low": low_val, "close": close_val, "volume": vol_val
                })
            except Exception as inner_e:
                logger.warning("Skipping candle at %s: %s", idx, inner_e)
                continue

        # Calculate EMAs
        closes = [c["close"] for c in candles]
        highs = [c["high"] for c in candles]
        lows = [c["low"] for c in candles]
        volumes = [c["volume"] for c in candles]
        opens_vals = [c["open"] for c in candles]

        # Moving Averages
        ema9 = _calc_ema(closes, 9); ema21 = _calc_ema(closes, 21)
        sma20 = _calc_sma(closes, 20); sma50 = _calc_sma(closes, 50)
        wma14 = _calc_wma(closes, 14)

        # Momentum
        rsi14 = _calc_rsi(closes, 14)
        macd_line, signal_line, macd_hist = _calc_macd(closes, 12, 26, 9)
        stoch_k, stoch_d = _calc_stochastic(highs, lows, closes)
        cci20 = _calc_cci(highs, lows, closes)
        williams_r = _calc_williams_r(highs, lows, closes)
        roc12 = _calc_roc(closes)
        mfi14 = _calc_mfi(highs, lows, closes, volumes)

        # Trend
        adx14 = _calc_adx(highs, lows, closes)
        st = _calc_supertrend(highs, lows, closes)
        aroon_up, aroon_down = _calc_aroon(highs, lows)
        
        # Volatility
        bb_upper, bb_mid, bb_lower = _calc_bollinger(closes, 20, 2)
        atr14 = _calc_atr(highs, lows, closes)
        kelt_u, kelt_m, kelt_l = _calc_keltner(highs, lows, closes)
        don_u, don_m, don_l = _calc_donchian(highs, lows)

        # Additional indicators from LEAN port
        hma9 = _calc_hma(closes, 9); hma21 = _calc_hma(closes, 21)
        tema9 = _calc_tema(closes, 9); tema21 = _calc_tema(closes, 21)
        dema9 = _calc_dema(closes, 9); dema21 = _calc_dema(closes, 21)
        wilder14 = _calc_wilder_ma(closes, 14)
        psar_vals = _calc_psar(highs, lows, closes)
        ha_open, ha_high, ha_low, ha_close = _calc_heikin_ashi(opens_vals, highs, lows, closes)
        ad_line = _calc_accumulation_distribution(highs, lows, closes, volumes)
        force_idx = _calc_force_index(closes, volumes)
        emv_vals = _calc_ease_of_movement(highs, lows, volumes)
        vwap14 = _calc_vwap(closes, volumes)

        # Volume
        obv = _calc_obv(closes, volumes)
        cmf20 = _calc_cmf(highs, lows, closes, volumes)

        for i, c in enumerate(candles):
            c["ema9"] = _v(ema9, i); c["ema21"] = _v(ema21, i)
            c["sma20"] = _v(sma20, i); c["sma50"] = _v(sma50, i)
            c["wma14"] = _v(wma14, i)
            c["hma9"] = _v(hma9, i); c["hma21"] = _v(hma21, i)
            c["tema9"] = _v(tema9, i); c["tema21"] = _v(tema21, i)
            c["dema9"] = _v(dema9, i); c["dema21"] = _v(dema21, i)
            c["wilder14"] = _v(wilder14, i)
            c["rsi14"] = _v(rsi14, i, 2)
            c["macd"] = _v(macd_line, i, 4)
            c["macd_signal"] = _v(signal_line, i, 4)
            c["macd_hist"] = _v(macd_hist, i, 6)
            c["stoch_k"] = _v(stoch_k, i, 2)
            c["stoch_d"] = _v(stoch_d, i, 2)
            c["cci"] = _v(cci20, i, 2)
            c["williams_r"] = _v(williams_r, i, 2)
            c["roc"] = _v(roc12, i, 4)
            c["mfi"] = _v(mfi14, i, 2)
            c["adx"] = _v(adx14, i, 2)
            c["psar"] = _v(psar_vals, i, 4)
            c["supertrend"] = _v(st, i)
            c["aroon_up"] = _v(aroon_up, i, 2)
            c["aroon_down"] = _v(aroon_down, i, 2)
            c["bb_upper"] = _v(bb_upper, i, 4)
            c["bb_mid"] = _v(bb_mid, i, 4)
            c["bb_lower"] = _v(bb_lower, i, 4)
            c["atr"] = _v(atr14, i, 4)
            c["kelt_upper"] = _v(kelt_u, i, 4)
            c["kelt_mid"] = _v(kelt_m, i, 4)
            c["kelt_lower"] = _v(kelt_l, i, 4)
            c["don_upper"] = _v(don_u, i, 4)
            c["don_mid"] = _v(don_m, i, 4)
            c["don_lower"] = _v(don_l, i, 4)
            c["ha_open"] = _v(ha_open, i, 4)
            c["ha_high"] = _v(ha_high, i, 4)
            c["ha_low"] = _v(ha_low, i, 4)
            c["ha_close"] = _v(ha_close, i, 4)
            c["ad_line"] = _v(ad_line, i, 0)
            c["force_idx"] = _v(force_idx, i, 4)
            c["emv"] = _v(emv_vals, i, 4)
            c["vwap"] = _v(vwap14, i, 4)
            c["obv"] = _v(obv, i, 0)
            c["cmf"] = _v(cmf20, i, 4)

        return {"success": True, "data": candles[-count:], "source": "yfinance"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def _calc_sma(prices: list, period: int) -> list:
    sma = []
    for i in range(len(prices)):
        if i < period - 1:
            sma.append(None)
        else:
            sma.append(sum(prices[i - period + 1:i + 1]) / period)
    return sma


def _v(arr, idx, dec=4):
    """Safe value extraction with rounding."""
    if idx < len(arr) and arr[idx] is not None:
        try:
            return round(float(arr[idx]), dec)
        except (ValueError, TypeError):
            return arr[idx]
    return None


def _calc_wma(prices: list, period: int) -> list:
    wma = []
    for i in range(len(prices)):
        if i < period - 1:
            wma.append(None)
        else:
            weight_sum = sum((j + 1) * prices[i - period + 1 + j] for j in range(period))
            wma.append(weight_sum / (period * (period + 1) / 2))
    return wma


def _calc_ema(prices: list, period: int) -> list:
    multiplier = 2 / (period + 1)
    ema = []
    sma = sum(prices[:period]) / period if len(prices) >= period else prices[0]
    for i, p in enumerate(prices):
        if i < period - 1:
            ema.append(None)
        elif i == period - 1:
            ema.append(sma)
        else:
            val = (p - ema[-1]) * multiplier + ema[-1]
            ema.append(val)
    return ema


def _calc_rsi(prices: list, period: int = 14) -> list:
    rsi = [None] * len(prices)
    if len(prices) <= period:
        return rsi
    gains, losses = [], []
    for i in range(1, period + 1):
        diff = prices[i] - prices[i - 1]
        gains.append(max(diff, 0))
        losses.append(max(-diff, 0))
    avg_gain = sum(gains) / period
    avg_loss = sum(losses) / period
    for i in range(period, len(prices)):
        if i > period:
            diff = prices[i] - prices[i - 1]
            avg_gain = (avg_gain * (period - 1) + max(diff, 0)) / period
            avg_loss = (avg_loss * (period - 1) + max(-diff, 0)) / period
        rs = avg_gain / avg_loss if avg_loss != 0 else 100
        rsi[i] = 100 - (100 / (1 + rs))
    return rsi


def _calc_macd(prices: list, fast: int = 12, slow: int = 26, signal: int = 9) -> tuple:
    ema_fast = _calc_ema(prices, fast)
    ema_slow = _calc_ema(prices, slow)
    macd_line = [(ema_fast[i] - ema_slow[i]) if ema_fast[i] is not None and ema_slow[i] is not None else None for i in range(len(prices))]
    signal_line = _calc_ema([m for m in macd_line if m is not None], signal)
    sig_idx = 0
    macd_hist = []
    for i in range(len(prices)):
        if macd_line[i] is not None and sig_idx < len(signal_line) and signal_line[sig_idx] is not None:
            macd_hist.append(macd_line[i] - signal_line[sig_idx])
            sig_idx += 1
        else:
            macd_hist.append(None)
    # Pad signal_line to full length
    full_signal = [None] * len(prices)
    sig_idx = 0
    for i in range(len(prices)):
        if macd_line[i] is not None and sig_idx < len(signal_line):
            full_signal[i] = signal_line[sig_idx]
            sig_idx += 1
    return macd_line, full_signal, macd_hist


def _calc_bollinger(prices: list, period: int = 20, std_dev: float = 2.0) -> tuple:
    upper, mid, lower = [], [], []
    for i in range(len(prices)):
        if i < period - 1:
            upper.append(None); mid.append(None); lower.append(None)
        else:
            window = prices[i - period + 1:i + 1]
            sma = sum(window) / period
            variance = sum((x - sma) ** 2 for x in window) / period
            std = variance ** 0.5
            mid.append(sma)
            upper.append(sma + std_dev * std)
            lower.append(sma - std_dev * std)
    return upper, mid, lower


def _calc_stochastic(highs, lows, closes, k_period=14, d_period=3) -> tuple:
    k, d = [], []
    for i in range(len(closes)):
        if i < k_period - 1:
            k.append(None); d.append(None)
        else:
            hh = max(highs[i - k_period + 1:i + 1])
            ll = min(lows[i - k_period + 1:i + 1])
            k_val = ((closes[i] - ll) / (hh - ll) * 100) if (hh - ll) != 0 else 50
            k.append(k_val)
    d = _calc_sma([x for x in k if x is not None], d_period)
    d_full = [None] * len(k)
    di = 0
    for i in range(len(k)):
        if k[i] is not None and di < len(d):
            d_full[i] = d[di]
            di += 1
    return k, d_full


def _calc_cci(highs, lows, closes, period=20) -> list:
    cci = []
    for i in range(len(closes)):
        if i < period - 1:
            cci.append(None)
        else:
            tp = (highs[i] + lows[i] + closes[i]) / 3
            tp_avg = sum((highs[j] + lows[j] + closes[j]) / 3 for j in range(i - period + 1, i + 1)) / period
            mad = sum(abs((highs[j] + lows[j] + closes[j]) / 3 - tp_avg) for j in range(i - period + 1, i + 1)) / period
            cci.append((tp - tp_avg) / (0.015 * mad) if mad != 0 else 0)
    return cci


def _calc_williams_r(highs, lows, closes, period=14) -> list:
    wr = []
    for i in range(len(closes)):
        if i < period - 1:
            wr.append(None)
        else:
            hh = max(highs[i - period + 1:i + 1])
            ll = min(lows[i - period + 1:i + 1])
            wr.append(((hh - closes[i]) / (hh - ll)) * -100 if (hh - ll) != 0 else -50)
    return wr


def _calc_mfi(highs, lows, closes, volumes, period=14) -> list:
    mfi = []
    typical = [(highs[i] + lows[i] + closes[i]) / 3 for i in range(len(closes))]
    for i in range(len(closes)):
        if i < period:
            mfi.append(None)
        else:
            pos_flow, neg_flow = 0, 0
            for j in range(i - period + 1, i + 1):
                if j > 0 and typical[j] > typical[j - 1]:
                    pos_flow += typical[j] * volumes[j]
                elif j > 0:
                    neg_flow += typical[j] * volumes[j]
            mfi.append(100 - (100 / (1 + pos_flow / neg_flow))) if neg_flow != 0 else mfi.append(100)
    return mfi


def _calc_roc(closes, period=12) -> list:
    roc = []
    for i in range(len(closes)):
        if i < period:
            roc.append(None)
        else:
            roc.append((closes[i] - closes[i - period]) / closes[i - period] * 100)
    return roc


def _calc_adx(highs, lows, closes, period=14) -> list:
    atr = _calc_atr(highs, lows, closes, period)
    adx = []
    for i in range(len(closes)):
        if i < period * 2:
            adx.append(None)
        else:
            dx_sum = 0
            for j in range(i - period + 1, i + 1):
                up = highs[j] - highs[j - 1]
                down = lows[j - 1] - lows[j]
                pdi = (up / atr[j] * 100) if up > down and up > 0 else 0
                ndi = (down / atr[j] * 100) if down > up and down > 0 else 0
                dx_sum += abs(pdi - ndi) / (pdi + ndi) * 100 if (pdi + ndi) > 0 else 0
            adx.append(dx_sum / period)
    return adx


def _calc_atr(highs, lows, closes, period=14) -> list:
    atr, tr = [], []
    for i in range(len(closes)):
        if i == 0:
            tr.append(highs[i] - lows[i])
        else:
            tr.append(max(highs[i] - lows[i], abs(highs[i] - closes[i - 1]), abs(lows[i] - closes[i - 1])))
    return _calc_ema(tr, period)


def _calc_keltner(highs, lows, closes, period=20, multiplier=2.0) -> tuple:
    ema = _calc_ema(closes, period)
    atr = _calc_atr(highs, lows, closes, period)
    upper, mid, lower = [], [], []
    for i in range(len(closes)):
        if ema[i] is None or atr[i] is None:
            upper.append(None); mid.append(None); lower.append(None)
        else:
            mid.append(ema[i])
            upper.append(ema[i] + multiplier * atr[i])
            lower.append(ema[i] - multiplier * atr[i])
    return upper, mid, lower


def _calc_donchian(highs, lows, period=20) -> tuple:
    upper, mid, lower = [], [], []
    for i in range(len(highs)):
        if i < period - 1:
            upper.append(None); mid.append(None); lower.append(None)
        else:
            hh = max(highs[i - period + 1:i + 1])
            ll = min(lows[i - period + 1:i + 1])
            upper.append(hh); lower.append(ll); mid.append((hh + ll) / 2)
    return upper, mid, lower


def _calc_obv(closes, volumes) -> list:
    obv, val = [], 0
    for i in range(len(closes)):
        if i == 0:
            val = volumes[i]
        elif closes[i] > closes[i - 1]:
            val += volumes[i]
        elif closes[i] < closes[i - 1]:
            val -= volumes[i]
        obv.append(val)
    return obv


def _calc_cmf(highs, lows, closes, volumes, period=20) -> list:
    cmf = []
    for i in range(len(closes)):
        if i < period - 1:
            cmf.append(None)
        else:
            mfv_sum, vol_sum = 0, 0
            for j in range(i - period + 1, i + 1):
                mf = ((closes[j] - lows[j]) - (highs[j] - closes[j])) / (highs[j] - lows[j]) if (highs[j] - lows[j]) != 0 else 0
                mfv_sum += mf * volumes[j]
                vol_sum += volumes[j]
            cmf.append(mfv_sum / vol_sum if vol_sum != 0 else 0)
    return cmf


def _calc_aroon(highs, lows, period=25) -> tuple:
    aroon_up, aroon_down = [], []
    for i in range(len(highs)):
        if i < period:
            aroon_up.append(None); aroon_down.append(None)
        else:
            window_high = highs[i - period:i + 1]
            window_low = lows[i - period:i + 1]
            up = ((period - window_high.index(max(window_high))) / period) * 100
            down = ((period - window_low.index(min(window_low))) / period) * 100
            aroon_up.append(up); aroon_down.append(down)
    return aroon_up, aroon_down


def _calc_supertrend(highs, lows, closes, period=10, multiplier=3.0) -> list:
    atr = _calc_atr(highs, lows, closes, period)
    st = []
    for i in range(len(closes)):
        if i < period - 1:
            st.append(None)
        else:
            hl_avg = (highs[i] + lows[i]) / 2
            upper = hl_avg + multiplier * atr[i]
            lower = hl_avg - multiplier * atr[i]
            st.append(1 if closes[i] > upper else -1)
    return st


# ── LEAN-ported indicators ─────────────────────────────────────────


def _calc_hma(prices: list, period: int) -> list:
    """Hull Moving Average — smooth with low lag."""
    n = len(prices)
    half = max(1, int(period // 2))
    sqrt_n = max(1, int(period ** 0.5))
    wma_half = _calc_lwma(prices, half)
    wma_full = _calc_lwma(prices, period)
    diff = [None] * n
    for i in range(n):
        if wma_half[i] is not None and wma_full[i] is not None:
            diff[i] = 2 * wma_half[i] - wma_full[i]
    hma = _calc_lwma_raw(diff, sqrt_n)
    return hma


def _calc_lwma(prices: list, period: int) -> list:
    """Linear Weighted Moving Average."""
    result = []
    denom = period * (period + 1) // 2
    for i in range(len(prices)):
        if i < period - 1:
            result.append(None)
        else:
            num = sum((period - j) * prices[i - j] for j in range(period))
            result.append(num / denom)
    return result


def _calc_lwma_raw(values: list, period: int) -> list:
    """LWMA on already-computed values (may contain None)."""
    result = []
    denom = period * (period + 1) // 2
    for i in range(len(values)):
        if i < period - 1 or any(values[i - period + 1 + j] is None for j in range(period)):
            result.append(None)
        else:
            num = sum((period - j) * values[i - j] for j in range(period))
            result.append(num / denom)
    return result


def _calc_tema(prices: list, period: int) -> list:
    """Triple Exponential Moving Average: TEMA = 3*EMA1 - 3*EMA2 + EMA3."""
    ema1 = _calc_ema(prices, period)
    ema2 = _calc_ema([v if v is not None else prices[i] for i, v in enumerate(ema1)], period)
    ema3 = _calc_ema([v if v is not None else prices[i] for i, v in enumerate(ema2)], period)
    tema = []
    for i in range(len(prices)):
        if ema1[i] is not None and ema2[i] is not None and ema3[i] is not None:
            tema.append(3 * ema1[i] - 3 * ema2[i] + ema3[i])
        else:
            tema.append(None)
    return tema


def _calc_dema(prices: list, period: int) -> list:
    """Double Exponential Moving Average: DEMA = 2*EMA1 - EMA2."""
    ema1 = _calc_ema(prices, period)
    ema2 = _calc_ema([v if v is not None else prices[i] for i, v in enumerate(ema1)], period)
    dema = []
    for i in range(len(prices)):
        if ema1[i] is not None and ema2[i] is not None:
            dema.append(2 * ema1[i] - ema2[i])
        else:
            dema.append(None)
    return dema


def _calc_wilder_ma(prices: list, period: int) -> list:
    """Wilder's Moving Average (alpha = 1/period, used in RSI/ADX)."""
    result = []
    for i in range(len(prices)):
        if i == 0:
            result.append(prices[i])
        elif i < period:
            sma = sum(prices[:i + 1]) / (i + 1)
            result.append(sma)
        else:
            val = result[-1] + (prices[i] - result[-1]) / period
            result.append(val)
    return result


def _calc_psar(highs, lows, closes, af_start=0.02, af_increment=0.02, af_max=0.2):
    """Parabolic SAR."""
    n = len(highs)
    if n == 0:
        return []
    psar = [None] * n
    sar = highs[0]
    af = af_start
    trend = 1  # 1=up, -1=down
    ep = lows[0]
    for i in range(1, n):
        if trend == 1:  # Up trend
            sar = sar + af * (ep - sar)
            if highs[i] > ep:
                ep = highs[i]
                af = min(af + af_increment, af_max)
            if sar > lows[i]:
                trend = -1
                sar = ep
                ep = lows[i]
                af = af_start
        else:  # Down trend
            sar = sar + af * (ep - sar)
            if lows[i] < ep:
                ep = lows[i]
                af = min(af + af_increment, af_max)
            if sar < highs[i]:
                trend = 1
                sar = ep
                ep = highs[i]
                af = af_start
        psar[i] = round(sar, 4)
    return psar


def _calc_heikin_ashi(opens, highs, lows, closes):
    """Heikin-Ashi candlestick OHLC."""
    n = len(opens)
    ha_o, ha_h, ha_l, ha_c = [], [], [], []
    prev_ha_o = (opens[0] + closes[0]) / 2
    for i in range(n):
        if i == 0:
            cur_ha_c = (opens[i] + highs[i] + lows[i] + closes[i]) / 4
            cur_ha_o = (opens[i] + closes[i]) / 2
            cur_ha_h = highs[i]
            cur_ha_l = lows[i]
        else:
            cur_ha_c = (opens[i] + highs[i] + lows[i] + closes[i]) / 4
            cur_ha_o = (prev_ha_o + cur_ha_c) / 2
            cur_ha_h = max(highs[i], cur_ha_o, cur_ha_c)
            cur_ha_l = min(lows[i], cur_ha_o, cur_ha_c)
        ha_o.append(round(cur_ha_o, 4))
        ha_h.append(round(cur_ha_h, 4))
        ha_l.append(round(cur_ha_l, 4))
        ha_c.append(round(cur_ha_c, 4))
        prev_ha_o = cur_ha_o
    return ha_o, ha_h, ha_l, ha_c


def _calc_accumulation_distribution(highs, lows, closes, volumes):
    """Accumulation/Distribution Line."""
    ad, val = [], 0
    for i in range(len(closes)):
        if i == 0:
            val = 0
        else:
            hl = highs[i] - lows[i]
            if hl != 0:
                mfm = ((closes[i] - lows[i]) - (highs[i] - closes[i])) / hl
                val += mfm * volumes[i]
        ad.append(int(val))
    return ad


def _calc_force_index(closes, volumes, period=13):
    """Force Index = EMA(Volume * (Close - Prev Close), period)."""
    fi_raw = [0]
    for i in range(1, len(closes)):
        fi_raw.append((closes[i] - closes[i - 1]) * volumes[i])
    fi_smoothed = _calc_ema(fi_raw, period)
    return fi_smoothed


def _calc_ease_of_movement(highs, lows, volumes, period=14, scale=10000):
    """Ease of Movement Value (EMV)."""
    n = len(highs)
    emv = []
    for i in range(n):
        if i == 0 or volumes[i] == 0 or (highs[i] - lows[i]) == 0:
            emv.append(0.0)
        else:
            mid = ((highs[i] + lows[i]) / 2) - ((highs[i - 1] + lows[i - 1]) / 2)
            ratio = (volumes[i] / scale) / (highs[i] - lows[i])
            emv.append(mid / ratio if ratio != 0 else 0.0)
    smoothed = _calc_sma(emv, period)
    return smoothed


def _calc_vwap(closes, volumes, period=14):
    """Volume Weighted Average Price over a rolling window."""
    vwap = []
    for i in range(len(closes)):
        if i < period - 1:
            vwap.append(None)
        else:
            tp_sum = sum(closes[i - period + 1 + j] * volumes[i - period + 1 + j] for j in range(period))
            vol_sum = sum(volumes[i - period + 1 + j] for j in range(period))
            vwap.append(tp_sum / vol_sum if vol_sum != 0 else closes[i])
    return vwap


# ── Price Alert API (in-memory store) ────────────────────────────────

import uuid as _uuid

_alerts_store: dict = {}
_alerts_lock = asyncio.Lock()


@router.get("/mt5/alerts")
async def get_alerts(user=Depends(resolve_user)):
    async with _alerts_lock:
        alerts = list(_alerts_store.values())
        return {"success": True, "data": alerts}


@router.post("/mt5/alert/save")
async def save_alert(body: dict, user=Depends(resolve_user)):
    alert_id = body.get("id", "")
    async with _alerts_lock:
        if alert_id and alert_id in _alerts_store:
            existing = _alerts_store[alert_id]
            existing.update({k: v for k, v in body.items() if v is not None and k != "id"})
        else:
            new_id = str(_uuid.uuid4())[:8]
            alert = {
                "id": new_id,
                "symbol": body.get("symbol", ""),
                "alert_type": body.get("alert_type", "above"),
                "trigger_price": body.get("trigger_price", 0),
                "range_high": body.get("range_high", 0),
                "channel": body.get("channel", "push"),
                "note": body.get("note", ""),
                "enabled": body.get("enabled", True),
                "triggered_count": 0,
                "is_triggered": False,
            }
            _alerts_store[new_id] = alert
        return {"success": True, "data": list(_alerts_store.values())}


@router.post("/mt5/alert/toggle")
async def toggle_alert(body: dict, user=Depends(resolve_user)):
    alert_id = body.get("id", "")
    async with _alerts_lock:
        if alert_id in _alerts_store:
            _alerts_store[alert_id]["enabled"] = body.get("enabled", not _alerts_store[alert_id].get("enabled", True))
            return {"success": True}
    return {"success": False, "error": "Alert not found"}


@router.post("/mt5/alert/delete")
async def delete_alert(body: dict, user=Depends(resolve_user)):
    alert_id = body.get("id", "")
    async with _alerts_lock:
        if alert_id in _alerts_store:
            del _alerts_store[alert_id]
            return {"success": True}
    return {"success": False, "error": "Alert not found"}
