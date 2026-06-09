"""Server-Side Alert Engine — evaluates price conditions and fires webhooks."""
import asyncio
import json
import logging
import os
import time
from typing import Optional
import httpx
import yfinance as yf

logger = logging.getLogger("guardian.alert_engine")

# In-memory alert store (would be persisted to DB in production)
_alert_rules: dict[str, dict] = {}
_last_checked: dict[str, float] = {}

class AlertRule:
    def __init__(self, rule_id: str, symbol: str, condition: str, threshold: float,
                 webhook_url: str = "", user_id: str = ""):
        self.id = rule_id
        self.symbol = symbol
        self.condition = condition  # "above", "below", "crosses_above", "crosses_below"
        self.threshold = threshold
        self.webhook_url = webhook_url
        self.user_id = user_id
        self.last_value = 0.0
        self.last_triggered = 0
        self.active = True

    def evaluate(self, current_price: float) -> tuple[bool, str]:
        now = time.time()
        # Min 60s between triggers for same rule
        if now - self.last_triggered < 60:
            return False, ""

        triggered = False
        message = ""

        if self.condition == "above" and current_price > self.threshold:
            triggered = True
            message = f"{self.symbol} crossed ABOVE ${self.threshold:.2f} (now ${current_price:.2f})"
        elif self.condition == "below" and current_price < self.threshold:
            triggered = True
            message = f"{self.symbol} crossed BELOW ${self.threshold:.2f} (now ${current_price:.2f})"
        elif self.condition == "crosses_above" and self.last_value > 0:
            if self.last_value <= self.threshold and current_price > self.threshold:
                triggered = True
                message = f"{self.symbol} CROSSED ABOVE ${self.threshold:.2f} (now ${current_price:.2f})"
        elif self.condition == "crosses_below" and self.last_value > 0:
            if self.last_value >= self.threshold and current_price < self.threshold:
                triggered = True
                message = f"{self.symbol} CROSSED BELOW ${self.threshold:.2f} (now ${current_price:.2f})"

        self.last_value = current_price
        if triggered:
            self.last_triggered = now

        return triggered, message


class AlertEngine:
    def __init__(self):
        self.rules: dict[str, AlertRule] = {}
        self._task: Optional[asyncio.Task] = None

    def add_rule(self, symbol: str, condition: str, threshold: float,
                 webhook_url: str = "", user_id: str = "") -> str:
        import uuid
        rule_id = uuid.uuid4().hex[:10]
        self.rules[rule_id] = AlertRule(rule_id, symbol, condition, threshold, webhook_url, user_id)
        logger.info(f"Alert rule added: {symbol} {condition} {threshold} (id={rule_id})")
        return rule_id

    def remove_rule(self, rule_id: str) -> bool:
        if rule_id in self.rules:
            del self.rules[rule_id]
            logger.info(f"Alert rule removed: {rule_id}")
            return True
        return False

    def list_rules(self) -> list[dict]:
        return [{
            "id": r.id, "symbol": r.symbol, "condition": r.condition,
            "threshold": r.threshold, "active": r.active,
            "last_value": r.last_value, "last_triggered": r.last_triggered,
        } for r in self.rules.values()]

    async def start(self):
        logger.info("Alert engine started")
        self._task = asyncio.create_task(self._run())

    async def stop(self):
        if self._task:
            self._task.cancel()
            self._task = None
        logger.info("Alert engine stopped")

    async def _run(self):
        while True:
            try:
                await self._check_all()
            except Exception as e:
                logger.error(f"Alert check error: {e}")
            await asyncio.sleep(15)  # check every 15 seconds

    async def _check_all(self):
        if not self.rules:
            return

        # Group rules by symbol
        by_symbol: dict[str, list[AlertRule]] = {}
        for rule in self.rules.values():
            if rule.active:
                by_symbol.setdefault(rule.symbol, []).append(rule)

        for symbol, rules in by_symbol.items():
            try:
                price = await self._fetch_price(symbol)
                if price is None or price <= 0:
                    continue

                for rule in rules:
                    triggered, message = rule.evaluate(price)
                    if triggered:
                        logger.info(f"ALERT: {message}")
                        if rule.webhook_url:
                            await self._fire_webhook(rule.webhook_url, message, symbol, price)
            except Exception as e:
                logger.warning(f"Failed to check {symbol}: {e}")

    async def _fetch_price(self, symbol: str) -> Optional[float]:
        """Fetch current price for a symbol."""
        try:
            ticker = yf.Ticker(symbol)
            data = ticker.history(period="1d", interval="1m")
            if not data.empty:
                return float(data['Close'].iloc[-1])
        except Exception:
            pass

        # Fallback: try fast_info
        try:
            ticker = yf.Ticker(symbol)
            info = ticker.fast_info
            if hasattr(info, 'last_price') and info.last_price:
                return float(info.last_price)
            if hasattr(info, 'previous_close') and info.previous_close:
                return float(info.previous_close)
        except Exception:
            pass

        return None

    async def _fire_webhook(self, url: str, message: str, symbol: str, price: float):
        """Fire a webhook with the alert payload."""
        try:
            async with httpx.AsyncClient(timeout=10) as client:
                payload = {
                    "type": "alert",
                    "symbol": symbol,
                    "price": price,
                    "message": message,
                    "timestamp": time.time(),
                }
                await client.post(url, json=payload)
                logger.info(f"Webhook fired: {url}")
        except Exception as e:
            logger.warning(f"Webhook failed: {e}")


# Singleton
alert_engine = AlertEngine()
