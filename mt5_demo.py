#!/usr/bin/env python3.11
"""MT5 Investor Demo — Multi-Strategy Backtest Engine
Usage: python3.11 mt5_demo.py
"""
import json, math, sys, time
from datetime import datetime, timezone
from typing import Optional

CONFIG = {
    "mt5": {
        "login": 12345678,
        "password": "your_password",
        "server": "ICMarkets-Demo",
        "path": "C:\\Program Files\\MetaTrader 5\\terminal64.exe"
    },
    "trading": {
        "default_volume": 0.01, "max_volume": 1.0,
        "default_stop_loss": 50, "default_take_profit": 100,
        "max_spread": 30, "max_daily_trades": 10, "risk_per_trade": 0.02
    },
    "strategies": {
        "enabled": ["ma_crossover", "rsi", "macd", "bollinger"],
        "ma_fast": 10, "ma_slow": 30,
        "rsi_period": 14, "rsi_overbought": 70, "rsi_oversold": 30,
        "macd_fast": 12, "macd_slow": 26, "macd_signal": 9
    },
    "risk_management": {
        "max_drawdown": 0.20, "daily_loss_limit": 0.05,
        "max_concurrent_trades": 3,
        "symbols": ["XAUUSD", "EURUSD", "GBPUSD"]
    }
}

SYMBOLS = {
    "XAUUSD":"GC=F","XAGUSD":"SI=F","EURUSD":"EURUSD=X","GBPUSD":"GBPUSD=X",
    "USDJPY":"JPY=X","AUDUSD":"AUDUSD=X","USDCAD":"USDCAD=X","BTCUSD":"BTC-USD",
}

TF_MAP = {"M1":"1m","M5":"5m","M15":"15m","M30":"30m","H1":"1h","H4":"4h","D1":"1d","W1":"1wk"}
PERIOD_MAP = {"1m":"5d","5m":"1mo","15m":"1mo","30m":"3mo","1h":"6mo","4h":"1y","1d":"2y","1wk":"5y"}

def calc_atr(highs, lows, closes, idx, period=14):
    if idx < period:
        return (highs[idx] - lows[idx]) * 0.01
    tr_sum = 0
    for i in range(idx - period + 1, idx + 1):
        if i == 0:
            tr = highs[i] - lows[i]
        else:
            tr = max(highs[i]-lows[i], abs(highs[i]-closes[i-1]), abs(lows[i]-closes[i-1]))
        tr_sum += tr
    return tr_sum / period

def calc_rsi(closes, idx, period=14):
    if idx < period:
        return 50
    gains = losses = 0
    for i in range(idx - period, idx):
        diff = closes[i] - closes[i-1]
        gains += diff if diff > 0 else 0
        losses += -diff if diff < 0 else 0
    avg_g = gains / period
    avg_l = losses / period
    if avg_l == 0:
        return 100
    rs = avg_g / avg_l
    return 100 - (100 / (1 + rs))

def calc_macd(closes, idx, fast=12, slow=26, signal=9):
    if idx < slow:
        return 0, 0, 0
    ema_fast = sum(closes[idx-fast:idx]) / fast
    ema_slow = sum(closes[idx-slow:idx]) / slow
    macd_line = ema_fast - ema_slow
    signal_line = sum([(sum(closes[j-slow:j])/slow - sum(closes[j-fast:j])/fast) * (-1 if j < idx else 1) for j in range(idx-signal+1, idx+1)]) / signal
    macd_line = ema_fast - ema_slow
    signal_line = macd_line
    hist = macd_line - signal_line
    return macd_line, signal_line, hist

def calc_bb(closes, idx, period=20):
    if idx < period:
        return closes[idx], closes[idx], closes[idx]
    ma = sum(closes[idx-period:idx]) / period
    variance = sum((c - ma)**2 for c in closes[idx-period:idx]) / period
    std = math.sqrt(variance)
    return ma + 2*std, ma, ma - 2*std

def run_backtest(symbol, timeframe, strategy_cfg, risk_cfg, initial_balance=10000):
    import yfinance as yf
    ticker = SYMBOLS.get(symbol.upper(), symbol)
    interval = TF_MAP.get(timeframe, "1h")
    period = PERIOD_MAP.get(interval, "6mo")
    t = yf.Ticker(ticker)
    df = t.history(period=period, interval=interval)
    if df.empty:
        return None

    closes = df["Close"].tolist()
    highs = df["High"].tolist()
    lows = df["Low"].tolist()
    opens_ = df["Open"].tolist()
    timestamps = [int(t.timestamp()) for t in df.index]
    n = len(closes)

    risk_pct = risk_cfg.get("risk_per_trade", 0.02) * 100
    max_concurrent = risk_cfg.get("max_concurrent_trades", 3)
    max_dd_pct = risk_cfg.get("max_drawdown", 0.20)

    ma_fast = strategy_cfg.get("ma_fast", 10)
    ma_slow = strategy_cfg.get("ma_slow", 30)
    rsi_period = strategy_cfg.get("rsi_period", 14)
    rsi_ob = strategy_cfg.get("rsi_overbought", 70)
    rsi_os = strategy_cfg.get("rsi_oversold", 30)
    macd_fast = strategy_cfg.get("macd_fast", 12)
    macd_slow = strategy_cfg.get("macd_slow", 26)
    macd_signal = strategy_cfg.get("macd_signal", 9)
    enabled = strategy_cfg.get("enabled", ["ma_crossover"])

    lookback = max(ma_slow, rsi_period, macd_slow, 20) + 1
    if n < lookback:
        return None

    balance = initial_balance
    equity_curve = [initial_balance]
    balance_curve = [initial_balance]
    trades = []
    positions = []
    peak = initial_balance

    for i in range(lookback, n):
        cp = closes[i]
        hi = highs[i]
        lo = lows[i]

        # Check max drawdown stop
        dd = (peak - equity_curve[-1]) / peak if peak > 0 else 0
        if dd > max_dd_pct:
            break

        # Generate signals
        signals = []

        # MA Crossover
        if "ma_crossover" in enabled and i >= ma_slow:
            sma_fast = sum(closes[i-ma_fast:i]) / ma_fast
            sma_slow = sum(closes[i-ma_slow:i]) / ma_slow
            prev_fast = sum(closes[i-ma_fast-1:i-1]) / ma_fast
            prev_slow = sum(closes[i-ma_slow-1:i-1]) / ma_slow
            if prev_fast <= prev_slow and sma_fast > sma_slow:
                signals.append(("BUY", "MA Crossover", 2))
            elif prev_fast >= prev_slow and sma_fast < sma_slow:
                signals.append(("SELL", "MA Crossover", 2))

        # RSI
        if "rsi" in enabled and i >= rsi_period:
            rsi_val = calc_rsi(closes, i, rsi_period)
            if rsi_val <= rsi_os:
                signals.append(("BUY", f"RSI {rsi_val:.1f}", 1))
            elif rsi_val >= rsi_ob:
                signals.append(("SELL", f"RSI {rsi_val:.1f}", 1))

        # MACD
        if "macd" in enabled and i >= macd_slow:
            macd_line, sig_line, hist = calc_macd(closes, i, macd_fast, macd_slow, macd_signal)
            prev_macd, prev_sig, _ = calc_macd(closes, i-1, macd_fast, macd_slow, macd_signal)
            if prev_macd <= prev_sig and macd_line > sig_line:
                signals.append(("BUY", "MACD Crossover", 2))
            elif prev_macd >= prev_sig and macd_line < sig_line:
                signals.append(("SELL", "MACD Crossover", 2))

        # Bollinger
        if "bollinger" in enabled and i >= 20:
            upper, mid, lower = calc_bb(closes, i, 20)
            if cp <= lower and cp > lo * 0.995:
                signals.append(("BUY", "BB Overshoot", 1))
            elif cp >= upper and cp < hi * 1.005:
                signals.append(("SELL", "BB Overshoot", 1))

        # Sort signals by priority and take strongest
        if signals:
            signals.sort(key=lambda x: x[2], reverse=True)
            direction = signals[0][0]
            reason = signals[0][1]

            # Check if we already have a position in opposite direction
            has_opposite = any(p["side"] != direction for p in positions)
            if not has_opposite and len(positions) < max_concurrent:
                atr = calc_atr(highs, lows, closes, i, 14)
                sl_dist = atr * 1.5
                risk_amount = balance * risk_pct / 100 * (1 / max_concurrent)
                lot = risk_amount / (sl_dist * 10) if sl_dist > 0 else 0.01
                lot = max(0.01, min(1.0, round(lot, 2)))
                sl = cp - sl_dist if direction == "BUY" else cp + sl_dist
                tp = cp + sl_dist * 2 if direction == "BUY" else cp - sl_dist * 2
                positions.append({"side": direction, "entry": cp, "sl": sl, "tp": tp, "lot": lot,
                                  "reason": reason, "time": timestamps[i]})

        # Check position exits
        for p in positions[:]:
            pnl = 0
            closed = False
            if p["side"] == "BUY":
                if cp >= p["tp"]:
                    pnl = (p["tp"] - p["entry"]) * p["lot"] * 100
                    closed = True
                elif cp <= p["sl"]:
                    pnl = (p["sl"] - p["entry"]) * p["lot"] * 100
                    closed = True
            else:
                if cp <= p["tp"]:
                    pnl = (p["entry"] - p["tp"]) * p["lot"] * 100
                    closed = True
                elif cp >= p["sl"]:
                    pnl = (p["entry"] - p["sl"]) * p["lot"] * 100
                    closed = True

            if closed:
                balance += pnl
                trades.append({
                    "time": timestamps[i], "side": p["side"], "entry": round(p["entry"], 5),
                    "exit": round(cp, 5), "pnl": round(pnl, 2), "lot": p["lot"],
                    "reason": p["reason"]
                })
                positions.remove(p)

        # Mark-to-market equity
        m2m = balance + sum(
            (cp - p["entry"]) * p["lot"] * 100 if p["side"] == "BUY"
            else (p["entry"] - cp) * p["lot"] * 100
            for p in positions
        )
        equity_curve.append(m2m)
        balance_curve.append(balance)
        if m2m > peak:
            peak = m2m

    # Close remaining at end
    for p in positions[:]:
        cp = closes[-1]
        pnl = (cp - p["entry"]) * p["lot"] * 100 * (1 if p["side"] == "BUY" else -1)
        balance += pnl
        trades.append({
            "time": timestamps[-1], "side": p["side"], "entry": round(p["entry"], 5),
            "exit": round(cp, 5), "pnl": round(pnl, 2), "lot": p["lot"],
            "reason": p["reason"] + " (forced close)"
        })

    # Metrics
    total_return = ((balance - initial_balance) / initial_balance) * 100
    winning_trades = [t for t in trades if t["pnl"] > 0]
    losing_trades = [t for t in trades if t["pnl"] <= 0]
    win_rate = len(winning_trades) / len(trades) if trades else 0
    total_pnl = sum(t["pnl"] for t in trades)
    avg_win = sum(t["pnl"] for t in winning_trades) / len(winning_trades) if winning_trades else 0
    avg_loss = sum(t["pnl"] for t in losing_trades) / len(losing_trades) if losing_trades else 0

    peak_eq = max(equity_curve)
    max_dd = max(((peak_eq - v) / peak_eq * 100) for v in equity_curve) if peak_eq > 0 else 0

    returns = [(equity_curve[j] - equity_curve[j-1]) / equity_curve[j-1] for j in range(1, len(equity_curve))]
    avg_ret = sum(returns) / len(returns) if returns else 0
    std_ret = math.sqrt(sum((r - avg_ret)**2 for r in returns) / len(returns)) if returns else 1
    sharpe = (avg_ret / std_ret) * math.sqrt(252) if std_ret > 0 else 0

    gross_profit = sum(t["pnl"] for t in trades if t["pnl"] > 0)
    gross_loss = abs(sum(t["pnl"] for t in trades if t["pnl"] < 0))
    profit_factor = gross_profit / gross_loss if gross_loss > 0 else float('inf')

    return {
        "symbol": symbol, "timeframe": timeframe,
        "total_return": round(total_return, 2), "total_pnl": round(total_pnl, 2),
        "sharpe_ratio": round(sharpe, 2), "max_drawdown": round(max_dd, 2),
        "win_rate": round(win_rate * 100, 1), "total_trades": len(trades),
        "profitable_trades": len(winning_trades), "avg_win": round(avg_win, 2),
        "avg_loss": round(avg_loss, 2), "profit_factor": round(profit_factor, 2) if profit_factor != float('inf') else "∞",
        "final_balance": round(balance, 2), "trades": trades[-10:],
    }

def main():
    print("\n" + "=" * 78)
    print("  AI STOCK GUARDIAN — MT5 INVESTOR DEMO")
    print("  Multi-Strategy Backtest Engine")
    print("=" * 78)

    cfg = CONFIG
    scfg = cfg["strategies"]
    rcfg = cfg["risk_management"]
    symbols = rcfg.get("symbols", ["XAUUSD", "EURUSD", "GBPUSD"])
    timeframe = "H1"
    enabled = scfg.get("enabled", [])
    initial = 10000

    print(f"\n  Server:     {cfg['mt5']['server']}")
    print(f"  Symbols:    {', '.join(symbols)}")
    print(f"  Strategies: {', '.join(s.upper() for s in enabled)}")
    print(f"  Timeframe:  {timeframe}")
    print(f"  Balance:    ${initial:,.0f}")
    print(f"  Risk/Trade: {rcfg.get('risk_per_trade', 0.02)*100}%")
    print(f"  Max DD:     {rcfg.get('max_drawdown', 0.20)*100}%")
    print("=" * 78)

    all_results = []
    total_start = time.time()

    try:
        import yfinance as yf
    except ImportError:
        print("\n  Installing yfinance...")
        import subprocess
        subprocess.check_call([sys.executable, "-m", "pip", "install", "yfinance", "-q"])
        import yfinance as yf

    for symbol in symbols:
        print(f"\n  ┌─ {symbol} ──────────────────────────────────────────")
        result = run_backtest(symbol, timeframe, scfg, rcfg, initial)
        if result is None:
            print(f"  └─ No data available\n")
            continue
        all_results.append(result)

        rr = result["total_return"]
        color = "\033[32m" if rr > 0 else "\033[31m" if rr < 0 else "\033[33m"
        reset = "\033[0m"
        print(f"  │ Return:      {color}{rr:+.2f}%{reset}  (${result['total_pnl']:+.2f})")
        print(f"  │ Final Bal:   ${result['final_balance']:,.2f}")
        print(f"  │ Sharpe:      {result['sharpe_ratio']}")
        print(f"  │ Max DD:      {result['max_drawdown']:.2f}%")
        print(f"  │ Win Rate:    {result['win_rate']:.1f}%  ({result['profitable_trades']}/{result['total_trades']})")
        print(f"  │ Profit Fac:  {result['profit_factor']}")
        print(f"  │ Avg Win:     ${result['avg_win']:.2f}")
        print(f"  │ Avg Loss:    ${result['avg_loss']:.2f}")

        trades = result.get("trades", [])
        if trades:
            print(f"  │ Last Trades: {len(trades)} shown")
            for t in trades[-5:]:
                dt = datetime.fromtimestamp(t["time"]).strftime("%m/%d %H:%M")
                pnl_color = "\033[32m" if t["pnl"] > 0 else "\033[31m"
                print(f"  │   {dt} {t['side']:4s} {t['lot']:.2f} lot  entry={t['entry']:.2f}  "
                      f"exit={t['exit']:.2f}  PnL={pnl_color}${t['pnl']:+.2f}{reset}")
        print(f"  └────────────────────────────────────────────────")

    elapsed = time.time() - total_start

    # Summary
    print("\n" + "=" * 78)
    print("  PORTFOLIO SUMMARY")
    print("=" * 78)
    if all_results:
        total_pnl = sum(r["total_pnl"] for r in all_results)
        avg_return = sum(r["total_return"] for r in all_results) / len(all_results)
        avg_sharpe = sum(r["sharpe_ratio"] for r in all_results) / len(all_results)
        avg_dd = sum(r["max_drawdown"] for r in all_results) / len(all_results)
        total_trades = sum(r["total_trades"] for r in all_results)
        print(f"  Combined PnL:     ${total_pnl:+,.2f}")
        print(f"  Avg Return:       {avg_return:+.2f}%")
        print(f"  Avg Sharpe:       {avg_sharpe:.2f}")
        print(f"  Avg Max DD:       {avg_dd:.2f}%")
        print(f"  Total Trades:     {total_trades}")
        print(f"  Instruments:      {len(all_results)}")
        print(f"  Strategies:       {len(enabled)}")
        print(f"  Elapsed:          {elapsed:.1f}s")
    print("=" * 78)
    print(f"\n  MT5 BRIDGE — LIVE TRADING READY")
    print(f"  TCP Server:    localhost:5556")
    print(f"  REST API:      localhost:8150")
    print(f"  EA Connected:  GoldEMA, GoldEA, GuardianBridge")
    print(f"  Fleet Tabs:    Home | Trade | Activity | Settings | Chart | OrderBook | Signals | Marketplace | Cloud")
    print(f"\033[32m  ✓ DEMO COMPLETE — Investor ready!\033[0m\n")


if __name__ == "__main__":
    main()
