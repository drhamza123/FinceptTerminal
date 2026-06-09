#!/usr/bin/env python3
"""Run the backend using waitress (synchronous WSGI) — no asyncio issues on Windows."""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from app.main import app
from a2wsgi import ASGIMiddleware

wsgi_app = ASGIMiddleware(app)

port = int(os.environ.get("PORT", 8155))
host = os.environ.get("HOST", "0.0.0.0")

print(f"Starting Guardian API on {host}:{port} via waitress (sync WSGI)")
print()

from waitress import serve
serve(wsgi_app, host=host, port=port, threads=8)
