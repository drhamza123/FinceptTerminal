import os
from fastapi import APIRouter
from fastapi.responses import HTMLResponse, FileResponse

router = APIRouter(tags=["download"])

DOWNLOAD_DIR = r"C:\opt\guardian\downloads"

HTML_PAGE = """<!DOCTYPE html>
<html lang="en">
<head><meta charset="UTF-8"><title>AI Stock Guardian — Download</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#0a0a0f;color:#e5e5e5;display:flex;justify-content:center;align-items:center;min-height:100vh;}
.card{background:#131722;border:1px solid #1e222d;border-radius:16px;padding:48px;max-width:520px;text-align:center;}
h1{font-size:28px;font-weight:700;margin-bottom:8px;color:#f59e0b;}
.sub{color:#787b86;font-size:14px;margin-bottom:32px;}
.btn{display:inline-block;background:#f59e0b;color:#080808;padding:14px 36px;border-radius:8px;font-weight:700;font-size:16px;text-decoration:none;}
.btn:hover{background:#d97706;}
.info{margin-top:24px;font-size:12px;color:#787b86;color:#f59e0b;}
</style></head><body>
<div class="card">
<h1>AI Stock Guardian</h1>
<p class="sub">Professional trading terminal — Windows x64</p>
<a class="btn" href="/terminal/exe">Download Installer (44 MB)</a>
<div class="info">Version 4.0.3 &bull; MSVC 2022 &bull; Qt 6.8.3</div>
</div></body></html>"""

@router.get("/terminal", response_class=HTMLResponse)
async def download_page():
    return HTMLResponse(HTML_PAGE)

@router.get("/terminal/exe")
async def download_exe():
    path = os.path.join(DOWNLOAD_DIR, "AI_Stock_Guardian_Setup.exe")
    if not os.path.exists(path):
        return HTMLResponse("<h1>File not found</h1>", status_code=404)
    return FileResponse(path, filename="AI_Stock_Guardian_Setup.exe", media_type="application/octet-stream")
