from typing import Dict, Optional
import numpy as np

class ResamplingCorrelation:
    def __init__(self, bar_ms: int = 1000, window: int = 300):
        self.bar_ms = bar_ms
        self.window = window
        self.bar_id = -1
        self.bar_prices: Dict[str, float] = {}
        self.last: Dict[str, float] = {}
        self.history: list = []

    def on_tick(self, ts: float, symbol: str, price: float):
        self.last[symbol] = price
        bid = int(ts // self.bar_ms)
        if bid > self.bar_id:
            if self.bar_id != -1:
                snap = {s: self.bar_prices.get(s, self.last[s]) for s in self.last}
                self.history.append(snap)
                if len(self.history) > self.window:
                    self.history.pop(0)
            self.bar_id = bid
            self.bar_prices = {}
        self.bar_prices[symbol] = price

    def corr(self) -> Optional[Dict]:
        if len(self.history) < 5:
            return None
        import pandas as pd
        df = pd.DataFrame(self.history)
        logr = np.log(df / df.shift(1)).dropna()
        if len(logr) < 2:
            return None
        c = logr.corr()
        return c.to_dict() if c is not None else None
