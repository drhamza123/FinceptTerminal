"""Vectorized multi-strategy backtester with NumPy/Pandas + multi-core support.
Now with spread, slippage, and commission modeling for realistic P&L.
"""
import concurrent.futures
import logging
import math
import random
from datetime import datetime, timezone

import numpy as np
import pandas as pd
from fastapi import APIRouter, Depends, WebSocket, WebSocketDisconnect

from app.models.user import User
from app.routers.auth import resolve_user

logger = logging.getLogger("guardian.backtest")
router = APIRouter(tags=["backtest"])

# ── Realistic Market Friction Defaults ───────────────────────────
# Spread in price units (half-spread applied each side).
# Typical values: XAUUSD=0.5, EURUSD=0.00015, BTCUSD=50, AAPL=0.02
DEFAULT_SPREAD_MAP = {
    "XAUUSD": 0.5, "XAGUSD": 0.03, "XPTUSD": 2.0,
    "WTI": 0.05, "BRENT": 0.05,
    "EURUSD": 0.00015, "GBPUSD": 0.0002, "USDJPY": 0.03, "AUDUSD": 0.0002,
    "USDCAD": 0.0002, "NZDUSD": 0.0002,
    "BTCUSD": 50.0, "ETHUSD": 3.0,
}

# Slippage as fraction of price (0.01% = 0.0001).
# Applied randomly to each exit with uniform distribution in [0, max_slip]
DEFAULT_SLIPPAGE_PCT = 0.0005  # 0.05%

# Commission per lot per side (USD). Round-turn = 2x.
# Typical: futures $5-8/lot, forex $3-7/lot, stocks $0.005/share
DEFAULT_COMMISSION_PER_LOT = 7.0  # USD per lot per side

SYMBOLS = {
    "XAUUSD": "GC=F", "XAGUSD": "SI=F", "XPTUSD": "PL=F",
    "WTI": "CL=F", "BRENT": "BZ=F",
    "EURUSD": "EURUSD=X", "GBPUSD": "GBPUSD=X", "USDJPY": "JPY=X",
    "AUDUSD": "AUDUSD=X", "USDCAD": "USDCAD=X", "NZDUSD": "NZDUSD=X",
    "BTCUSD": "BTC-USD", "ETHUSD": "ETH-USD",
    "AAPL": "AAPL", "MSFT": "MSFT", "GOOGL": "GOOGL",
    "AMZN": "AMZN", "TSLA": "TSLA", "META": "META", "NVDA": "NVDA",
}
TF_MAP = {"M1":"1m","M5":"5m","M15":"15m","M30":"30m","H1":"1h","H4":"4h","D1":"1d","W1":"1wk"}
PERIOD_MAP = {"1m":"5d","5m":"1mo","15m":"1mo","30m":"3mo","1h":"6mo","4h":"1y","1d":"2y","1wk":"5y"}


# ── Vectorized Indicators ───────────────────────────────────────

def _ema(arr: np.ndarray, period: int) -> np.ndarray:
    """Vectorized EMA using pandas."""
    s = pd.Series(arr)
    return s.ewm(span=period, adjust=False).mean().to_numpy()

def _sma(arr: np.ndarray, period: int) -> np.ndarray:
    """Vectorized SMA."""
    s = pd.Series(arr)
    return s.rolling(period).mean().to_numpy()

def _rsi(arr: np.ndarray, period: int = 14) -> np.ndarray:
    """Vectorized RSI. Returns same length as input (nan-padded at start)."""
    diff = np.diff(arr)
    gains = np.where(diff > 0, diff, 0)
    losses = np.where(diff < 0, -diff, 0)
    avg_g = pd.Series(gains).ewm(span=period, adjust=False).mean().to_numpy()
    avg_l = pd.Series(losses).ewm(span=period, adjust=False).mean().to_numpy()
    rs = np.divide(avg_g, avg_l, out=np.ones_like(avg_g) * 100, where=avg_l != 0)
    rsi = 100 - (100 / (1 + rs))
    # Pad with nans at the beginning to match input length
    if len(rsi) < len(arr):
        rsi = np.concatenate([np.full(len(arr) - len(rsi), np.nan), rsi])
    return rsi

def _macd(arr: np.ndarray, fast=12, slow=26, signal=9):
    """Vectorized MACD."""
    ema_f = _ema(arr, fast)
    ema_s = _ema(arr, slow)
    macd_line = ema_f - ema_s
    sig_line = _ema(macd_line, signal)
    hist = macd_line - sig_line
    return macd_line, sig_line, hist

def _bollinger(arr: np.ndarray, period=20, std_dev=2.0):
    """Vectorized Bollinger Bands."""
    ma = _sma(arr, period)
    rolling_std = pd.Series(arr).rolling(period).std().to_numpy()
    upper = ma + std_dev * rolling_std
    lower = ma - std_dev * rolling_std
    return upper, ma, lower

def _atr(high: np.ndarray, low: np.ndarray, close: np.ndarray, period=14) -> np.ndarray:
    """Vectorized ATR."""
    prev_close = np.roll(close, 1)
    prev_close[0] = close[0]
    tr = np.maximum(high - low,
                    np.maximum(np.abs(high - prev_close),
                               np.abs(low - prev_close)))
    return pd.Series(tr).ewm(span=period, adjust=False).mean().to_numpy()

def _supertrend(high: np.ndarray, low: np.ndarray, close: np.ndarray,
                period=10, multiplier=3.0) -> np.ndarray:
    """Vectorized SuperTrend direction: 1=up, -1=down, 0=none."""
    atr = _atr(high, low, close, period)
    hl_avg = (high + low) / 2
    upper = hl_avg + multiplier * atr
    lower = hl_avg - multiplier * atr
    st = np.zeros_like(close)
    st[close > upper] = 1
    st[close < lower] = -1
    return st


# ── Strategy Signal Generators (Vectorized) ─────────────────────

def _signal_ema_crossover(close: np.ndarray, fast=9, slow=21) -> np.ndarray:
    """1 = BUY, -1 = SELL, 0 = HOLD, nan = no signal."""
    ema_f = _ema(close, fast)
    ema_s = _ema(close, slow)
    prev_f = np.roll(ema_f, 1)
    prev_s = np.roll(ema_s, 1)
    sig = np.zeros_like(close)
    cross_above = (prev_f <= prev_s) & (ema_f > ema_s)
    cross_below = (prev_f >= prev_s) & (ema_f < ema_s)
    sig[cross_above] = 1
    sig[cross_below] = -1
    sig[np.isnan(ema_f) | np.isnan(ema_s)] = 0
    return sig

def _signal_rsi(close: np.ndarray, period=14, oversold=30, overbought=70) -> np.ndarray:
    """RSI mean reversion signals."""
    rsi = _rsi(close, period)
    sig = np.zeros_like(close)
    sig[rsi < oversold] = 1
    sig[rsi > overbought] = -1
    sig[np.isnan(rsi)] = 0
    return sig

def _signal_macd(close: np.ndarray, fast=12, slow=26, signal=9) -> np.ndarray:
    """MACD crossover signals."""
    macd_line, sig_line, _ = _macd(close, fast, slow, signal)
    prev_macd = np.roll(macd_line, 1)
    prev_sig = np.roll(sig_line, 1)
    sig = np.zeros_like(close)
    cross_above = (prev_macd <= prev_sig) & (macd_line > sig_line)
    cross_below = (prev_macd >= prev_sig) & (macd_line < sig_line)
    sig[cross_above] = 1
    sig[cross_below] = -1
    sig[np.isnan(macd_line) | np.isnan(sig_line)] = 0
    return sig

def _signal_bollinger(close: np.ndarray, period=20, std=2.0) -> np.ndarray:
    """Bollinger band touch signals."""
    upper, mid, lower = _bollinger(close, period, std)
    sig = np.zeros_like(close)
    sig[close <= lower] = 1
    sig[close >= upper] = -1
    sig[np.isnan(upper) | np.isnan(lower)] = 0
    return sig

def _signal_supertrend(high: np.ndarray, low: np.ndarray, close: np.ndarray,
                       period=10, multiplier=3.0) -> np.ndarray:
    """SuperTrend breakout signals."""
    st = _supertrend(high, low, close, period, multiplier)
    prev_st = np.roll(st, 1)
    prev_st[0] = 0
    sig = np.zeros_like(close)
    sig[(prev_st <= 0) & (st > 0)] = 1
    sig[(prev_st >= 0) & (st < 0)] = -1
    return sig


# ── Main Vectorized Backtest Engine ─────────────────────────────

def _apply_spread(price: float, direction: str, spread: float) -> float:
    """Apply half-spread: buy at ask (price + spread/2), sell at bid (price - spread/2)."""
    if spread <= 0:
        return price
    return price + (spread / 2) if direction == "BUY" else price - (spread / 2)


def _apply_slippage(price: float, slip_pct: float) -> float:
    """Apply random slippage as a fraction of price.
    Returns price adjusted by [0, slip_pct] in a random direction.
    """
    if slip_pct <= 0:
        return price
    slip = price * slip_pct * random.uniform(0, 1)
    direction = 1 if random.random() < 0.5 else -1
    return price + slip * direction


def _apply_commission(lot: float, commission_per_lot: float) -> float:
    """Calculate commission cost for one side of a trade."""
    return lot * commission_per_lot


def _compute_pnl(entry: float, exit_: float, lot: float, side: str,
                 spread: float, slip_pct: float, commission_per_lot: float,
                 timestamps_i: int, reason: str) -> tuple:
    """Compute realistic P&L with spread, slippage, and commission.
    Returns (pnl, commission_cost, spread_cost, slippage_cost, adj_entry, adj_exit).
    """
    # Entry price: apply half-spread (buy pays ask, sell receives bid)
    adj_entry = _apply_spread(entry, side, spread)

    # Exit price: apply half-spread opposite + random slippage
    exit_side = "SELL" if side == "BUY" else "BUY"
    adj_exit = _apply_spread(exit_, exit_side, spread)
    adj_exit = _apply_slippage(adj_exit, slip_pct)

    # Raw P&L
    multiplier = 1 if side == "BUY" else -1
    gross_pnl = (adj_exit - adj_entry) * lot * 100 * multiplier

    # Costs
    entry_comm = _apply_commission(lot, commission_per_lot)
    exit_comm = _apply_commission(lot, commission_per_lot)
    total_comm = entry_comm + exit_comm

    # Track spread cost separately
    raw_entry = entry
    spread_cost = abs(adj_entry - raw_entry) * lot * 100 * 2  # both sides
    slippage_cost = abs(adj_exit - exit_) * lot * 100

    net_pnl = gross_pnl - total_comm

    return net_pnl, total_comm, spread_cost, slippage_cost, adj_entry, adj_exit


_DEFAULT_SPREAD = 0.0
_DEFAULT_SLIPPAGE = 0.0005
_DEFAULT_COMMISSION = 7.0


def _run_backtest_vectorized(df: pd.DataFrame, strategy: str = "ema_crossover",
                              fast=9, slow=21, initial_balance=10000, risk_pct=2.0,
                              rsi_period=14, rsi_oversold=30, rsi_overbought=70,
                              macd_fast=12, macd_slow=26, macd_signal=9,
                              bb_period=20, bb_std=2.0,
                              st_period=10, st_multiplier=3.0,
                              spread: float = _DEFAULT_SPREAD,
                              slippage_pct: float = _DEFAULT_SLIPPAGE,
                              commission_per_lot: float = _DEFAULT_COMMISSION) -> dict:
    """Vectorized backtest with realistic spread, slippage, and commission modeling.

    - spread: half-spread added to buys, subtracted from sells (price units)
    - slippage_pct: random slippage fraction applied to exits (0.05% = 0.0005)
    - commission_per_lot: USD per lot per side (round-turn = 2x)
    """
    o = df["Open"].to_numpy(dtype=np.float64)
    h = df["High"].to_numpy(dtype=np.float64)
    l = df["Low"].to_numpy(dtype=np.float64)
    c = df["Close"].to_numpy(dtype=np.float64)
    v = df["Volume"].to_numpy(dtype=np.float64) if "Volume" in df.columns else np.ones_like(c)
    n = len(c)

    # Generate signals (once, vectorized)
    signal = np.zeros(n, dtype=np.int8)
    if strategy in ("ema_crossover", "all"):
        sig = _signal_ema_crossover(c, fast, slow)
        signal = np.where(sig != 0, sig, signal)
    if strategy in ("rsi", "all"):
        sig = _signal_rsi(c, rsi_period, rsi_oversold, rsi_overbought)
        signal = np.where(sig != 0, sig, signal)
    if strategy in ("macd", "all"):
        sig = _signal_macd(c, macd_fast, macd_slow, macd_signal)
        signal = np.where(sig != 0, sig, signal)
    if strategy in ("bollinger", "all"):
        sig = _signal_bollinger(c, bb_period, bb_std)
        signal = np.where(sig != 0, sig, signal)
    if strategy in ("supertrend", "all"):
        sig = _signal_supertrend(h, l, c, st_period, st_multiplier)
        signal = np.where(sig != 0, sig, signal)

    # Precompute ATR for risk sizing
    atr = _atr(h, l, c, 14)
    timestamps = df.index.astype(np.int64) // 10**9

    # Track cumulative costs
    total_commission_paid = 0.0
    total_spread_paid = 0.0
    total_slippage_paid = 0.0

    # Simulation loop (positions must be sequential)
    equity_curve = np.full(n, initial_balance, dtype=np.float64)
    balance_curve = np.full(n, initial_balance, dtype=np.float64)
    balance = float(initial_balance)
    trades: list[dict] = []
    positions: list[dict] = []
    min_bars = max(50, fast, slow, 26, 20, 14, rsi_period, macd_slow,
                   bb_period, st_period)

    for i in range(min_bars, n):
        cp = c[i]
        sl_dist = atr[i] * 1.5 if not np.isnan(atr[i]) else cp * 0.01

        sig_val = signal[i]
        if sig_val != 0:
            direction = "BUY" if sig_val > 0 else "SELL"
            risk_amount = balance * risk_pct / 100
            lot = max(0.01, min(10, round(risk_amount / (sl_dist * 10) if sl_dist > 0 else 0.01, 2)))

            # Close opposite positions first (entry costs already accounted)
            opposite = [p for p in positions if p["side"] != direction]
            for p in opposite:
                exit_side = "SELL" if p["side"] == "BUY" else "BUY"
                adj_exit = _apply_spread(cp, exit_side, spread)
                adj_exit = _apply_slippage(adj_exit, slippage_pct)
                if p["side"] == "BUY":
                    gross_pnl = (adj_exit - p["entry"]) * p["lot"] * 100
                else:
                    gross_pnl = (p["entry"] - adj_exit) * p["lot"] * 100
                exit_comm = _apply_commission(p["lot"], commission_per_lot)
                net_pnl = gross_pnl - exit_comm
                # Exit costs relative to trigger price (cp = market price acting as trigger)
                raw_trigger = cp
                exit_impact = abs(adj_exit - raw_trigger) * p["lot"] * 100
                half_spread_amt = (spread / 2) * p["lot"] * 100
                exit_spread_cost = min(half_spread_amt, exit_impact) if spread > 0 else 0
                exit_slip_cost = max(0, exit_impact - exit_spread_cost)
                balance += net_pnl
                total_commission_paid += exit_comm
                total_spread_paid += exit_spread_cost
                total_slippage_paid += exit_slip_cost
                trades.append({
                    "time": int(timestamps[i]), "side": p["side"],
                    "entry": round(p["entry"], 4), "exit": round(adj_exit, 4),
                    "pnl": round(net_pnl, 2), "lot": p["lot"],
                    "commission": round(exit_comm, 2),
                    "spread_cost": round(exit_spread_cost, 2),
                    "slippage_cost": round(exit_slip_cost, 2),
                    "reason": p["reason"] + " (reversed)"
                })
                positions.remove(p)

            if len(positions) < 3:
                # Apply spread to entry price
                adj_entry = _apply_spread(cp, direction, spread)
                sl = adj_entry - sl_dist if direction == "BUY" else adj_entry + sl_dist
                tp = adj_entry + sl_dist * 2 if direction == "BUY" else adj_entry - sl_dist * 2
                positions.append({
                    "side": direction, "entry": adj_entry, "entry_raw": cp, "sl": sl, "tp": tp,
                    "lot": lot, "reason": f"Signal {strategy}", "time": int(timestamps[i])
                })
                # Entry commission
                entry_comm = _apply_commission(lot, commission_per_lot)
                total_commission_paid += entry_comm
                balance -= entry_comm

        # Check exits (with realistic frictions)
        for p in positions[:]:
            pnl = 0.0
            closed = False
            trigger_price = None
            adj_exit = cp  # default fallback
            exit_reason = p["reason"]

            if p["side"] == "BUY":
                if cp >= p["tp"]:
                    trigger_price = p["tp"]
                    adj_exit = _apply_spread(p["tp"], "SELL", spread)
                    adj_exit = _apply_slippage(adj_exit, slippage_pct)
                    pnl = (adj_exit - p["entry"]) * p["lot"] * 100
                    closed = True
                elif cp <= p["sl"]:
                    trigger_price = p["sl"]
                    adj_exit = _apply_spread(p["sl"], "SELL", spread)
                    adj_exit = _apply_slippage(adj_exit, slippage_pct)
                    pnl = (adj_exit - p["entry"]) * p["lot"] * 100
                    closed = True
            else:
                if cp <= p["tp"]:
                    trigger_price = p["tp"]
                    adj_exit = _apply_spread(p["tp"], "BUY", spread)
                    adj_exit = _apply_slippage(adj_exit, slippage_pct)
                    pnl = (p["entry"] - adj_exit) * p["lot"] * 100
                    closed = True
                elif cp >= p["sl"]:
                    trigger_price = p["sl"]
                    adj_exit = _apply_spread(p["sl"], "BUY", spread)
                    adj_exit = _apply_slippage(adj_exit, slippage_pct)
                    pnl = (p["entry"] - adj_exit) * p["lot"] * 100
                    closed = True

            if closed and trigger_price is not None:
                exit_comm = _apply_commission(p["lot"], commission_per_lot)
                net_pnl = pnl - exit_comm
                # Entry spread: adjusted entry vs raw signal price
                raw_entry = p.get("entry_raw", p["entry"])
                entry_sprd = abs(p["entry"] - raw_entry) * p["lot"] * 100
                # Exit spread: adjusted exit vs raw trigger price (half-spread on exit side)
                raw_trigger = trigger_price
                exit_sprd_half = abs(adj_exit - raw_trigger) * p["lot"] * 100
                # Of the total exit impact, half-spread is the fixed spread cost
                half_spread_amt = (spread / 2) * p["lot"] * 100
                exit_spread_cost = min(half_spread_amt, exit_sprd_half) if spread > 0 else 0
                exit_slip_cost = max(0, exit_sprd_half - exit_spread_cost)
                sprd_cost = entry_sprd + exit_spread_cost
                slip_cost = exit_slip_cost
                balance += net_pnl
                total_commission_paid += exit_comm
                total_spread_paid += sprd_cost
                total_slippage_paid += slip_cost
                trades.append({
                    "time": int(timestamps[i]), "side": p["side"],
                    "entry": round(p["entry"], 4),
                    "exit": round(adj_exit, 4),
                    "pnl": round(net_pnl, 2), "lot": p["lot"],
                    "commission": round(exit_comm, 2),
                    "spread_cost": round(sprd_cost, 2),
                    "slippage_cost": round(slip_cost, 2),
                    "reason": exit_reason
                })
                positions.remove(p)

        m2m = balance + sum(
            (cp - p["entry"]) * p["lot"] * 100 if p["side"] == "BUY"
            else (p["entry"] - cp) * p["lot"] * 100
            for p in positions
        )
        equity_curve[i] = m2m
        balance_curve[i] = balance

    # Close remaining
    final_idx = n - 1
    for p in positions:
        cp_final = c[final_idx]
        raw_entry = p.get("entry_raw", p["entry"])
        pnl, comm, sprd, slip, adj_entry, adj_exit = _compute_pnl(
            raw_entry, cp_final, p["lot"], p["side"],
            spread, slippage_pct, commission_per_lot,
            int(timestamps[final_idx]), p["reason"] + " (close)"
        )
        balance += pnl
        total_commission_paid += comm
        total_spread_paid += sprd
        total_slippage_paid += slip
        trades.append({
            "time": int(timestamps[final_idx]), "side": p["side"],
            "entry": round(adj_entry, 4), "exit": round(adj_exit, 4),
            "pnl": round(pnl, 2), "lot": p["lot"],
            "commission": round(comm, 2),
            "spread_cost": round(sprd, 2),
            "slippage_cost": round(slip, 2),
            "reason": p["reason"] + " (close)"
        })

    return _calc_metrics_vectorized(balance, initial_balance, trades, equity_curve,
                                     total_commission_paid, total_spread_paid, total_slippage_paid)


def _calc_metrics_vectorized(balance, initial_balance, trades, equity_curve,
                             total_commission=0.0, total_spread=0.0, total_slippage=0.0):
    """Vectorized metrics computation."""
    total_return = ((balance - initial_balance) / initial_balance) * 100
    total_pnl = sum(t["pnl"] for t in trades) if trades else 0
    winning = [t for t in trades if t["pnl"] > 0]
    losing = [t for t in trades if t["pnl"] <= 0]
    win_rate = len(winning) / len(trades) if trades else 0
    avg_win = sum(t["pnl"] for t in winning) / len(winning) if winning else 0
    avg_loss = sum(t["pnl"] for t in losing) / len(losing) if losing else 0

    # Vectorized drawdown
    eq = np.array(equity_curve, dtype=np.float64)
    peak = np.maximum.accumulate(eq)
    dd = (peak - eq) / peak * 100
    max_dd = float(np.max(dd)) if len(dd) > 0 else 0

    # Vectorized Sharpe
    eq_rets = np.diff(eq) / eq[:-1]
    avg_ret = float(np.mean(eq_rets)) if len(eq_rets) > 0 else 0
    std_ret = float(np.std(eq_rets, ddof=1)) if len(eq_rets) > 1 else 1
    sharpe = (avg_ret / std_ret) * math.sqrt(252) if std_ret > 0 else 0

    gross_profit = sum(t["pnl"] for t in trades if t["pnl"] > 0)
    gross_loss = abs(sum(t["pnl"] for t in trades if t["pnl"] < 0))
    profit_factor = gross_profit / gross_loss if gross_loss > 0 else float('inf')

    monthly = {}
    for t in trades:
        dt = datetime.fromtimestamp(t["time"])
        key = dt.strftime("%Y-%m")
        monthly[key] = monthly.get(key, 0) + t["pnl"]

    return {
        "total_return": round(total_return, 2),
        "total_pnl": round(total_pnl, 2),
        "sharpe_ratio": round(sharpe, 2),
        "max_drawdown": round(max_dd, 2),
        "win_rate": round(win_rate, 2),
        "total_trades": len(trades),
        "profitable_trades": len(winning),
        "avg_win": round(avg_win, 2),
        "avg_loss": round(avg_loss, 2),
        "profit_factor": round(profit_factor, 2) if profit_factor != float('inf') else 0,
        "final_balance": round(balance, 2),
        "equity_curve": [round(float(v), 2) for v in equity_curve],
        "balance_curve": [round(float(balance), 2) for _ in equity_curve],
        "trades": trades[-20:],
        "monthly_returns": monthly,
        "cost_analysis": {
            "total_commission": round(total_commission, 2),
            "total_spread_cost": round(total_spread, 2),
            "total_slippage_cost": round(total_slippage, 2),
            "total_costs": round(total_commission + total_spread + total_slippage, 2),
        },
    }


# ── Picklable wrapper for multiprocessing ───────────────────────

def _run_single_backtest(cfg: dict) -> dict:
    """Picklable wrapper for ProcessPoolExecutor."""
    df = cfg["df"]
    result = _run_backtest_vectorized(
        df, strategy=cfg.get("strategy", "ema_crossover"),
        fast=cfg.get("fast_period", 9), slow=cfg.get("slow_period", 21),
        initial_balance=float(cfg.get("initial_balance", 10000)),
        risk_pct=float(cfg.get("risk_percent", 2.0)),
        rsi_period=cfg.get("rsi_period", 14),
        rsi_oversold=cfg.get("rsi_oversold", 30),
        rsi_overbought=cfg.get("rsi_overbought", 70),
        macd_fast=cfg.get("macd_fast", 12),
        macd_slow=cfg.get("macd_slow", 26),
        macd_signal=cfg.get("macd_signal", 9),
        bb_period=cfg.get("bb_period", 20),
        bb_std=cfg.get("bb_std", 2.0),
        st_period=cfg.get("st_period", 10),
        st_multiplier=cfg.get("st_multiplier", 3.0),
        spread=float(cfg.get("spread", _DEFAULT_SPREAD)),
        slippage_pct=float(cfg.get("slippage_pct", _DEFAULT_SLIPPAGE)),
        commission_per_lot=float(cfg.get("commission_per_lot", _DEFAULT_COMMISSION)),
    )
    result.update({
        "symbol": cfg.get("symbol", ""),
        "timeframe": cfg.get("timeframe", ""),
        "strategy": cfg.get("strategy", "ema_crossover"),
        "fast_period": cfg.get("fast_period", 9),
        "slow_period": cfg.get("slow_period", 21),
    })
    return result


ALL_STRATEGIES = {
    "ema_crossover": {"name": "EMA Crossover", "params": ["fast_period", "slow_period"]},
    "rsi": {"name": "RSI Mean Reversion", "params": ["rsi_period", "rsi_oversold", "rsi_overbought"]},
    "macd": {"name": "MACD Crossover", "params": ["macd_fast", "macd_slow", "macd_signal"]},
    "bollinger": {"name": "Bollinger Bands", "params": ["bb_period", "bb_std"]},
    "supertrend": {"name": "SuperTrend", "params": ["st_period", "st_multiplier"]},
    "ichimoku": {"name": "Ichimoku Cloud", "params": []},
    "all": {"name": "All Strategies", "params": []},
}


# ── Flask-like Compat Wrapper (original function name preserved) ─

def _run_backtest(df, strategy, fast, slow, initial_balance, risk_pct,
                  **kwargs):
    return _run_backtest_vectorized(df, strategy, fast, slow,
                                     initial_balance, risk_pct, **kwargs)


# ── REST Endpoints ───────────────────────────────────────────────

@router.post("/backtest/run")
async def run_backtest(body: dict, user: User = Depends(resolve_user)):
    symbol = body.get("symbol", "XAUUSD")
    timeframe = body.get("timeframe", "H1")
    strategy = body.get("strategy", "ema_crossover")
    fast_period = body.get("fast_period", 9)
    slow_period = body.get("slow_period", 21)
    initial_balance = float(body.get("initial_balance", 10000))
    risk_percent = float(body.get("risk_percent", 2.0))
    spread = float(body.get("spread", -1))
    slippage_pct = float(body.get("slippage_pct", -1))
    commission_per_lot = float(body.get("commission_per_lot", -1))

    try:
        import yfinance as yf
        ticker = SYMBOLS.get(symbol.upper(), symbol)
        interval = TF_MAP.get(timeframe, "1h")
        period = PERIOD_MAP.get(interval, "6mo")
        t = yf.Ticker(ticker)
        df = t.history(period=period, interval=interval)
        if df.empty:
            return {"success": False, "error": f"No data for {symbol}"}

        # Auto-select spread from known map if not explicitly provided
        if spread < 0:
            spread = DEFAULT_SPREAD_MAP.get(symbol.upper(), _DEFAULT_SPREAD)
        if slippage_pct < 0:
            slippage_pct = _DEFAULT_SLIPPAGE
        if commission_per_lot < 0:
            commission_per_lot = _DEFAULT_COMMISSION

        result = _run_backtest_vectorized(df, strategy, fast_period, slow_period,
                                           initial_balance, risk_percent,
                                           spread=spread,
                                           slippage_pct=slippage_pct,
                                           commission_per_lot=commission_per_lot)
        result["symbol"] = symbol
        result["timeframe"] = timeframe
        return {"success": True, "data": result}
    except Exception as e:
        logger.error("Backtest error: %s", e)
        return {"success": False, "error": str(e)}


@router.post("/backtest/parallel-run")
async def parallel_backtest(body: dict, user: User = Depends(resolve_user)):
    configs = body.get("configs", [])
    if not configs:
        return {"success": False, "error": "No backtest configs provided"}

    try:
        import yfinance as yf
        results = []
        with concurrent.futures.ProcessPoolExecutor(max_workers=min(8, len(configs))) as executor:
            futures = [executor.submit(_run_single_backtest, cfg) for cfg in configs]
            for future in concurrent.futures.as_completed(futures):
                try:
                    results.append(future.result())
                except Exception as e:
                    results.append({"error": str(e)})

        results.sort(key=lambda x: x.get("sharpe_ratio", -999) if x.get("sharpe_ratio") is not None else -999, reverse=True)
        return {"success": True, "data": {"results": results, "total": len(results)}}
    except Exception as e:
        logger.error("Parallel backtest error: %s", e)
        return {"success": False, "error": str(e)}


@router.post("/backtest/optimize")
async def optimize_strategy(body: dict, user: User = Depends(resolve_user)):
    symbol = body.get("symbol", "XAUUSD")
    timeframe = body.get("timeframe", "H1")
    initial_balance = float(body.get("initial_balance", 10000))
    risk_percent = float(body.get("risk_percent", 2.0))
    fast_range = body.get("fast_range", [5, 9, 13, 17, 21])
    slow_range = body.get("slow_range", [21, 30, 40, 50, 60])
    strategy = body.get("strategy", "ema_crossover")
    spread = float(body.get("spread", -1))
    slippage_pct = float(body.get("slippage_pct", -1))
    commission_per_lot = float(body.get("commission_per_lot", -1))

    try:
        import yfinance as yf
        ticker = SYMBOLS.get(symbol.upper(), symbol)
        interval = TF_MAP.get(timeframe, "1h")
        period = PERIOD_MAP.get(interval, "6mo")
        t = yf.Ticker(ticker)
        df = t.history(period=period, interval=interval)
        if df.empty:
            return {"success": False, "error": f"No data for {symbol}"}

        configs = []
        for fp in fast_range:
            for sp in slow_range:
                if fp >= sp:
                    continue
                configs.append({
                    "df": df, "strategy": strategy,
                    "fast_period": fp, "slow_period": sp,
                    "initial_balance": initial_balance, "risk_percent": risk_percent,
                    "spread": spread if spread >= 0 else DEFAULT_SPREAD_MAP.get(symbol.upper(), _DEFAULT_SPREAD),
                    "slippage_pct": slippage_pct if slippage_pct >= 0 else _DEFAULT_SLIPPAGE,
                    "commission_per_lot": commission_per_lot if commission_per_lot >= 0 else _DEFAULT_COMMISSION,
                })

        with concurrent.futures.ProcessPoolExecutor(max_workers=min(8, len(configs))) as executor:
            futures = {executor.submit(_run_single_backtest, c): c for c in configs}
            results = []
            for future in concurrent.futures.as_completed(futures):
                r = future.result()
                results.append({
                    "fast_period": r["fast_period"], "slow_period": r["slow_period"],
                    "total_return": r["total_return"], "sharpe_ratio": r["sharpe_ratio"],
                    "max_drawdown": r["max_drawdown"], "win_rate": r["win_rate"],
                    "total_trades": r["total_trades"], "profit_factor": r["profit_factor"],
                })

        results.sort(key=lambda x: x["sharpe_ratio"], reverse=True)
        return {"success": True, "data": {"top_results": results[:10], "all_results": results, "total": len(results)}}
    except Exception as e:
        return {"success": False, "error": str(e)}


@router.post("/backtest/walk-forward")
async def walk_forward_optimization(body: dict, user: User = Depends(resolve_user)):
    symbol = body.get("symbol", "XAUUSD")
    timeframe = body.get("timeframe", "H1")
    initial_balance = float(body.get("initial_balance", 10000))
    risk_percent = float(body.get("risk_percent", 2.0))
    train_pct = float(body.get("train_pct", 70))
    spread = float(body.get("spread", -1))
    slippage_pct = float(body.get("slippage_pct", -1))
    commission_per_lot = float(body.get("commission_per_lot", -1))

    import yfinance as yf
    ticker = SYMBOLS.get(symbol.upper(), symbol)
    interval = TF_MAP.get(timeframe, "1h")
    period = PERIOD_MAP.get(interval, "6mo")
    t = yf.Ticker(ticker)
    df = t.history(period=period, interval=interval)
    if df.empty:
        return {"success": False, "error": f"No data for {symbol}"}

    # Resolve friction defaults
    if spread < 0:
        spread = DEFAULT_SPREAD_MAP.get(symbol.upper(), _DEFAULT_SPREAD)
    if slippage_pct < 0:
        slippage_pct = _DEFAULT_SLIPPAGE
    if commission_per_lot < 0:
        commission_per_lot = _DEFAULT_COMMISSION

    split = int(len(df) * train_pct / 100)
    df_train = df.iloc[:split]
    df_test = df.iloc[split:]

    # Parallelized grid search on train set
    configs = []
    for fp in [5, 9, 13, 17, 21]:
        for sp in [21, 30, 40, 50, 60]:
            if fp >= sp:
                continue
            configs.append({
                "df": df_train, "strategy": "ema_crossover",
                "fast_period": fp, "slow_period": sp,
                "initial_balance": initial_balance, "risk_percent": risk_percent,
                "spread": spread, "slippage_pct": slippage_pct,
                "commission_per_lot": commission_per_lot,
            })

    best_sharpe = -999
    best_params = {"fast_period": 9, "slow_period": 21}
    with concurrent.futures.ProcessPoolExecutor(max_workers=min(8, len(configs))) as executor:
        futures = {executor.submit(_run_single_backtest, c): c for c in configs}
        for future in concurrent.futures.as_completed(futures):
            r = future.result()
            if r.get("sharpe_ratio", -999) > best_sharpe:
                best_sharpe = r["sharpe_ratio"]
                best_params = {"fast_period": r["fast_period"], "slow_period": r["slow_period"]}

    test_result = _run_backtest_vectorized(df_test, "ema_crossover",
                                            best_params["fast_period"],
                                            best_params["slow_period"],
                                            initial_balance, risk_percent,
                                            spread=spread,
                                            slippage_pct=slippage_pct,
                                            commission_per_lot=commission_per_lot)

    return {"success": True, "data": {
        "best_params": best_params,
        "train_result": {"sharpe": round(best_sharpe, 2)},
        "test_result": test_result,
    }}


@router.websocket("/backtest/ws/tick-replay")
async def tick_replay_ws(ws: WebSocket):
    await ws.accept()
    try:
        config = await ws.receive_json()
        symbols = config.get("symbols", ["AAPL", "MSFT"])
        data_dir = config.get("data_dir", "data/ticks")
        cash = config.get("cash", 1000000)

        from app.services.backtest.multi_asset_merger import merge_tick_streams
        from app.services.backtest.tick_replay_engine import StreamingReplayEngine
        from app.services.backtest.strategies.pairs_trading import PairsTrading

        import asyncio
        async def send(data: dict):
            await ws.send_json(data)

        def cb(data: dict):
            asyncio.run_coroutine_threadsafe(send(data), asyncio.get_running_loop())

        stream = merge_tick_streams(data_dir, symbols)
        strategy = PairsTrading(symbols[0], symbols[1])
        engine = StreamingReplayEngine(strategy, cash, on_update=cb)
        loop = asyncio.get_running_loop()
        report = await loop.run_in_executor(None, engine.run, stream)
        await ws.send_json({"type": "REPORT", "data": report})

    except WebSocketDisconnect:
        pass
    except Exception as e:
        await ws.send_json({"type": "ERROR", "message": str(e)})
