import asyncio
import logging
import time
from typing import Dict, Optional

logger = logging.getLogger("guardian.synthetic_orders")

class SyntheticOrderManager:
    def __init__(self):
        self.ocos: Dict[str, dict] = {}       # oco_id -> {parent, child_a, child_b, active}
        self.otos: Dict[str, dict] = {}       # oto_id -> {trigger, target, triggered}
        self.icebergs: Dict[str, dict] = {}   # iceberg_id -> {total, remaining, visible, slice_size, price}

    def register_oco(self, parent_id: str, child_a_id: str, child_b_id: str) -> str:
        oco_id = f"oco_{parent_id}"
        self.ocos[oco_id] = {"parent": parent_id, "a": child_a_id, "b": child_b_id, "active": True}
        logger.info("OCO registered: %s (%s, %s)", oco_id, child_a_id, child_b_id)
        return oco_id

    def register_oto(self, trigger_id: str, target_id: str, trigger_price: float, direction: str) -> str:
        oto_id = f"oto_{trigger_id}"
        self.otos[oto_id] = {"trigger": trigger_id, "target": target_id,
                             "price": trigger_price, "dir": direction, "triggered": False}
        logger.info("OTO registered: %s → %s @ %.2f %s", oto_id, target_id, trigger_price, direction)
        return oto_id

    def register_iceberg(self, iceberg_id: str, total_qty: float, slice_size: float, price: float) -> str:
        self.icebergs[iceberg_id] = {
            "total": total_qty, "remaining": total_qty,
            "visible": min(slice_size, total_qty), "slice_size": slice_size,
            "price": price, "active": True
        }
        logger.info("Iceberg registered: %s total=%.2f slice=%.2f", iceberg_id, total_qty, slice_size)
        return iceberg_id

    async def on_order_update(self, order_id: str, status: str, filled_qty: float = 0):
        """Called when ANY order status changes via WebSocket or bridge."""
        tasks = []

        # OCO check
        for oid, oco in list(self.ocos.items()):
            if not oco["active"]:
                continue
            if status in ("FILLED", "CANCELED", "REJECTED"):
                if order_id == oco["parent"]:
                    logger.info("OCO %s: parent done, spawning children", oid)
                    tasks.append(self._spawn(oco["a"]))
                    tasks.append(self._spawn(oco["b"]))
                    oco["active"] = False
                elif order_id == oco["a"] or order_id == oco["b"]:
                    sibling = oco["b"] if order_id == oco["a"] else oco["a"]
                    logger.info("OCO %s: child done, canceling sibling %s", oid, sibling)
                    tasks.append(self._cancel(sibling))
                    oco["active"] = False

        # OTO check
        for oid, oto in list(self.otos.items()):
            if oto["triggered"]:
                continue
            if order_id == oto["trigger"] and status == "FILLED":
                logger.info("OTO %s: trigger filled, spawning target", oid)
                tasks.append(self._spawn(oto["target"]))
                oto["triggered"] = True

        # Iceberg check
        for iid, ice in list(self.icebergs.items()):
            if not ice["active"]:
                continue
            if order_id == iid and status == "FILLED":
                ice["remaining"] -= filled_qty
                logger.info("Iceberg %s: %.2f remaining, spawning next slice", iid, ice["remaining"])
                if ice["remaining"] > 0:
                    next_qty = min(ice["slice_size"], ice["remaining"])
                    tasks.append(self._spawn_slice(iid, next_qty, ice["price"]))
                else:
                    ice["active"] = False

        if tasks:
            await asyncio.gather(*tasks)

    async def _spawn(self, order_id: str):
        """Send the child order to the broker."""
        import httpx
        try:
            async with httpx.AsyncClient() as c:
                await c.post("http://localhost:8150/mt5/order/market",
                    json={"order_id": order_id, "action": "submit"},
                    timeout=5)
        except Exception as e:
            logger.error("Spawn failed for %s: %s", order_id, e)

    async def _cancel(self, order_id: str):
        """Cancel a sibling order immediately."""
        import httpx
        try:
            async with httpx.AsyncClient() as c:
                await c.post("http://localhost:8150/mt5/order/cancel",
                    json={"order_id": order_id},
                    timeout=5)
        except Exception as e:
            logger.error("Cancel failed for %s: %s", order_id, e)

    async def _spawn_slice(self, iceberg_id: str, qty: float, price: float):
        """Place the next visible slice of an iceberg order."""
        import httpx
        try:
            async with httpx.AsyncClient() as c:
                await c.post("http://localhost:8150/mt5/order/limit",
                    json={"symbol": "XAUUSD", "volume": qty, "price": price,
                          "side": "BUY", "iceberg_id": iceberg_id},
                    timeout=5)
        except Exception as e:
            logger.error("Iceberg slice failed for %s: %s", iceberg_id, e)

synthetic_mgr = SyntheticOrderManager()
