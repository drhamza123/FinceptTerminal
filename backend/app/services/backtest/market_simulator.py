from typing import Dict, List, Optional
from .data_loader import Tick
from dataclasses import dataclass, field
from enum import Enum
import uuid

class Side(str, Enum):
    BUY = "BUY"
    SELL = "SELL"

@dataclass
class Order:
    order_id: str
    symbol: str
    side: Side
    qty: float
    ts_submit: float = 0

@dataclass
class Fill:
    order_id: str; symbol: str; side: Side; qty: float
    fill_price: float; ts_fill: float; latency_ms: float

@dataclass
class PortfolioState:
    cash: float
    positions: Dict[str, float]
    equity: float
    prices: Dict[str, float]

class MarketSimulator:
    def __init__(self, cash: float = 1_000_000, latency_ms: float = 15, slippage_bps: float = 1.5):
        self.cash = cash
        self.latency_ms = latency_ms
        self.slippage_bps = slippage_bps
        self.positions: Dict[str, float] = {}
        self.prices: Dict[str, float] = {}
        self.pending: List[Order] = []
        self.trades: List[Fill] = []
        self.equity: List[tuple] = []

    def submit(self, symbol: str, side: Side, qty: float, ts: float):
        self.pending.append(Order(str(uuid.uuid4())[:8], symbol, side, qty, ts))

    def on_tick(self, tick: Tick, ts: float):
        self.prices[tick.symbol] = (tick.bid + tick.ask) / 2
        remaining = []
        for o in self.pending:
            if o.symbol != tick.symbol or ts < o.ts_submit + self.latency_ms:
                remaining.append(o)
                continue
            price = tick.ask if o.side == Side.BUY else tick.bid
            slip = 1 + self.slippage_bps / 10000
            price = price * slip if o.side == Side.BUY else price / slip
            cost = o.qty * price
            if o.side == Side.BUY:
                self.cash -= cost
                self.positions[o.symbol] = self.positions.get(o.symbol, 0) + o.qty
            else:
                self.cash += cost
                self.positions[o.symbol] = self.positions.get(o.symbol, 0) - o.qty
            self.trades.append(Fill(o.order_id, o.symbol, o.side, o.qty, price, ts, ts - o.ts_submit))
        self.pending = remaining
        self.equity.append((ts, self._equity()))

    def _equity(self) -> float:
        return self.cash + sum(pos * self.prices.get(sym, 0) for sym, pos in self.positions.items())

    def state(self) -> PortfolioState:
        return PortfolioState(self.cash, self.positions.copy(), self._equity(), self.prices.copy())
