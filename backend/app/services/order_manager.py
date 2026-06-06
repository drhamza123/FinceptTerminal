import asyncio
import json
import logging
import time
import uuid
from enum import IntEnum
from typing import Optional

logger = logging.getLogger("guardian.orders")


class OrderState(IntEnum):
    PENDING = 0
    SUBMITTED = 1
    PARTIAL = 2
    FILLED = 3
    CANCELLED = 4
    REJECTED = 5
    EXPIRED = 6
    TRIGGERED = 7  # For OTO trigger orders


class OrderSide(IntEnum):
    BUY = 0
    SELL = 1


class OrderType(IntEnum):
    MARKET = 0
    LIMIT = 1
    STOP = 2
    STOP_LIMIT = 3
    BRACKET = 4
    OCO = 5
    OTO = 6
    ICEBERG = 7
    TRAILING_STOP = 8


class Order:
    def __init__(self, *, symbol: str, side: OrderSide, order_type: OrderType,
                 volume: float, price: float = 0, sl: float = 0, tp: float = 0,
                 limit_price: float = 0, stop_price: float = 0,
                 trailing_distance: float = 0, visible_volume: float = 0,
                 trigger_price: float = 0, trigger_direction: str = "above",
                 expiration: int = 0, ea_key: str = "",
                 parent_id: str = "", sibling_id: str = ""):
        self.id = str(uuid.uuid4())[:8]
        self.symbol = symbol.upper()
        self.side = OrderSide(side) if isinstance(side, int) else side
        self.order_type = OrderType(order_type) if isinstance(order_type, int) else order_type
        self.volume = volume
        self.price = price
        self.sl = sl
        self.tp = tp
        self.limit_price = limit_price
        self.stop_price = stop_price
        self.trailing_distance = trailing_distance
        self.visible_volume = visible_volume or volume
        self.trigger_price = trigger_price
        self.trigger_direction = trigger_direction
        self.expiration = expiration
        self.ea_key = ea_key
        self.parent_id = parent_id
        self.sibling_id = sibling_id  # OCO sibling

        self.state = OrderState.PENDING
        self.ticket = 0
        self.filled_volume = 0.0
        self.avg_fill_price = 0.0
        self.created_at = time.time()
        self.updated_at = time.time()
        self.filled_at: Optional[float] = None
        self.reject_reason: str = ""
        self.execution_log: list[dict] = []

    def to_dict(self) -> dict:
        return {
            "id": self.id, "symbol": self.symbol,
            "side": "BUY" if self.side == OrderSide.BUY else "SELL",
            "order_type": self.order_type.name,
            "state": self.state.name,
            "volume": self.volume, "filled_volume": self.filled_volume,
            "price": self.price, "sl": self.sl, "tp": self.tp,
            "limit_price": self.limit_price, "stop_price": self.stop_price,
            "trailing_distance": self.trailing_distance,
            "trigger_price": self.trigger_price,
            "trigger_direction": self.trigger_direction,
            "ticket": self.ticket,
            "avg_fill_price": self.avg_fill_price,
            "ea_key": self.ea_key,
            "parent_id": self.parent_id, "sibling_id": self.sibling_id,
            "created_at": self.created_at, "updated_at": self.updated_at,
            "filled_at": self.filled_at,
            "reject_reason": self.reject_reason,
            "execution_log": self.execution_log[-10:],
        }


class OrderManager:
    """Central order lifecycle manager with state machine.
    
    State transitions:
      PENDING -> SUBMITTED -> PARTIAL -> FILLED
      PENDING -> REJECTED
      PENDING/SUBMITTED -> CANCELLED
      PENDING -> EXPIRED
      OTO: TRIGGERED -> SUBMITTED
    """

    def __init__(self):
        self._orders: dict[str, Order] = {}
        self._positions: dict[str, dict] = {}  # ticket -> position info
        self._ea_keys: set[str] = set()
        self._lock = asyncio.Lock()
        self._oto_watchers: list[dict] = []
        self._trailing_stops: dict[int, dict] = {}  # ticket -> trailing config

    # ── Order lifecycle ──────────────────────────────────────────

    async def place_market(self, symbol: str, side: OrderSide, volume: float,
                           sl: float = 0, tp: float = 0,
                           ea_key: str = "") -> Order:
        order = Order(symbol=symbol, side=side, order_type=OrderType.MARKET,
                      volume=volume, sl=sl, tp=tp, ea_key=ea_key)
        async with self._lock:
            order.state = OrderState.SUBMITTED
            order.updated_at = time.time()
            self._orders[order.id] = order
        logger.info("Market order placed: %s %s %s %s",
                     order.id, side.name, volume, symbol)
        return order

    async def place_limit(self, symbol: str, side: OrderSide, volume: float,
                          price: float, sl: float = 0, tp: float = 0,
                          ea_key: str = "") -> Order:
        order = Order(symbol=symbol, side=side, order_type=OrderType.LIMIT,
                      volume=volume, price=price, sl=sl, tp=tp, ea_key=ea_key)
        async with self._lock:
            self._orders[order.id] = order
        logger.info("Limit order placed: %s %s %s @ %s",
                     order.id, side.name, volume, price)
        return order

    async def place_stop(self, symbol: str, side: OrderSide, volume: float,
                         stop_price: float, limit_price: float = 0,
                         sl: float = 0, tp: float = 0,
                         ea_key: str = "") -> Order:
        ot = OrderType.STOP_LIMIT if limit_price > 0 else OrderType.STOP
        order = Order(symbol=symbol, side=side, order_type=ot,
                      volume=volume, stop_price=stop_price,
                      limit_price=limit_price, sl=sl, tp=tp, ea_key=ea_key)
        async with self._lock:
            self._orders[order.id] = order
        return order

    async def place_bracket(self, symbol: str, side: OrderSide, volume: float,
                            entry_price: float, sl: float, tp: float,
                            ea_key: str = "") -> Order:
        order = Order(symbol=symbol, side=side, order_type=OrderType.BRACKET,
                      volume=volume, price=entry_price, sl=sl, tp=tp,
                      ea_key=ea_key)
        async with self._lock:
            order.state = OrderState.SUBMITTED
            self._orders[order.id] = order
        return order

    async def place_oco(self, symbol: str, side: OrderSide, volume: float,
                        stop_price: float, limit_price: float,
                        ea_key: str = "") -> tuple[Order, Order]:
        stop = Order(symbol=symbol, side=side, order_type=OrderType.STOP,
                     volume=volume, stop_price=stop_price, ea_key=ea_key)
        lmt = Order(symbol=symbol, side=side, order_type=OrderType.LIMIT,
                    volume=volume, limit_price=limit_price, ea_key=ea_key)
        stop.sibling_id = lmt.id
        lmt.sibling_id = stop.id
        async with self._lock:
            self._orders[stop.id] = stop
            self._orders[lmt.id] = lmt
        return stop, lmt

    async def place_oto(self, symbol: str, trigger_side: OrderSide,
                        trigger_price: float, trigger_direction: str,
                        target_side: OrderSide, target_volume: float,
                        target_price: float, sl: float = 0, tp: float = 0,
                        ea_key: str = "") -> tuple[Order, Order]:
        trigger = Order(symbol=symbol, side=trigger_side,
                        order_type=OrderType.STOP,
                        volume=0, stop_price=trigger_price,
                        trigger_price=trigger_price,
                        trigger_direction=trigger_direction,
                        ea_key=ea_key)
        target = Order(symbol=symbol, side=target_side,
                       order_type=OrderType.MARKET,
                       volume=target_volume, price=target_price,
                       sl=sl, tp=tp, ea_key=ea_key, parent_id=trigger.id)
        trigger.sibling_id = target.id
        async with self._lock:
            self._orders[trigger.id] = trigger
            self._orders[target.id] = target
            self._oto_watchers.append({
                "trigger_id": trigger.id,
                "target_id": target.id,
                "symbol": symbol, "direction": trigger_direction,
                "price": trigger_price,
            })
        return trigger, target

    async def place_iceberg(self, symbol: str, side: OrderSide,
                            volume: float, price: float,
                            visible_volume: float, sl: float = 0,
                            tp: float = 0, ea_key: str = "") -> Order:
        order = Order(symbol=symbol, side=side, order_type=OrderType.ICEBERG,
                      volume=volume, price=price,
                      visible_volume=visible_volume, sl=sl, tp=tp,
                      ea_key=ea_key)
        async with self._lock:
            self._orders[order.id] = order
        return order

    async def cancel_order(self, order_id: str) -> bool:
        async with self._lock:
            order = self._orders.get(order_id)
            if not order:
                return False
            if order.state in (OrderState.FILLED, OrderState.CANCELLED,
                               OrderState.REJECTED):
                return False
            old = order.state
            order.state = OrderState.CANCELLED
            order.updated_at = time.time()
            order.execution_log.append({
                "t": time.time(), "event": "cancelled",
                "from": old.name
            })
            # Cancel OCO sibling
            if order.sibling_id:
                sibling = self._orders.get(order.sibling_id)
                if sibling and sibling.state == OrderState.PENDING:
                    sibling.state = OrderState.CANCELLED
                    sibling.updated_at = time.time()
        return True

    async def modify_sltp(self, order_id: str, sl: float,
                          tp: float) -> bool:
        async with self._lock:
            order = self._orders.get(order_id)
            if not order or order.state not in (OrderState.SUBMITTED,
                                                 OrderState.PARTIAL,
                                                 OrderState.FILLED):
                return False
            order.sl = sl
            order.tp = tp
            order.updated_at = time.time()
        return True

    async def set_trailing_stop(self, ticket: int,
                                distance: float) -> bool:
        async with self._lock:
            self._trailing_stops[ticket] = {
                "distance": distance, "activated_at": time.time()
            }
        return True

    async def fill_order(self, order_id: str, price: float,
                         volume: Optional[float] = None,
                         ticket: int = 0) -> Optional[Order]:
        async with self._lock:
            order = self._orders.get(order_id)
            if not order:
                return None
            fill_vol = volume or order.volume
            order.filled_volume += fill_vol
            order.avg_fill_price = (
                (order.avg_fill_price * (order.filled_volume - fill_vol) +
                 price * fill_vol) / order.filled_volume
                if order.filled_volume > 0 else price
            )
            order.ticket = ticket or order.ticket
            if order.filled_volume >= order.volume * 0.999:
                order.state = OrderState.FILLED
                order.filled_at = time.time()
                order.execution_log.append({
                    "t": time.time(), "event": "filled",
                    "price": price, "volume": fill_vol
                })
                # Cancel OCO sibling on fill
                if order.sibling_id:
                    sibling = self._orders.get(order.sibling_id)
                    if sibling and sibling.state == OrderState.PENDING:
                        sibling.state = OrderState.CANCELLED
                        sibling.updated_at = time.time()
            else:
                order.state = OrderState.PARTIAL
                order.execution_log.append({
                    "t": time.time(), "event": "partial_fill",
                    "price": price, "volume": fill_vol
                })
            order.updated_at = time.time()
            return order

    async def reject_order(self, order_id: str, reason: str = "") -> bool:
        async with self._lock:
            order = self._orders.get(order_id)
            if not order:
                return False
            order.state = OrderState.REJECTED
            order.reject_reason = reason
            order.updated_at = time.time()
        return True

    # ── Position management ──────────────────────────────────────

    async def open_position(self, order_id: str, symbol: str, side: str,
                            volume: float, entry_price: float,
                            sl: float = 0, tp: float = 0,
                            ticket: int = 0) -> dict:
        pos = {
            "ticket": ticket or int(time.time()),
            "symbol": symbol, "side": side, "volume": volume,
            "entry_price": entry_price, "current_price": entry_price,
            "sl": sl, "tp": tp, "pnl": 0.0, "swap": 0.0,
            "order_id": order_id,
            "opened_at": time.time(),
        }
        async with self._lock:
            self._positions[str(pos["ticket"])] = pos
        return pos

    async def close_position(self, ticket: int, exit_price: float,
                             volume: Optional[float] = None) -> Optional[dict]:
        async with self._lock:
            pos = self._positions.get(str(ticket))
            if not pos:
                return None
            close_vol = volume or pos["volume"]
            pos["pnl"] = (exit_price - pos["entry_price"]) * close_vol \
                if pos["side"] == "BUY" else \
                (pos["entry_price"] - exit_price) * close_vol
            pos["exit_price"] = exit_price
            pos["closed_at"] = time.time()
            return pos

    async def update_position_price(self, ticket: int, price: float) -> Optional[dict]:
        async with self._lock:
            pos = self._positions.get(str(ticket))
            if not pos:
                return None
            pos["current_price"] = price
            pos["pnl"] = (price - pos["entry_price"]) * pos["volume"] \
                if pos["side"] == "BUY" else \
                (pos["entry_price"] - price) * pos["volume"]
            # Check trailing stop
            ts = self._trailing_stops.get(ticket)
            if ts and pos["side"] == "BUY":
                new_sl = price - ts["distance"]
                if new_sl > pos["sl"]:
                    pos["sl"] = new_sl
            elif ts and pos["side"] == "SELL":
                new_sl = price + ts["distance"]
                if new_sl < pos["sl"] or pos["sl"] == 0:
                    pos["sl"] = new_sl
            return pos

    async def get_positions(self) -> list[dict]:
        async with self._lock:
            return [p for p in self._positions.values()
                    if "closed_at" not in p]

    async def get_positions_history(self) -> list[dict]:
        async with self._lock:
            return [p for p in self._positions.values()
                    if "closed_at" in p]

    # ── Queries ──────────────────────────────────────────────────

    async def get_order(self, order_id: str) -> Optional[Order]:
        async with self._lock:
            return self._orders.get(order_id)

    async def get_orders(self, state: Optional[OrderState] = None,
                         symbol: str = "", ea_key: str = "",
                         order_type: Optional[OrderType] = None) -> list[dict]:
        async with self._lock:
            result = []
            for o in self._orders.values():
                if state is not None and o.state != state:
                    continue
                if symbol and o.symbol != symbol:
                    continue
                if ea_key and o.ea_key != ea_key:
                    continue
                if order_type is not None and o.order_type != order_type:
                    continue
                result.append(o.to_dict())
            return sorted(result, key=lambda x: x["created_at"], reverse=True)

    async def get_account_summary(self) -> dict:
        async with self._lock:
            open_positions = [p for p in self._positions.values()
                              if "closed_at" not in p]
            total_pnl = sum(p.get("pnl", 0) for p in open_positions)
            return {
                "balance": 10000.0,
                "equity": 10000.0 + total_pnl,
                "margin": 0.0,
                "free_margin": 10000.0 + total_pnl,
                "margin_level": 100.0 if total_pnl == 0 else 999.0,
                "open_positions": len(open_positions),
                "open_orders": sum(1 for o in self._orders.values()
                                   if o.state in (OrderState.PENDING,
                                                  OrderState.SUBMITTED)),
                "total_pnl": total_pnl,
            }

    # ── OTO watcher (call on price update) ───────────────────────

    async def check_oto_triggers(self, symbol: str,
                                 current_price: float) -> list[Order]:
        triggered = []
        async with self._lock:
            remaining = []
            for w in self._oto_watchers:
                if w["symbol"] != symbol:
                    remaining.append(w)
                    continue
                triggered_flag = False
                if w["direction"] == "above" and current_price >= w["price"]:
                    triggered_flag = True
                elif w["direction"] == "below" and current_price <= w["price"]:
                    triggered_flag = True
                if triggered_flag:
                    target = self._orders.get(w["target_id"])
                    if target and target.state == OrderState.PENDING:
                        target.state = OrderState.SUBMITTED
                        target.updated_at = time.time()
                        triggered.append(target)
                else:
                    remaining.append(w)
            self._oto_watchers = remaining
        return triggered


# Singleton
_order_manager: Optional[OrderManager] = None


def get_order_manager() -> OrderManager:
    global _order_manager
    if _order_manager is None:
        _order_manager = OrderManager()
    return _order_manager
