import logging
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager

from app.config import settings

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("guardian")

from app.database import init_db

# Initialize database synchronously for sync WSGI server
import asyncio
try:
    asyncio.run(init_db())
    logger.info("Database initialized")
except Exception as e:
    logger.warning(f"Database init failed: {e}")

from app.routers import auth, health, llm, news, macro, market, billing, chat, forum, support, marine, quantlib, agent, news_ws, geopolitics, mt5_bridge, backtest, freelance, vps, community, execution, alert_router, fix_router
from app.routers.trade_server_router import router as trade_server_router
from app.routers.worker_router import router as worker_router
from app.services.alert_engine import alert_engine
from app.services.mt5_trade_bridge import price_bridge

app = FastAPI(
    title=settings.APP_NAME,
    version=settings.VERSION,
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
app.include_router(execution.router)
app.include_router(alert_router.router)
app.include_router(trade_server_router)
app.include_router(fix_router.router)
app.include_router(worker_router)

# Start alert engine on import
try:
    import asyncio
    loop = asyncio.new_event_loop()
    loop.create_task(alert_engine.start())
    logger.info("Alert engine running")
except Exception as e:
    logger.warning(f"Alert engine failed: {e}")

# Auto-enable MT5 AutoTrading via Win32 API (only on Windows, in session 0)
try:
    import sys, time, ctypes, re, subprocess as sp
    if sys.platform == "win32":
        logger.info("[AutoTrading] Attempting to enable...")
        try:
            output = sp.check_output(
                'tasklist /NH /FO CSV /FI "IMAGENAME eq terminal64.exe"', 
                shell=True, timeout=5
            ).decode()
            pids = set()
            for line in output.split('\n'):
                if 'terminal64' in line.lower():
                    m = re.search(r'"(\d+)"', line)
                    if m: pids.add(int(m.group(1)))
            
            if not pids:
                logger.warning("[AutoTrading] No MT5 process found")
            else:
                kernel32 = ctypes.windll.kernel32
                TH32CS_SNAPTHREAD = 0x00000004
                
                class TEntry(ctypes.Structure):
                    _fields_ = [("size", ctypes.c_uint32), ("usage", ctypes.c_uint32),
                                ("tid", ctypes.c_uint32), ("pid", ctypes.c_uint32),
                                ("prio", ctypes.c_long), ("dprio", ctypes.c_long),
                                ("flags", ctypes.c_uint32)]
                
                for pid in pids:
                    snap = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
                    if snap <= 0: continue
                    te = TEntry(); te.size = ctypes.sizeof(TEntry)
                    found = False
                    if kernel32.Thread32First(snap, ctypes.byref(te)):
                        while True:
                            if te.pid == pid:
                                import win32gui, win32con
                                hwnds = []
                                win32gui.EnumThreadWindows(te.tid, lambda h, l: l.append(h), hwnds)
                                for hwnd in hwnds:
                                    title = win32gui.GetWindowText(hwnd)
                                    cls = win32gui.GetClassName(hwnd)
                                    if "MetaTrader" in title or "MetaTrader" in cls:
                                        logger.info(f"[AutoTrading] Found MT5: hwnd={hwnd}")
                                        win32gui.PostMessage(hwnd, win32con.WM_COMMAND, 32843, 0)
                                        time.sleep(0.2)
                                        win32gui.PostMessage(hwnd, win32con.WM_COMMAND, 32843, 0)
                                        logger.info("[AutoTrading] Ctrl+E sent!")
                                        found = True
                                        break
                                if found: break
                            if not kernel32.Thread32Next(snap, ctypes.byref(te)): break
                    kernel32.CloseHandle(snap)
                    if not found:
                        logger.warning(f"[AutoTrading] MT5 PID={pid} running but no window found")
        except Exception as e:
            logger.warning(f"[AutoTrading] Failed: {e}")
except Exception:
    pass

# Start price bridge (yfinance → C++ Trade Server)
try:
    price_bridge.start()
    logger.info("Price bridge running")
except Exception as e:
    logger.warning(f"Price bridge failed: {e}")


@app.get("/")
async def root():
    return {"success": True, "message": "Guardian API is running", "data": {"version": settings.VERSION}}


@app.get("/bridge/status")
async def bridge_status():
    """Check price bridge status."""
    return {"success": True, "data": {"running": getattr(price_bridge, '_running', False)}}
