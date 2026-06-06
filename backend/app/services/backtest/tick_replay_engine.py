from typing import Iterator, Callable, Dict, Any, Optional
from .data_loader import Tick
from .market_simulator import MarketSimulator
from .cointegration_engine import RollingCointegration

class StreamingReplayEngine:
    def __init__(self, strategy, cash: float = 1_000_000, latency_ms: float = 15,
                 slippage_bps: float = 1.5, on_update: Callable = None):
        self.sim = MarketSimulator(cash, latency_ms, slippage_bps)
        self.coint = RollingCointegration()
        self.strategy = strategy
        self.on_update = on_update

    def run(self, stream: Iterator[Tick]) -> Dict[str, Any]:
        if hasattr(self.strategy, 'init'):
            self.strategy.init(self.sim.cash)
        count = 0
        last_ts = 0
        for tick in stream:
            last_ts = tick.timestamp_ms
            self.sim.on_tick(tick, tick.timestamp_ms)
            self.coint.on_tick(tick.timestamp_ms, tick.symbol, (tick.bid + tick.ask) / 2)
            st = self.sim.state()
            z = 0.0
            if self.coint.is_cointegrated:
                prices = list(st.prices.values())
                if len(prices) >= 2:
                    z = self.coint.z_score(prices[0], prices[1])
            self.strategy.on_tick(tick.timestamp_ms, tick, st, self.coint, self.sim)
            count += 1
            if self.on_update and count % 50_000 == 0:
                self.on_update({"type": "PROGRESS", "ts": tick.timestamp_ms,
                                "equity": st.equity, "z_score": round(z, 3),
                                "trades": len(self.sim.trades)})
        if self.on_update:
            self.on_update({"type": "DONE", "final_equity": self.sim.state().equity,
                            "trades": len(self.sim.trades)})
        return {"trades": len(self.sim.trades), "final_equity": self.sim.state().equity}
