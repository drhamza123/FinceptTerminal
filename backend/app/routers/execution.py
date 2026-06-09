import os
import subprocess
import sys
import tempfile
import time
import uuid
from dataclasses import dataclass, field

from fastapi import APIRouter, Body, Depends

from app.routers.auth import resolve_user

router = APIRouter(tags=["execution"])


@dataclass
class ScriptDeployment:
    id: str
    name: str
    language: str
    source: str
    target: str
    status: str = "deployed"
    created_at: float = field(default_factory=time.time)
    updated_at: float = field(default_factory=time.time)
    mt5_deployed: bool = False
    mt5_path: str = ""
    last_output: str = ""
    last_error: str = ""
    process: subprocess.Popen | None = None
    script_path: str = ""

    def to_dict(self) -> dict:
        running = self.process is not None and self.process.poll() is None
        return {
            "id": self.id,
            "name": self.name,
            "language": self.language,
            "target": self.target,
            "status": "running" if running else self.status,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
            "mt5_deployed": self.mt5_deployed,
            "mt5_path": self.mt5_path,
            "last_output": self.last_output[-4000:],
            "last_error": self.last_error[-4000:],
            "pid": self.process.pid if running else 0,
        }


_deployments: dict[str, ScriptDeployment] = {}


def _normalize_language(value: str, source: str) -> str:
    lang = (value or "").strip().lower()
    if lang in {"mql5", "mq5", "mt5"}:
        return "mql5"
    if lang in {"python", "py"}:
        return "python"
    probe = source.lower()
    if "#property" in probe or "ontick(" in probe or "ctrade" in probe:
        return "mql5"
    return "python"


@router.get("/execution/scripts")
async def list_scripts(user=Depends(resolve_user)):
    return {"success": True, "data": [d.to_dict() for d in _deployments.values()]}


@router.get("/execution/scripts/status")
async def list_script_status():
    return {"success": True, "data": [{
        "id": d.id,
        "name": d.name,
        "language": d.language,
        "target": d.target,
        "status": d.to_dict()["status"],
        "mt5_deployed": d.mt5_deployed,
        "mt5_path": d.mt5_path,
    } for d in _deployments.values()]}


@router.post("/execution/scripts/deploy")
async def deploy_script(body: dict = Body(...), user=Depends(resolve_user)):
    source = (body.get("source") or body.get("code") or "").strip()
    if not source:
        return {"success": False, "error": "No script source provided"}

    language = _normalize_language(body.get("language", ""), source)
    name = (body.get("name") or ("AIGeneratedEA" if language == "mql5" else "PythonStrategy")).strip()
    target = body.get("target") or ("mt5" if language == "mql5" else "execution")
    deployment_id = uuid.uuid4().hex[:10]
    deployment = ScriptDeployment(
        id=deployment_id,
        name=name,
        language=language,
        source=source,
        target=target,
        mt5_deployed=bool(body.get("mt5_deployed", False)),
        mt5_path=body.get("mt5_path", ""),
    )
    _deployments[deployment_id] = deployment
    return {"success": True, "data": deployment.to_dict()}


@router.post("/execution/scripts/{deployment_id}/mt5")
async def mark_mt5_deployed(deployment_id: str, body: dict = Body(default={}), user=Depends(resolve_user)):
    deployment = _deployments.get(deployment_id)
    if not deployment:
        return {"success": False, "error": "Deployment not found"}
    deployment.mt5_deployed = bool(body.get("mt5_deployed", True))
    deployment.mt5_path = body.get("mt5_path", deployment.mt5_path)
    deployment.updated_at = time.time()
    return {"success": True, "data": deployment.to_dict()}


@router.post("/execution/scripts/{deployment_id}/run")
async def run_script(deployment_id: str, user=Depends(resolve_user)):
    deployment = _deployments.get(deployment_id)
    if not deployment:
        return {"success": False, "error": "Deployment not found"}
    if deployment.language != "python":
        deployment.status = "deployed"
        deployment.last_error = "MQL5 scripts run inside MetaTrader after EA deployment."
        deployment.updated_at = time.time()
        return {"success": False, "error": deployment.last_error, "data": deployment.to_dict()}
    if deployment.process is not None and deployment.process.poll() is None:
        return {"success": True, "data": deployment.to_dict(), "message": "Already running"}

    work_dir = tempfile.mkdtemp(prefix="fincept_exec_")
    script_path = os.path.join(work_dir, f"{deployment.name or deployment.id}.py")
    with open(script_path, "w", encoding="utf-8") as f:
        f.write(deployment.source)

    creationflags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    proc = subprocess.Popen(
        [sys.executable, "-u", script_path],  # -u for unbuffered output
        cwd=work_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        creationflags=creationflags,
    )
    deployment.process = proc
    deployment.script_path = script_path
    deployment.status = "running"
    deployment.updated_at = time.time()
    return {"success": True, "data": deployment.to_dict()}


@router.post("/execution/scripts/{deployment_id}/stop")
async def stop_script(deployment_id: str, user=Depends(resolve_user)):
    deployment = _deployments.get(deployment_id)
    if not deployment:
        return {"success": False, "error": "Deployment not found"}
    if deployment.process is not None and deployment.process.poll() is None:
        deployment.process.terminate()
        try:
            out, err = deployment.process.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            deployment.process.kill()
            out, err = deployment.process.communicate(timeout=2)
        deployment.last_output = out or deployment.last_output
        deployment.last_error = err or deployment.last_error
    deployment.status = "stopped"
    deployment.updated_at = time.time()
    return {"success": True, "data": deployment.to_dict()}
