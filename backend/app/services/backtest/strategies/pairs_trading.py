from ..data_loader import Tick
from ..market_simulator import PortfolioState, MarketSimulator, Side
from ..cointegration_engine import RollingCointegration
from typing import Optional

class PairsTrading:
    def __init__(self, a: str, b: str, entry_z: float = 2.0, exit_z: float = 0.5):
        self.a = a
        self.b = b
        self.entry_z = entry_z
        self.exit_z = exit_z
        self.pos = 0

    def init(self, cash: float):
        pass

    def on_tick(self, ts: float, tick: Tick, state: PortfolioState,
                coint: RollingCointegration, broker: MarketSimulator):
        if not coint.is_cointegrated:
            return
        pa = state.prices.get(self.a, 0)
        pb = state.prices.get(self.b, 0)
        if pa <= 0 or pb <= 0:
            return
        z = coint.z_score(pa, pb)
        if self.pos == 0:
            if z > self.entry_z:
                broker.submit(self.a, Side.SELL, 100, ts)
                broker.submit(self.b, Side.BUY, 100, ts)
                self.pos = -1
            elif z < -self.entry_z:
                broker.submit(self.a, Side.BUY, 100, ts)
                broker.submit(self.b, Side.SELL, 100, ts)
                self.pos = 1
        elif self.pos == -1 and z <= self.exit_z:
            broker.submit(self.a, Side.BUY, 100, ts)
            broker.submit(self.b, Side.SELL, 100, ts)
            self.pos = 0
        elif self.pos == 1 and z >= -self.exit_z:
            broker.submit(self.a, Side.SELL, 100, ts)
            broker.submit(self.b, Side.BUY, 100, ts)
            self.pos = 0
