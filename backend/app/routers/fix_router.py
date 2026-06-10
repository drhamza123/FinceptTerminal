import logging
import socket
import json
import uuid
import threading
from datetime import datetime, timezone

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel
from sqlalchemy import create_engine, Column, String, Integer, Text, DateTime, JSON
from sqlalchemy.orm import declarative_base, sessionmaker

from app.config import settings

logger = logging.getLogger("guardian.fix")

router = APIRouter(prefix="/fix", tags=["fix"])

FIX_BRIDGE_HOST = "127.0.0.1"
FIX_BRIDGE_PORT = 5560

engine = create_engine(settings.DATABASE_URL.replace("+aiosqlite", "").replace("+asyncpg", ""))
Base = declarative_base()
Session = sessionmaker(bind=engine)


class FIXBrokerConfig(Base):
    __tablename__ = "fix_broker_configs"
    id = Column(String, primary_key=True, default=lambda: str(uuid.uuid4()))
    name = Column(String, unique=True, nullable=False)
    host = Column(String, nullable=False)
    port = Column(Integer, nullable=False)
    sender_comp = Column(String, nullable=False)
    target_comp = Column(String, nullable=False)
    username = Column(String, default="")
    password = Column(String, default="")
    status = Column(String, default="disconnected")
    created_at = Column(DateTime, default=lambda: datetime.now(timezone.utc))
    updated_at = Column(DateTime, default=lambda: datetime.now(timezone.utc), onupdate=lambda: datetime.now(timezone.utc))


Base.metadata.create_all(bind=engine, tables=[FIXBrokerConfig.__table__], checkfirst=True)


def _send_fix_command(cmd: str) -> str:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(10)
        s.connect((FIX_BRIDGE_HOST, FIX_BRIDGE_PORT))
        s.sendall((cmd + "\n").encode())
        resp = s.recv(8192).decode().strip()
        s.close()
        return resp
    except Exception as e:
        logger.error(f"FIX bridge command failed: {e}")
        raise HTTPException(status_code=502, detail=f"FIX bridge unreachable: {e}")


@router.post("/configure")
def fix_configure(body: dict):
    name = body.get("name", "").strip()
    host = body.get("host", "").strip()
    port = body.get("port", 0)
    sender = body.get("sender_comp", "").strip()
    target = body.get("target_comp", "").strip()

    if not all([name, host, port, sender, target]):
        raise HTTPException(status_code=400, detail="name, host, port, sender_comp, target_comp required")

    cmd = f"CONNECT|{name}|{host}|{port}|{sender}|{target}"
    resp = _send_fix_command(cmd)

    try:
        result = json.loads(resp)
    except json.JSONDecodeError:
        result = {"raw": resp}

    db = Session()
    try:
        existing = db.query(FIXBrokerConfig).filter_by(name=name).first()
        if existing:
            existing.host = host
            existing.port = port
            existing.sender_comp = sender
            existing.target_comp = target
            existing.status = "connected" if result.get("status") == "connected" else "failed"
        else:
            cfg = FIXBrokerConfig(
                name=name, host=host, port=port,
                sender_comp=sender, target_comp=target,
                status="connected" if result.get("status") == "connected" else "failed"
            )
            db.add(cfg)
        db.commit()
    finally:
        db.close()

    return {"success": result.get("status") == "connected", "message": resp, "data": result}


@router.post("/order")
def fix_order(body: dict):
    session_name = body.get("session", "").strip()
    symbol = body.get("symbol", "").strip()
    side = body.get("side", "BUY").strip().upper()
    qty = body.get("quantity", 0)
    order_type = body.get("order_type", "MARKET").strip().upper()
    price = body.get("price", 0)
    stop = body.get("stop_price", 0)

    if not all([session_name, symbol, qty]):
        raise HTTPException(status_code=400, detail="session, symbol, quantity required")

    cmd = f"ORDER|{session_name}|{symbol}|{side}|{qty}|{order_type}|{price}|{stop}"
    resp = _send_fix_command(cmd)

    try:
        result = json.loads(resp)
    except json.JSONDecodeError:
        result = {"raw": resp}

    return {"success": result.get("status") == "filled", "message": resp, "data": result}


@router.get("/status")
def fix_status():
    resp = _send_fix_command("STATUS")
    try:
        result = json.loads(resp)
    except json.JSONDecodeError:
        result = {"raw": resp}

    db = Session()
    try:
        brokers = db.query(FIXBrokerConfig).all()
    finally:
        db.close()

    return {
        "success": True,
        "data": {
            "bridge": result,
            "configured_brokers": [
                {
                    "name": b.name,
                    "host": b.host,
                    "port": b.port,
                    "sender_comp": b.sender_comp,
                    "target_comp": b.target_comp,
                    "status": b.status,
                }
                for b in brokers
            ]
        }
    }


@router.delete("/disconnect/{name}")
def fix_disconnect(name: str):
    db = Session()
    try:
        broker = db.query(FIXBrokerConfig).filter_by(name=name).first()
        if broker:
            broker.status = "disconnected"
            db.commit()
            return {"success": True, "message": f"Broker '{name}' marked disconnected"}
        return {"success": False, "message": f"Broker '{name}' not found"}
    finally:
        db.close()


# ── FIX Mock Simulator ──────────────────────────────────────────

@router.post("/simulator/start")
def start_simulator():
    try:
        from app.services.fix_simulator import FixSimulator
        t = threading.Thread(target=lambda: FixSimulator().start(), daemon=True)
        t.start()
        return {"success": True, "message": "FIX simulator starting on port 9878"}
    except Exception as e:
        return {"success": False, "message": str(e), "error_type": type(e).__name__}


@router.post("/simulator/connect")
def connect_simulator():
    try:
        from app.services.fix_client import fix_client
        ok = fix_client.connect("127.0.0.1", 9878)
        return {"success": ok, "message": "connected" if ok else "connection failed"}
    except Exception as e:
        return {"success": False, "message": str(e), "error_type": type(e).__name__}


class FixOrderRequest(BaseModel):
    symbol: str
    side: str
    qty: float


@router.post("/order/send")
def send_fix_order(order: FixOrderRequest):
    from app.services.fix_client import fix_client
    if not fix_client.connected:
        raise HTTPException(status_code=503, detail="FIX client not connected. POST /fix/simulator/connect first")
    try:
        result = fix_client.send_market_order(order.symbol, order.side, order.qty)
        return {"success": True, "data": result}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/client/status")
def fix_client_status():
    from app.services.fix_client import fix_client
    return {"success": True, "data": {"connected": fix_client.connected}}
