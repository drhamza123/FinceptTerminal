import asyncio
import json
import logging

import httpx
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager

from app.config import settings
from app.database import init_db
from app.routers import auth, health, llm, news, macro, market, billing, chat, forum, support, marine, quantlib, agent, news_ws, geopolitics, mt5_bridge, backtest, freelance, vps, community

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("guardian")


@asynccontextmanager
async def lifespan(app: FastAPI):
    logger.info("Starting Guardian API backend...")
    await init_db()
    from app.routers.news_ws import start_poller, stop_poller
    from app.routers.mt5_bridge import start_tcp_server, stop_tcp_server
    start_poller(app)
    await start_tcp_server()
    from app.services.mt5_zmq_worker import MT5ZMQBridge
    zmq_bridge = MT5ZMQBridge()
    await zmq_bridge.start()
    app.state.zmq_bridge = zmq_bridge
    logger.info("Database initialized and MT5 TCP server + ZMQ bridge started.")
    yield
    stop_poller()
    await stop_tcp_server()
    if hasattr(app.state, "zmq_bridge"):
        await app.state.zmq_bridge.stop()
    logger.info("Shutting down.")


app = FastAPI(
    title=settings.APP_NAME,
    version=settings.VERSION,
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.CORS_ORIGINS,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(auth.router)
app.include_router(health.router)
app.include_router(llm.router)
app.include_router(news.router)
app.include_router(macro.router)
app.include_router(market.router)
app.include_router(billing.router)
app.include_router(chat.router)
app.include_router(forum.router)
app.include_router(support.router)
app.include_router(marine.router)
app.include_router(quantlib.router)
app.include_router(agent.router)
app.include_router(news_ws.router)
app.include_router(geopolitics.router)
app.include_router(mt5_bridge.router)
app.include_router(backtest.router)
app.include_router(freelance.router)
app.include_router(vps.router)
app.include_router(community.router)


@app.get("/")
async def root():
    return {"success": True, "message": "Guardian API is running", "data": {"version": settings.VERSION}}
