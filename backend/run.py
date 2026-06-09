#!/usr/bin/env python3
"""Run the AI Stock Guardian backend server."""
import sys
import asyncio

# Fix Windows ProactorEventLoop conflict with ZMQ — MUST be before any asyncio usage
if sys.platform == "win32":
    asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())

import os
import uvicorn


def main():
    port = int(os.environ.get("PORT", 8150))
    host = os.environ.get("HOST", "0.0.0.0")
    print(f"Starting AI Stock Guardian API on {host}:{port}")
    print(f"Set LLM_PROVIDER_API_KEY in .env to enable AI chat proxy")
    print()
    reload_flag = os.environ.get("DEV", "0") == "1"
    uvicorn.run("app.main:app", host=host, port=port, reload=reload_flag, workers=1,
                loop="asyncio")


if __name__ == "__main__":
    main()
