// src/algo_engine/BacktestEngine.cpp
#include "algo_engine/BacktestEngine.h"

#include "algo_engine/ConditionEvaluator.h"
#include "core/logging/Logger.h"

#include <QDateTime>

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>

namespace fincept::algo {

namespace {

constexpr int kWarmupBars = 50;
constexpr int kEvalWindow = 500;

double round_to(double v, int decimals) {
    double f = std::pow(10.0, decimals);
    return std::round(v * f) / f;
}

double bars_per_year(const QString& tf) {
    if (tf == "1m")  return 252.0 * 390.0;
    if (tf == "3m")  return 252.0 * 130.0;
    if (tf == "5m")  return 252.0 * 78.0;
    if (tf == "15m") return 252.0 * 26.0;
    if (tf == "30m") return 252.0 * 13.0;
    if (tf == "1h")  return 252.0 * 6.5;
    if (tf == "4h")  return 252.0 * 1.625;
    if (tf == "1d")  return 252.0;
    return 252.0;
}

} // namespace

QJsonObject BacktestEngine::run(const QVector<OhlcvCandle>& candles,
                                const QJsonArray& entry_conditions, const QString& entry_logic,
                                const QJsonArray& exit_conditions, const QString& exit_logic,
                                const Config& cfg, const QString& timeframe) {
    const int n = candles.size();
    const double size_frac = std::clamp(cfg.position_size_pct, 1.0, 100.0) / 100.0;
    if (n < kWarmupBars + 10) {
        QJsonObject err;
        err["success"] = false;
        err["error"] = QString("Insufficient data: %1 candles (need at least %2)")
                           .arg(n).arg(kWarmupBars + 10);
        return err;
    }

    LOG_INFO("Backtest", QString("run: candles=%1 tf=%2 entryConds=%3 exitConds=%4 "
                                 "sl=%5 tp=%6 sizePct=%7 comm=%8 slip=%9 short=%10")
                             .arg(n).arg(timeframe)
                             .arg(entry_conditions.size()).arg(exit_conditions.size())
                             .arg(cfg.stop_loss_pct).arg(cfg.take_profit_pct)
                             .arg(cfg.position_size_pct)
                             .arg(cfg.commission_pct).arg(cfg.slippage_pct)
                             .arg(cfg.allow_short ? "yes" : "no"));

    int entry_eval_count = 0, entry_true_count = 0, exit_true_count = 0, entry_err_count = 0;
    QString last_entry_err;
    bool entry_sampled = false;

    double cash = cfg.initial_capital;
    bool in_pos = false;
    bool is_short = false;
    double entry_price = 0.0;
    int entry_bar = 0;
    long long shares = 0;
    double highest = 0.0;
    double lowest = 0.0;

    bool entry_signal = false;
    bool exit_signal = false;

    QJsonArray trades;
    QVector<double> equity_curve;
    equity_curve.reserve(n - kWarmupBars);
    QVector<double> benchmark_curve;
    benchmark_curve.reserve(n - kWarmupBars);
    const double bench_base = candles[kWarmupBars].close;
    double peak_equity = cfg.initial_capital;
    double max_dd = 0.0;

    auto charge_commission = [&](double price, long long qty) -> double {
        if (cfg.commission_pct <= 0.0) return 0.0;
        return price * qty * (cfg.commission_pct / 100.0);
    };

    auto apply_slippage = [&](double price, bool buying) -> double {
        if (cfg.slippage_pct <= 0.0) return price;
        return buying ? price * (1.0 + cfg.slippage_pct / 100.0)
                      : price * (1.0 - cfg.slippage_pct / 100.0);
    };

    auto close_trade = [&](double exit_price, const char* reason, int exit_bar_idx) {
        double fill = apply_slippage(exit_price, !is_short);
        double pnl;
        if (is_short) {
            pnl = (entry_price - fill) * static_cast<double>(shares);
        } else {
            pnl = (fill - entry_price) * static_cast<double>(shares);
        }
        double comm = charge_commission(fill, shares);
        pnl -= comm;
        double pnl_pct = entry_price > 0 ? pnl / (entry_price * shares) * 100.0 : 0.0;
        cash += fill * shares - comm;
        QJsonObject t;
        t["entry_bar"] = entry_bar;
        t["exit_bar"] = exit_bar_idx;
        t["entry_price"] = round_to(entry_price, 2);
        t["exit_price"] = round_to(fill, 2);
        t["shares"] = static_cast<double>(shares);
        t["side"] = is_short ? QStringLiteral("SHORT") : QStringLiteral("LONG");
        t["pnl"] = round_to(pnl, 2);
        t["pnl_pct"] = round_to(pnl_pct, 2);
        t["commission"] = round_to(comm, 2);
        t["reason"] = QString::fromLatin1(reason);
        t["bars_held"] = exit_bar_idx - entry_bar;
        trades.append(t);
        in_pos = false;
        shares = 0;
    };

    for (int i = kWarmupBars; i < n; ++i) {
        const OhlcvCandle& bar = candles[i];

        // ── 1. Execute pending signal fills at THIS bar's open ──────────────
        if (!in_pos && entry_signal) {
            double px = apply_slippage(bar.open, true);
            long long qty = px > 0 ? static_cast<long long>(std::floor(cash * size_frac / px)) : 0;
            if (qty > 0) {
                double comm = charge_commission(px, qty);
                if (cash >= px * qty + comm) {
                    in_pos = true;
                    is_short = false;
                    entry_price = px;
                    entry_bar = i;
                    shares = qty;
                    cash -= px * qty + comm;
                    highest = px;
                    lowest = px;
                }
            }
            entry_signal = false;
        } else if (in_pos && exit_signal) {
            close_trade(bar.open, "exit_signal", i);
            exit_signal = false;
        }

        // ── Short entry ─────────────────────────────────────────────────────
        if (!in_pos && !entry_signal && cfg.allow_short && entry_conditions.isEmpty()) {
            // Short conditions evaluated below at step 3
        }

        // ── 2. Intrabar stop-loss / take-profit on THIS bar ─────────────────
        if (in_pos) {
            if (is_short) {
                lowest = std::min(lowest, bar.low);
                bool have_stop = false;
                double stop_price = 0.0;
                if (cfg.stop_loss_pct > 0) {
                    stop_price = entry_price * (1.0 + cfg.stop_loss_pct / 100.0);
                    have_stop = true;
                }
                if (cfg.trailing_stop_pct > 0) {
                    double trail = lowest * (1.0 + cfg.trailing_stop_pct / 100.0);
                    stop_price = have_stop ? std::min(stop_price, trail) : trail;
                    have_stop = true;
                }
                double tp_price = cfg.take_profit_pct > 0 ? entry_price * (1.0 - cfg.take_profit_pct / 100.0) : 0.0;
                if (have_stop && bar.high >= stop_price) {
                    close_trade(stop_price, "stop_loss", i);
                } else if (cfg.take_profit_pct > 0 && bar.low <= tp_price) {
                    close_trade(tp_price, "take_profit", i);
                }
            } else {
                highest = std::max(highest, bar.high);
                bool have_stop = false;
                double stop_price = 0.0;
                if (cfg.stop_loss_pct > 0) {
                    stop_price = entry_price * (1.0 - cfg.stop_loss_pct / 100.0);
                    have_stop = true;
                }
                if (cfg.trailing_stop_pct > 0) {
                    double trail = highest * (1.0 - cfg.trailing_stop_pct / 100.0);
                    stop_price = have_stop ? std::max(stop_price, trail) : trail;
                    have_stop = true;
                }
                double tp_price = cfg.take_profit_pct > 0 ? entry_price * (1.0 + cfg.take_profit_pct / 100.0) : 0.0;
                if (have_stop && bar.low <= stop_price) {
                    close_trade(stop_price, "stop_loss", i);
                } else if (cfg.take_profit_pct > 0 && bar.high >= tp_price) {
                    close_trade(tp_price, "take_profit", i);
                }
            }
        }

        // ── 3. Evaluate conditions on close of bar i → latch for next bar ───
        const int start = std::max(0, i - kEvalWindow + 1);
        const QVector<OhlcvCandle> window = candles.mid(start, i - start + 1);
        if (!in_pos && !entry_signal && !entry_conditions.isEmpty()) {
            const auto g = ConditionEvaluator::evaluate_group(entry_conditions, entry_logic, window);
            ++entry_eval_count;
            if (g.triggered) {
                entry_signal = true;
                ++entry_true_count;
            }
            for (const auto& d : g.details)
                if (!d.error.isEmpty()) {
                    ++entry_err_count;
                    if (last_entry_err.isEmpty())
                        last_entry_err = d.error;
                }
            if (!entry_sampled && !g.details.isEmpty() && !std::isnan(g.details.first().computed_value)) {
                entry_sampled = true;
                for (const auto& d : g.details)
                    LOG_INFO("Backtest",
                             QString("  entry[bar %1] %2.%3 %4  lhs=%5 rhs=%6 met=%7 err=%8")
                                 .arg(i).arg(d.indicator, d.field, d.op)
                                 .arg(d.computed_value).arg(d.target_value)
                                 .arg(d.met ? QStringLiteral("Y") : QStringLiteral("N"))
                                 .arg(d.error));
            }
        } else if (in_pos && !exit_signal && !exit_conditions.isEmpty()) {
            if (ConditionEvaluator::evaluate_group(exit_conditions, exit_logic, window).triggered) {
                exit_signal = true;
                ++exit_true_count;
            }
        }

        // ── 4. Mark-to-market equity on close ───────────────────────────────
        double position_value = 0.0;
        if (in_pos) {
            if (is_short)
                position_value = (entry_price - bar.close) * shares;
            else
                position_value = bar.close * shares;
        }
        const double equity = cash + position_value;
        equity_curve.append(equity);
        benchmark_curve.append(bench_base > 0 ? cfg.initial_capital * bar.close / bench_base : cfg.initial_capital);
        if (equity > peak_equity) peak_equity = equity;
        double dd = peak_equity > 0 ? (peak_equity - equity) / peak_equity * 100.0 : 0.0;
        if (dd > max_dd) max_dd = dd;
    }

    if (in_pos)
        close_trade(candles[n - 1].close, "end_of_data", n - 1);

    LOG_INFO("Backtest",
             QString("done: evalBars=%1 entryTrue=%2 exitTrue=%3 entryErr=%4 trades=%5 lastErr='%6'")
                 .arg(entry_eval_count).arg(entry_true_count).arg(exit_true_count)
                 .arg(entry_err_count).arg(trades.size()).arg(last_entry_err));

    const int total_trades = trades.size();
    const double final_value = cash;
    const double total_return = final_value - cfg.initial_capital;
    const double total_return_pct = cfg.initial_capital > 0 ? total_return / cfg.initial_capital * 100.0 : 0.0;

    QJsonObject out;
    out["success"] = true;

    if (total_trades == 0) {
        out["total_trades"] = 0; out["winning_trades"] = 0; out["losing_trades"] = 0;
        out["win_rate"] = 0.0; out["total_return"] = round_to(total_return_pct, 2);
        out["total_return_abs"] = round_to(total_return, 2);
        out["final_value"] = round_to(final_value, 2);
        out["max_drawdown"] = round_to(max_dd, 2);
        out["avg_pnl"] = 0.0; out["avg_bars_held"] = 0.0;
        out["profit_factor"] = 0.0; out["sharpe_ratio"] = 0.0; out["sharpe"] = 0.0;
        out["sortino"] = 0.0; out["calmar"] = 0.0; out["expectancy"] = 0.0;
        out["equity_curve"] = QJsonArray(); out["benchmark_curve"] = QJsonArray();
        out["benchmark_return"] = 0.0; out["monthly_returns"] = QJsonArray();
        out["trades"] = trades; out["config"] = QJsonObject{
            {"commission_pct", cfg.commission_pct},
            {"slippage_pct", cfg.slippage_pct},
            {"allow_short", cfg.allow_short}
        };
        return out;
    }

    int wins = 0;
    double gross_profit = 0.0, gross_loss = 0.0, sum_pnl = 0.0;
    long long sum_bars_held = 0;
    for (const auto& tv : trades) {
        const QJsonObject t = tv.toObject();
        const double pnl = t.value("pnl").toDouble();
        sum_pnl += pnl; sum_bars_held += t.value("bars_held").toInt();
        if (pnl > 0) { ++wins; gross_profit += pnl; }
        else { gross_loss += std::abs(pnl); }
    }
    int losses = total_trades - wins;
    double win_rate = static_cast<double>(wins) / total_trades * 100.0;
    double avg_pnl = sum_pnl / total_trades;
    double avg_bars_held = static_cast<double>(sum_bars_held) / total_trades;
    double profit_factor = (gross_loss > 0) ? gross_profit / gross_loss
                                            : (gross_profit > 0 ? 999.99 : 0.0);
    if (profit_factor > 999.99) profit_factor = 999.99;

    double sharpe = 0.0, sortino = 0.0;
    if (equity_curve.size() > 1) {
        QVector<double> rets;
        rets.reserve(equity_curve.size() - 1);
        for (int i = 1; i < equity_curve.size(); ++i)
            if (equity_curve[i - 1] != 0.0)
                rets.append((equity_curve[i] - equity_curve[i - 1]) / equity_curve[i - 1]);
        if (!rets.isEmpty()) {
            double mean = 0.0; for (double r : rets) mean += r; mean /= rets.size();
            double var = 0.0, dvar = 0.0;
            for (double r : rets) {
                var += (r - mean) * (r - mean);
                if (r < 0.0) dvar += r * r;
            }
            var /= rets.size(); dvar /= rets.size();
            double sd = std::sqrt(var), dsd = std::sqrt(dvar);
            double ann = std::sqrt(bars_per_year(timeframe));
            if (sd > 0.0) sharpe = (mean / sd) * ann;
            if (dsd > 0.0) sortino = (mean / dsd) * ann;
        }
    }
    double calmar = (max_dd > 0.0) ? (total_return_pct / max_dd) : 0.0;

    QJsonArray equity_out, benchmark_out;
    {
        int sz = equity_curve.size();
        int step = sz > 500 ? sz / 500 : 1;
        for (int i = 0; i < sz; i += step) {
            equity_out.append(round_to(equity_curve[i], 2));
            benchmark_out.append(round_to(benchmark_curve[i], 2));
        }
        if (sz > 0 && equity_out.last().toDouble() != round_to(equity_curve.last(), 2)) {
            equity_out.append(round_to(equity_curve.last(), 2));
            benchmark_out.append(round_to(benchmark_curve.last(), 2));
        }
    }

    QJsonArray monthly_returns;
    {
        QString cur_month; bool have_month = false;
        double month_end = cfg.initial_capital, prev_month_end = cfg.initial_capital;
        auto flush = [&](const QString& label) {
            double ret = prev_month_end > 0 ? (month_end - prev_month_end) / prev_month_end * 100.0 : 0.0;
            QJsonObject m; m["month"] = label; m["return"] = round_to(ret, 2);
            monthly_returns.append(m); prev_month_end = month_end;
        };
        for (int j = 0; j < equity_curve.size(); ++j) {
            QString ym = QDateTime::fromMSecsSinceEpoch(candles[kWarmupBars + j].open_time, Qt::UTC).toString("yyyy-MM");
            if (have_month && ym != cur_month) flush(cur_month);
            cur_month = ym; have_month = true; month_end = equity_curve[j];
        }
        if (have_month) flush(cur_month);
    }

    double bench_final = benchmark_curve.isEmpty() ? cfg.initial_capital : benchmark_curve.last();
    double bench_return_pct = cfg.initial_capital > 0 ? (bench_final - cfg.initial_capital) / cfg.initial_capital * 100.0 : 0.0;

    out["total_trades"] = total_trades;
    out["winning_trades"] = wins; out["losing_trades"] = losses;
    out["win_rate"] = round_to(win_rate, 1);
    out["total_return"] = round_to(total_return_pct, 2);
    out["total_return_abs"] = round_to(total_return, 2);
    out["final_value"] = round_to(final_value, 2);
    out["max_drawdown"] = round_to(max_dd, 2);
    out["avg_pnl"] = round_to(avg_pnl, 2);
    out["avg_bars_held"] = round_to(avg_bars_held, 1);
    out["profit_factor"] = round_to(profit_factor, 2);
    out["sharpe_ratio"] = round_to(sharpe, 3);
    out["sharpe"] = round_to(sharpe, 3);
    out["sortino"] = round_to(sortino, 3);
    out["calmar"] = round_to(calmar, 2);
    out["expectancy"] = round_to(avg_pnl, 2);
    out["equity_curve"] = equity_out;
    out["benchmark_curve"] = benchmark_out;
    out["benchmark_return"] = round_to(bench_return_pct, 2);
    out["monthly_returns"] = monthly_returns;
    out["trades"] = trades;
    out["config"] = QJsonObject{
        {"commission_pct", cfg.commission_pct},
        {"slippage_pct", cfg.slippage_pct},
        {"allow_short", cfg.allow_short}
    };
    return out;
}

// Backward-compatible overload
QJsonObject BacktestEngine::run(const QVector<OhlcvCandle>& candles,
                                const QJsonArray& entry_conditions, const QString& entry_logic,
                                const QJsonArray& exit_conditions, const QString& exit_logic,
                                double stop_loss_pct, double take_profit_pct, double trailing_stop_pct,
                                double initial_capital, const QString& timeframe,
                                double position_size_pct) {
    Config cfg;
    cfg.stop_loss_pct = stop_loss_pct;
    cfg.take_profit_pct = take_profit_pct;
    cfg.trailing_stop_pct = trailing_stop_pct;
    cfg.initial_capital = initial_capital;
    cfg.position_size_pct = position_size_pct;
    return run(candles, entry_conditions, entry_logic, exit_conditions, exit_logic, cfg, timeframe);
}

// ── Grid Search ──────────────────────────────────────────────────────────────────

QJsonObject BacktestEngine::grid_search(
    const QVector<OhlcvCandle>& candles,
    const QJsonArray& param_ranges,
    const QJsonArray& base_entry_conditions, const QString& entry_logic,
    const QJsonArray& base_exit_conditions, const QString& exit_logic,
    const Config& cfg, const QString& timeframe, int top_n)
{
    // Generate all parameter combinations
    struct ParamSet {
        QJsonObject params;
        QJsonObject result;
        double sharpe = -999.0;
    };

    // Parse ranges
    struct Range { QString name; double min, max, step; };
    QVector<Range> ranges;
    for (const auto& r : param_ranges) {
        QJsonObject o = r.toObject();
        ranges.push_back({o["name"].toString(), o["min"].toDouble(),
                          o["max"].toDouble(), o["step"].toDouble()});
    }

    if (ranges.isEmpty()) {
        QJsonObject err;
        err["success"] = false;
        err["error"] = "No parameter ranges specified";
        return err;
    }

    // Recursive parameter value generation
    QVector<QJsonObject> param_sets;
    std::function<void(int, QJsonObject)> gen = [&](int idx, QJsonObject current) {
        if (idx >= ranges.size()) {
            param_sets.push_back(current);
            return;
        }
        for (double v = ranges[idx].min; v <= ranges[idx].max; v += ranges[idx].step) {
            QJsonObject next = current;
            next[ranges[idx].name] = v;
            gen(idx + 1, next);
        }
    };
    gen(0, QJsonObject{});

    LOG_INFO("Backtest", QString("grid_search: %1 parameter combinations").arg(param_sets.size()));

    // Run backtest for each param set (substitute params into conditions)
    QVector<ParamSet> results;
    for (const auto& params : param_sets) {
        // Substitute parameter values in condition JSON
        std::function<QJsonArray(QJsonArray)> sub = [&](QJsonArray conds) -> QJsonArray {
            QJsonArray result;
            for (int ci = 0; ci < conds.size(); ++ci) {
                QJsonObject o = conds[ci].toObject();
                if (o.contains("params")) {
                    QJsonObject p = o["params"].toObject();
                    for (auto it = params.begin(); it != params.end(); ++it) {
                        if (p.contains(it.key())) {
                            p[it.key()] = it.value();
                        }
                    }
                    o["params"] = p;
                }
                if (o.contains("conditions")) {
                    o["conditions"] = sub(o["conditions"].toArray());
                }
                result.append(o);
            }
            return result;
        };

        QJsonObject result = run(candles,
                                 sub(base_entry_conditions), entry_logic,
                                 sub(base_exit_conditions), exit_logic,
                                 cfg, timeframe);
        double sharpe = result["sharpe"].toDouble(-999.0);
        if (sharpe > -999.0 && result["total_trades"].toInt() >= 5) {
            ParamSet ps;
            ps.params = params;
            ps.result = result;
            ps.sharpe = sharpe;
            results.push_back(ps);
        }
    }

    // Sort by Sharpe descending
    std::sort(results.begin(), results.end(), [](const ParamSet& a, const ParamSet& b) {
        return a.sharpe > b.sharpe;
    });

    // Take top N
    if (top_n > 0 && results.size() > top_n)
        results.resize(top_n);

    QJsonArray arr;
    for (const auto& ps : results) {
        QJsonObject entry;
        entry["params"] = ps.params;
        entry["sharpe"] = round_to(ps.sharpe, 3);
        entry["total_return"] = ps.result["total_return"].toDouble();
        entry["max_drawdown"] = ps.result["max_drawdown"].toDouble();
        entry["total_trades"] = ps.result["total_trades"].toInt();
        entry["win_rate"] = ps.result["win_rate"].toDouble();
        entry["profit_factor"] = ps.result["profit_factor"].toDouble();
        arr.append(entry);
    }

    QJsonObject out;
    out["success"] = true;
    out["results"] = arr;
    out["total_combinations"] = static_cast<int>(param_sets.size());
    out["total_evaluated"] = static_cast<int>(results.size());
    return out;
}

} // namespace fincept::algo
