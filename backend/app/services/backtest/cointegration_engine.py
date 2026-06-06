import numpy as np
from typing import Dict, Optional

class RollingCointegration:
    def __init__(self, window: int = 252, bar_ms: int = 60_000):
        self.window = window
        self.bar_ms = bar_ms
        self.bar_id = -1
        self.bar_prices: Dict[str, float] = {}
        self.last: Dict[str, float] = {}
        self.history: list = []
        self.beta = 1.0
        self.spread_mean = 0.0
        self.spread_std = 1.0
        self.is_cointegrated = False

    def on_tick(self, ts: float, symbol: str, price: float):
        self.last[symbol] = price
        bid = int(ts // self.bar_ms)
        if bid > self.bar_id:
            if self.bar_id != -1:
                self._close_bar()
            self.bar_id = bid
            self.bar_prices = {}
        self.bar_prices[symbol] = price

    def _close_bar(self):
        snap = {s: self.bar_prices.get(s, self.last[s]) for s in self.last}
        self.history.append(snap)
        if len(self.history) > self.window:
            self.history.pop(0)
        if len(self.history) >= self.window:
            self._egranger()

    def _egranger(self):
        syms = list(self.bar_prices.keys())
        if len(syms) < 2:
            return
        y = np.array([h[syms[0]] for h in self.history], dtype=float)
        x = np.array([h[syms[1]] for h in self.history], dtype=float)
        if np.std(x) < 1e-10:
            return
        self.beta = np.cov(x, y)[0, 1] / np.var(x)
        spread = y - self.beta * x
        self.spread_mean = np.mean(spread)
        self.spread_std = np.std(spread)
        if self.spread_std < 1e-10:
            return
        ar = np.polyfit(np.arange(len(spread)), spread, 1)
        resid = spread - np.polyval(ar, np.arange(len(spread)))
        if np.std(resid) < 1e-10:
            return
        rho = np.corrcoef(resid[:-1], resid[1:])[0, 1]
        se = np.std(resid[1:] - rho * resid[:-1]) / np.std(resid[:-1]) if np.std(resid[:-1]) > 0 else 99
        t_stat = (rho - 1) / se if se > 0 else 0
        self.is_cointegrated = t_stat < -2.86  # ~95% critical value for ADF(0) with drift

    def z_score(self, pa: float, pb: float) -> float:
        if not self.is_cointegrated or self.spread_std < 1e-10:
            return 0.0
        return (pa - self.beta * pb - self.spread_mean) / self.spread_std
