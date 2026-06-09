"""Massive.com real-time market data integration."""
import json
import logging
import os
import threading
from typing import Optional
import websocket
import requests

logger = logging.getLogger("guardian.massive")

MASSIVE_API_KEY = os.environ.get("MASSIVE_API_KEY", "")
MASSIVE_REST_URL = "https://api.massive.com/v1"
MASSIVE_WS_URL = "wss://socket.massive.com"

class MassiveDataService:
    def __init__(self):
        self.api_key = MASSIVE_API_KEY
        self.ws: Optional[websocket.WebSocketApp] = None
        self._connected = False
        self._listeners = []
        self._ticker_subscriptions = set()

    @property
    def is_connected(self) -> bool:
        return self._connected

    def get_snapshot(self, ticker: str, asset_class: str = "stocks") -> Optional[dict]:
        if not self.api_key:
            logger.warning("MASSIVE_API_KEY not set")
            return None
        try:
            url = f"{MASSIVE_REST_URL}/{asset_class}/snapshots/{ticker}"
            resp = requests.get(url, params={"apiKey": self.api_key}, timeout=10)
            if resp.status_code == 200:
                return resp.json()
            elif resp.status_code == 404:
                logger.debug(f"Massive snapshot not found for {ticker}")
                return None
            else:
                logger.warning(f"Massive API error {resp.status_code}: {resp.text[:200]}")
                return None
        except Exception as e:
            logger.debug(f"Massive snapshot error: {e}")
            return None

    def get_quotes(self, tickers: list[str], asset_class: str = "stocks") -> Optional[dict]:
        if not self.api_key or not tickers:
            return None
        try:
            url = f"{MASSIVE_REST_URL}/{asset_class}/quotes"
            resp = requests.post(url, json={"tickers": tickers, "apiKey": self.api_key}, timeout=10)
            if resp.status_code == 200:
                return resp.json()
            return None
        except Exception as e:
            logger.debug(f"Massive quotes error: {e}")
            return None

massive_service = MassiveDataService()
