import asyncio
import logging
import os
import platform
import re
import shutil
import tempfile

from app.config import settings

logger = logging.getLogger("guardian.mt5")

_compile_semaphore = asyncio.Semaphore(1)
METAEDITOR_PATH = os.environ.get("MT5_METAEDITOR_PATH", "") or settings.MT5_METAEDITOR_PATH
EXPERTS_DIR = os.environ.get("MT5_EXPERTS_DIR", "") or settings.MT5_EXPERTS_DIR
DEV_MODE = (os.environ.get("MT5_DEV_MODE", "") or str(settings.MT5_DEV_MODE)).lower() in {"1", "true", "yes", "on"}


def _find_metaeditor() -> str:
    if METAEDITOR_PATH and os.path.exists(METAEDITOR_PATH):
        return METAEDITOR_PATH
    search_paths = [
        r"C:\Program Files\MetaTrader 5\metaeditor64.exe",
        r"C:\Program Files\MetaTrader 5\metaeditor.exe",
        os.path.expandvars(r"%PROGRAMFILES%\MetaTrader 5\metaeditor64.exe"),
        os.path.expandvars(r"%APPDATA%\MetaQuotes\Terminal\Common\metaeditor64.exe"),
    ]
    for p in search_paths:
        if os.path.exists(p):
            return p
    if platform.system() == "Darwin":
        wine_paths = [
            os.path.expanduser("~/.wine/drive_c/Program Files/MetaTrader 5/metaeditor64.exe"),
            "/Applications/Wine Staging.app/Contents/Resources/wine/bin/wine",
        ]
        for wp in wine_paths:
            if os.path.exists(wp):
                return wp
        if METAEDITOR_PATH:
            return METAEDITOR_PATH
    return ""


def _to_wine_path(local_path: str) -> str:
    if platform.system() != "Darwin":
        return local_path
    if local_path.startswith("/"):
        return "Z:" + local_path.replace("/", "\\")
    return local_path


async def _dev_compile(source_code: str, output_name: str) -> dict:
    """Dev-mode: simulate compilation without metaeditor64.exe."""
    tmp_dir = tempfile.mkdtemp(prefix="guardian_mt5_")
    src_name = output_name or "GeneratedEA"
    src_path = os.path.join(tmp_dir, f"{src_name}.mq5")
    ex5_path = os.path.join(tmp_dir, f"{src_name}.ex5")
    try:
        with open(src_path, "w") as f:
            f.write(source_code)
        # Create a dummy .ex5 file (real compilation needs metaeditor64.exe on Windows)
        with open(ex5_path, "wb") as f:
            f.write(b"\x00" * 1024)  # placeholder
        if EXPERTS_DIR and os.path.isdir(EXPERTS_DIR):
            dest = os.path.join(EXPERTS_DIR, f"{src_name}.ex5")
            with open(ex5_path, "rb") as sf, open(dest, "wb") as df:
                df.write(sf.read())
            ex5_path = dest
        return {
            "success": True,
            "ex5_path": ex5_path,
            "errors": [],
            "warnings": ["Dev mode: simulated compilation. Set MT5_METAEDITOR_PATH for real compilation."],
        }
    finally:
        asyncio.get_event_loop().call_later(30, lambda: _cleanup(tmp_dir))


async def compile_mql5(source_code: str, output_name: str | None = None) -> dict:
    # Dev mode: simulate compilation without metaeditor64.exe
    if DEV_MODE:
        return await _dev_compile(source_code, output_name or "GeneratedEA")

    metaeditor = _find_metaeditor()
    if not metaeditor:
        return {"success": False, "ex5_path": "", "errors": ["metaeditor64.exe not found. Set MT5_METAEDITOR_PATH in .env or enable MT5_DEV_MODE=1"], "warnings": []}
    tmp_dir = tempfile.mkdtemp(prefix="guardian_mt5_")
    src_name = output_name or "GeneratedEA"
    src_path = os.path.join(tmp_dir, f"{src_name}.mq5")
    log_path = os.path.join(tmp_dir, "compile.log")
    try:
        with open(src_path, "w") as f:
            f.write(source_code)
        async with _compile_semaphore:
            if platform.system() == "Darwin" and "wine" in metaeditor.lower():
                cmd = ["wine", metaeditor, f"/compile:\"{_to_wine_path(src_path)}\"", f"/log:\"{_to_wine_path(log_path)}\""]
            else:
                cmd = [metaeditor, f"/compile:\"{src_path}\"", f"/log:\"{log_path}\""]
            proc = await asyncio.create_subprocess_exec(*cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
            try:
                await asyncio.wait_for(proc.communicate(), timeout=30)
            except asyncio.TimeoutError:
                proc.kill()
                return {"success": False, "ex5_path": "", "errors": ["Compilation timed out"], "warnings": []}
        log_text = ""
        if os.path.exists(log_path):
            with open(log_path) as f:
                log_text = f.read()
        ex5_path = src_path.replace(".mq5", ".ex5")
        if os.path.exists(ex5_path):
            if EXPERTS_DIR and os.path.isdir(EXPERTS_DIR):
                dest = os.path.join(EXPERTS_DIR, f"{src_name}.ex5")
                with open(ex5_path, "rb") as sf, open(dest, "wb") as df:
                    df.write(sf.read())
                ex5_path = dest
            errors = re.findall(r"error.*?:.*?(?:\n|$)", log_text, re.IGNORECASE)
            warnings = re.findall(r"warning.*?:.*?(?:\n|$)", log_text, re.IGNORECASE)
            return {"success": True, "ex5_path": ex5_path, "errors": [e.strip() for e in errors if e.strip()], "warnings": [w.strip() for w in warnings if w.strip()]}
        else:
            errors = re.findall(r".*error.*?:.*", log_text, re.IGNORECASE) or ["Compilation failed — see log"]
            return {"success": False, "ex5_path": "", "errors": [e.strip() for e in errors[:10]], "warnings": []}
    finally:
        asyncio.get_event_loop().call_later(30, lambda: _cleanup(tmp_dir))


def _cleanup(path: str):
    try:
        shutil.rmtree(path)
    except Exception:
        pass


async def deploy_to_experts(ex5_path: str, target_name: str | None = None) -> dict:
    if not EXPERTS_DIR:
        if DEV_MODE:
            tmp_dir = tempfile.mkdtemp(prefix="guardian_experts_")
            dest_name = target_name or os.path.basename(ex5_path)
            if not dest_name.endswith(".ex5"):
                dest_name += ".ex5"
            dest_path = os.path.join(tmp_dir, dest_name)
            try:
                with open(ex5_path, "rb") as sf, open(dest_path, "wb") as df:
                    df.write(sf.read())
                return {"success": True, "deployed_path": dest_path, "note": "Dev mode: deployed to temp dir. Set MT5_EXPERTS_DIR for real deployment."}
            except Exception as e:
                return {"success": False, "error": str(e)}
        return {"success": False, "error": "MT5_EXPERTS_DIR not configured. Set it in .env or enable MT5_DEV_MODE=1"}
    os.makedirs(EXPERTS_DIR, exist_ok=True)
    dest_name = target_name or os.path.basename(ex5_path)
    if not dest_name.endswith(".ex5"):
        dest_name += ".ex5"
    dest_path = os.path.join(EXPERTS_DIR, dest_name)
    try:
        with open(ex5_path, "rb") as sf, open(dest_path, "wb") as df:
            df.write(sf.read())
        return {"success": True, "deployed_path": dest_path}
    except Exception as e:
        return {"success": False, "error": str(e)}
