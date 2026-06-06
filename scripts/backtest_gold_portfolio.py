#!/usr/bin/env python3.11
"""
Gold (XAUUSD) Portfolio Backtest — Comprehensive Report with Realistic Frictions.
Compares no-friction vs friction results side-by-side.  Run from project root.

Usage:
    cd backend && python3.11 ../scripts/backtest_gold_portfolio.py
"""
import sys, json, math
from datetime import datetime, timezone
from collections import defaultdict

import numpy as np
import pandas as pd

sys.path.insert(0, ".")
from app.routers.backtest import (
    _run_backtest_vectorized,
    _run_single_backtest,
    DEFAULT_SPREAD_MAP,
)


# ── Config ───────────────────────────────────────────────────────
SYMBOL = "XAUUSD"
TIMEFRAME = "H1"
INTERVAL = "1h"
PERIOD = "6mo"
STRATEGY = "ema_crossover"
FAST = 9
SLOW = 21
INITIAL_BALANCE = 10_000
RISK_PCT = 2.0

# Symbol-specific frictions
SPREAD = DEFAULT_SPREAD_MAP.get(SYMBOL, 0.5)
SLIPPAGE_PCT = 0.0003
COMMISSION_PER_LOT = 7.0

TABLE_WIDTH = 82


def fmt(v, decimals=2):
    if isinstance(v, float):
        return round(v, decimals)
    return v


def header(title):
    print()
    print("=" * TABLE_WIDTH)
    print(f"  {title}")
    print("=" * TABLE_WIDTH)


def subheader(title):
    print(f"  {title}")
    print("  " + "- *"[len(title):])


def main():
    # ── Fetch Data ──────────────────────────────────────────────
    header(f"GOLD PORTFOLIO BACKTEST — {SYMBOL} ({TIMEFRAME})")
    print(f"  Strategy:   {STRATEGY} ({FAST}/{SLOW})")
    print(f"  Balance:    ${INITIAL_BALANCE:,.0f}  Risk: {RISK_PCT}%")
    print(f"  Spread:     {SPREAD}  Slippage: {SLIPPAGE_PCT*100:.3f}%  Comm: ${COMMISSION_PER_LOT}/lot")
    print(f"  Data:       {INTERVAL} × {PERIOD}")

    import yfinance as yf
    print("\n  Fetching XAUUSD data from yfinance...", end=" ")
    t = yf.Ticker("GC=F")
    df = t.history(period=PERIOD, interval=INTERVAL)
    if df.empty:
        print("ERROR: no data")
        return
    print(f"OK — {len(df)} bars  ({df.index[0].date()} → {df.index[-1].date()})")

    # ── Run No-Friction ─────────────────────────────────────────
    header("PERFORMANCE: NO FRICTION vs WITH FRICTION")
    print(f"  {'Metric':<22s} {'No Friction':>16s} {'With Friction':>16s} {'Delta':>16s}")
    print("  " + "-" * (TABLE_WIDTH - 4))

    r_no = _run_backtest_vectorized(
        df.copy(), STRATEGY, FAST, SLOW,
        initial_balance=INITIAL_BALANCE, risk_pct=RISK_PCT,
        spread=0.0, slippage_pct=0.0, commission_per_lot=0.0,
    )
    r_fx = _run_backtest_vectorized(
        df.copy(), STRATEGY, FAST, SLOW,
        initial_balance=INITIAL_BALANCE, risk_pct=RISK_PCT,
        spread=SPREAD, slippage_pct=SLIPPAGE_PCT, commission_per_lot=COMMISSION_PER_LOT,
    )

    metrics = [
        ("Total Return %",   "total_return",     1),
        ("Total P&L ($)",    "total_pnl",        1),
        ("Final Balance ($)","final_balance",    1),
        ("Sharpe Ratio",     "sharpe_ratio",     1),
        ("Max Drawdown %",   "max_drawdown",     1),
        ("Win Rate %",       "win_rate",         100),
        ("Total Trades",     "total_trades",     0),
        ("Avg Win ($)",      "avg_win",          1),
        ("Avg Loss ($)",     "avg_loss",         1),
        ("Profit Factor",    "profit_factor",    1),
    ]
    for label, key, mult in metrics:
        v_no = r_no.get(key, 0)
        v_fx = r_fx.get(key, 0)
        if mult:
            v_no = v_no * mult
            v_fx = v_fx * mult
        d = v_fx - v_no
        if isinstance(v_no, int) and isinstance(v_fx, int):
            print(f"  {label:<22s} {v_no:>16d} {v_fx:>16d} {d:>+16d}")
        else:
            print(f"  {label:<22s} {v_no:>16.2f} {v_fx:>16.2f} {d:>+16.2f}")

    # ── Cost Analysis ────────────────────────────────────────────
    ca = r_fx["cost_analysis"]
    header("COST BREAKDOWN (Friction Run)")
    print(f"  {'Cost Component':<30s} {'Amount ($)':>15s} {'Per Trade ($)':>15s}")
    print("  " + "-" * (TABLE_WIDTH - 4))
    nt = max(r_fx["total_trades"], 1)
    print(f"  {'Commission':<30s} {ca['total_commission']:>15.2f} {ca['total_commission']/nt:>15.2f}")
    print(f"  {'Spread Cost':<30s} {ca['total_spread_cost']:>15.2f} {ca['total_spread_cost']/nt:>15.2f}")
    print(f"  {'Slippage Cost':<30s} {ca['total_slippage_cost']:>15.2f} {ca['total_slippage_cost']/nt:>15.2f}")
    print(f"  {'──' :<30s} {'──':>15s} {'──':>15s}")
    print(f"  {'Total Friction Cost':<30s} {ca['total_costs']:>15.2f} {ca['total_costs']/nt:>15.2f}")

    # ── Monthly Returns ──────────────────────────────────────────
    months_no = r_no.get("monthly_returns", {})
    months_fx = r_fx.get("monthly_returns", {})

    header("MONTHLY RETURNS")
    print(f"  {'Month':<10s} {'No Friction':>14s} {'With Friction':>14s} {'Delta':>14s}")
    print("  " + "-" * (TABLE_WIDTH - 4))
    all_months = sorted(set(list(months_no.keys()) + list(months_fx.keys())))
    for m in all_months:
        v_no = months_no.get(m, 0)
        v_fx = months_fx.get(m, 0)
        print(f"  {m:<10s} {v_no:>14.2f} {v_fx:>14.2f} {v_fx - v_no:>+14.2f}")

    # ── Top 10 Best / Worst Trades ───────────────────────────────
    header("TOP 10 BEST TRADES (Friction)")
    trades_fx = sorted(r_fx.get("trades", []), key=lambda t: t["pnl"], reverse=True)
    print(f"  {'#':>3s} {'Date':>22s} {'Side':>6s} {'Entry':>10s} {'Exit':>10s} {'P&L':>10s} {'Comm':>8s} {'Sprd':>8s} {'Slip':>8s}")
    print("  " + "-" * (TABLE_WIDTH - 4))
    for i, t in enumerate(trades_fx[:10]):
        dt = datetime.fromtimestamp(t["time"], tz=timezone.utc).strftime("%Y-%m-%d %H:%M")
        print(f"  {i+1:>3d} {dt:>22s} {t['side']:>6s} {t['entry']:>10.2f} {t['exit']:>10.2f} "
              f"{t['pnl']:>10.2f} {t.get('commission',0):>8.2f} {t.get('spread_cost',0):>8.2f} "
              f"{t.get('slippage_cost',0):>8.2f}")

    header("TOP 10 WORST TRADES (Friction)")
    trades_fx_w = sorted(r_fx.get("trades", []), key=lambda t: t["pnl"])
    print(f"  {'#':>3s} {'Date':>22s} {'Side':>6s} {'Entry':>10s} {'Exit':>10s} {'P&L':>10s} {'Comm':>8s} {'Sprd':>8s} {'Slip':>8s}")
    print("  " + "-" * (TABLE_WIDTH - 4))
    for i, t in enumerate(trades_fx_w[:10]):
        dt = datetime.fromtimestamp(t["time"], tz=timezone.utc).strftime("%Y-%m-%d %H:%M")
        print(f"  {i+1:>3d} {dt:>22s} {t['side']:>6s} {t['entry']:>10.2f} {t['exit']:>10.2f} "
              f"{t['pnl']:>10.2f} {t.get('commission',0):>8.2f} {t.get('spread_cost',0):>8.2f} "
              f"{t.get('slippage_cost',0):>8.2f}")

    # ── Equity Curve Summary ─────────────────────────────────────
    header("EQUITY CURVE SUMMARY")
    eq_no = np.array(r_no.get("equity_curve", [INITIAL_BALANCE]))
    eq_fx = np.array(r_fx.get("equity_curve", [INITIAL_BALANCE]))
    eq_no = eq_no[eq_no > 0]
    eq_fx = eq_fx[eq_fx > 0]

    def eq_stats(eq, label):
        peak = np.maximum.accumulate(eq)
        dd = (peak - eq) / peak * 100
        start = eq[0] if len(eq) > 0 else INITIAL_BALANCE
        end = eq[-1] if len(eq) > 0 else INITIAL_BALANCE
        print(f"  {label}:")
        print(f"    Start:   ${start:>10,.2f}")
        print(f"    End:     ${end:>10,.2f}  ({((end-start)/start)*100:>+.2f}%)")
        print(f"    Peak:    ${float(np.max(eq)):>10,.2f}")
        print(f"    Valley:  ${float(np.min(eq)):>10,.2f}")
        print(f"    Max DD:  {float(np.max(dd)):>9.2f}%")
        if len(eq) > 1:
            rets = np.diff(eq) / eq[:-1]
            sharpe = (np.mean(rets) / np.std(rets) * math.sqrt(252)
                      if np.std(rets) > 0 else 0)
            print(f"    Sharpe:  {sharpe:>10.3f}")

    eq_stats(eq_no, "No Friction")
    print()
    eq_stats(eq_fx, "With Friction")

    # ── Trade Distribution ────────────────────────────────────────
    header("TRADE DISTRIBUTION")
    pnls = [t["pnl"] for t in r_fx.get("trades", [])]
    if pnls:
        arr = np.array(pnls)
        wins = arr[arr > 0]
        losses = arr[arr <= 0]
        print(f"  Total Trades:  {len(arr)}")
        print(f"  Profitable:    {len(wins)} ({len(wins)/len(arr)*100:.1f}%)")
        print(f"  Losing:        {len(losses)} ({len(losses)/len(arr)*100:.1f}%)")
        print(f"  Avg P&L:       ${np.mean(arr):>8.2f}")
        print(f"  Std P&L:       ${np.std(arr, ddof=1):>8.2f}")
        if len(wins) > 0:
            print(f"  Avg Win:       ${np.mean(wins):>8.2f}  Best: ${np.max(wins):>8.2f}")
        if len(losses) > 0:
            print(f"  Avg Loss:      ${np.mean(losses):>8.2f}  Worst: ${np.min(losses):>8.2f}")
        if len(wins) > 0 and len(losses) > 0:
            pf = abs(np.sum(wins) / np.sum(losses)) if np.sum(losses) != 0 else float("inf")
            print(f"  Profit Factor: {pf:.2f}")

    # ── Summary ──────────────────────────────────────────────────
    header("COMPARISON: Friction Impact")
    pnl_no = r_no["total_pnl"]
    pnl_fx = r_fx["total_pnl"]
    ret_no = r_no["total_return"]
    ret_fx = r_fx["total_return"]
    print(f"  {'Metric':<28s} {'No Friction':>14s} {'With Friction':>14s} {'Delta':>14s}")
    print("  " + "-" * (TABLE_WIDTH - 4))
    print(f"  {'Total Return %':<28s} {ret_no:>14.2f} {ret_fx:>14.2f} {ret_fx - ret_no:>+14.2f}")
    print(f"  {'Total P&L ($)':<28s} {pnl_no:>14.2f} {pnl_fx:>14.2f} {pnl_fx - pnl_no:>+14.2f}")
    print(f"  {'Total Friction Cost ($)':<28s} {'':>14s} {ca['total_costs']:>14.2f} {'':>14s}")
    print(f"  {'Friction as % of gross P&L':<28s} {'':>14s} "
          f"{ca['total_costs'] / abs(pnl_no) * 100 if pnl_no != 0 else 0:>13.1f}% {'':>14s}")

    print()
    print("=" * TABLE_WIDTH)
    print(f"  Script: scripts/backtest_gold_portfolio.py")
    print(f"  For MT5 comparison: compile and run backend/mql5/GoldBacktestEA/GoldBacktestEA.mq5")
    print("=" * TABLE_WIDTH)


if __name__ == "__main__":
    main()
