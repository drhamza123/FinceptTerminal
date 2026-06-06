import asyncio
import logging
import time
from typing import List, Dict, Any, Optional
from dataclasses import dataclass, field
import numpy as np

logger = logging.getLogger("guardian.scanner")

SYMBOLS = ["XAUUSD", "XAGUSD", "EURUSD", "BTCUSD", "SPY", "AAPL", "MSFT", "TSLA", "NVDA"]
STRATEGIES = ["ema_crossover", "macd", "rsi", "bollinger"]
TIMEFRAMES = ["H1", "H4", "D1", "W1"]

@dataclass
class ScanResult:
    symbol: str = ""
    strategy: str = ""
    timeframe: str = ""
    params: dict = field(default_factory=dict)
    total_return: float = 0
    profit_factor: float = 0
    sharpe: float = 0
    max_drawdown: float = 0
    win_rate: float = 0
    total_trades: int = 0
    score: float = 0

    def compute_score(self):
        """Score = Sharpe * abs(return) * PF / max_dd."""
        pf = max(self.profit_factor, 0.01)
        dd = max(self.max_drawdown, 0.01)
        ret = abs(self.total_return) if self.total_return > 0 else 0
        self.score = round((self.sharpe * ret * pf) / (dd * 100), 2)


class StrategyScanner:
    def __init__(self):
        self.results: List[ScanResult] = []

    async def scan_all(self, capital: float = 100000,
                       start: str = "2026-01-01",
                       end: str = "2026-06-01") -> List[ScanResult]:
        """Scan all symbols × strategies × timeframes × parameters."""
        import httpx
        from app.config import settings

        tasks = []
        for symbol in SYMBOLS:
            for strategy in STRATEGIES:
                for tf in TIMEFRAMES:
                    params_list = self._param_grid(strategy)
                    for params in params_list:
                        tasks.append(self._run_one(symbol, strategy, tf, params, capital, start, end))

        logger.info(f"Scanner: {len(tasks)} combinations to test")
        done = 0
        for task in asyncio.as_completed(tasks):
            result = await task
            if result:
                self.results.append(result)
            done += 1
            if done % 50 == 0:
                logger.info(f"Scanner progress: {done}/{len(tasks)}")

        self.results.sort(key=lambda r: r.score, reverse=True)
        return self.results

    def _param_grid(self, strategy: str) -> List[dict]:
        if strategy == "ema_crossover":
            return [{"fast_ma": f, "slow_ma": s} for f in [5, 9, 12] for s in [20, 30, 50] if f < s]
        if strategy == "macd":
            return [{"fast_ma": 12, "slow_ma": 26, "signal_ma": 9}]
        if strategy == "rsi":
            return [{"rsi_period": p} for p in [7, 14, 21]]
        if strategy == "bollinger":
            return [{"bb_period": 20, "bb_std": 2.0}]
        return [{}]

    async def _run_one(self, symbol: str, strategy: str, tf: str,
                       params: dict, capital: float, start: str, end: str) -> Optional[ScanResult]:
        import httpx
        try:
            body = {
                "symbols": symbol, "capital": capital,
                "start_date": start, "end_date": end,
                "strategy": strategy, "timeframe": tf,
                **params
            }
            async with httpx.AsyncClient(timeout=30, verify=False) as client:
                resp = await client.post(
                    "http://localhost:8150/backtest/run",
                    json=body,
                    headers={"x-api-key": "scan", "x-session-token": "scan"}
                )
                if resp.status_code != 200:
                    return None
                data = resp.json().get("data", {})
                if not data or data.get("total_trades", 0) < 3:
                    return None

                result = ScanResult(
                    symbol=symbol, strategy=strategy, timeframe=tf,
                    params=params,
                    total_return=data.get("total_return", 0),
                    profit_factor=data.get("profit_factor", 0),
                    sharpe=data.get("sharpe", 0),
                    max_drawdown=abs(data.get("max_drawdown", 0)),
                    win_rate=data.get("win_rate", 0),
                    total_trades=data.get("total_trades", 0),
                )
                result.compute_score()
                return result
        except Exception as e:
            logger.debug(f"Scan error {symbol} {strategy}: {e}")
            return None

    def top_n(self, n: int = 10) -> List[ScanResult]:
        return self.results[:n]

    def best_portfolio(self, max_strategies: int = 5) -> List[ScanResult]:
        """Pick top uncorrelated strategies."""
        selected = []
        for r in self.results:
            if len(selected) >= max_strategies:
                break
            # Skip if same symbol + similar return direction
            if any(s.symbol == r.symbol and s.strategy == r.strategy for s in selected):
                continue
            selected.append(r)
        return selected

    def generate_mql5(self, result: ScanResult) -> str:
        """Generate MQL5 EA code from scan result."""
        strategy = result.strategy
        params = result.params
        symbol = result.symbol
        tf_map = {"H1": "PERIOD_H1", "H4": "PERIOD_H4", "D1": "PERIOD_D1", "W1": "PERIOD_W1"}

        code = f"""//+------------------------------------------------------------------+
//|                                                 {symbol}_{strategy}.mq5 |
//|                          Auto-generated by FinceptTerminal Scanner |
//+------------------------------------------------------------------+
#property strict
#property copyright "FinceptTerminal AI Scanner"
#property version   "1.00"
#property description "Strategy: {strategy} on {symbol} {result.timeframe}"
"""
        code += "// Parameters from optimization:\n"
        for k, v in params.items():
            code += f"//   {k} = {v}\n"

        code += f"""
input double InpLotSize = 0.1;
input double InpRiskPercent = 2.0;
int h_ma_fast, h_ma_slow;
double buf_fast[], buf_slow[];

int OnInit() {{
"""
        if strategy == "ema_crossover":
            fast = params.get("fast_ma", 9)
            slow = params.get("slow_ma", 21)
            code += f"""    h_ma_fast = iMA(Symbol(), {tf_map.get(result.timeframe, "PERIOD_H1")}, {fast}, 0, MODE_EMA, PRICE_CLOSE);
    h_ma_slow = iMA(Symbol(), {tf_map.get(result.timeframe, "PERIOD_H1")}, {slow}, 0, MODE_EMA, PRICE_CLOSE);
"""
        code += """    if (h_ma_fast == INVALID_HANDLE || h_ma_slow == INVALID_HANDLE)
        return INIT_FAILED;
    ArraySetAsSeries(buf_fast, true);
    ArraySetAsSeries(buf_slow, true);
    return INIT_SUCCEEDED;
}

void OnTick() {
    CopyBuffer(h_ma_fast, 0, 0, 3, buf_fast);
    CopyBuffer(h_ma_slow, 0, 0, 3, buf_slow);
"""
        if strategy == "ema_crossover":
            code += """    if (buf_fast[1] <= buf_slow[1] && buf_fast[0] > buf_slow[0])
        Print("BUY signal");
    if (buf_fast[1] >= buf_slow[1] && buf_fast[0] < buf_slow[0])
        Print("SELL signal");
"""
        code += "}\n//+------------------------------------------------------------------+\n"
        return code

scanner = StrategyScanner()
