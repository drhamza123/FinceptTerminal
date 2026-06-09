// src/algo_engine/BacktestEngine.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::algo {

/// Event-driven, single-symbol backtester with short selling, transaction costs,
/// and parameter grid search.
///
/// Pure computation: given candles + strategy parameters it returns a FLAT
/// metrics JSON object matching exactly what
/// `StrategyBuilderPanel::display_backtest_result` reads
/// (total_return, sharpe_ratio, max_drawdown, total_trades, win_rate,
///  profit_factor, final_value), plus equity_curve and the raw trade list.
///
/// Fill model (no look-ahead bias):
///   - Entry/exit SIGNALS are evaluated on the close of bar i.
///   - A signal fills at the OPEN of bar i+1.
///   - Stop-loss / take-profit (incl. trailing) are checked intrabar against
///     each bar's high/low, filling at the stop/target price.
///   - Commission deducted from cash on each fill as a % of trade value.
///   - Short selling: when allow_short is true, signals go both long and short.
///
/// On insufficient data the returned object has {"success": false, "error": …}.
class BacktestEngine {
public:
    struct Config {
        double stop_loss_pct = 0.0;
        double take_profit_pct = 0.0;
        double trailing_stop_pct = 0.0;
        double initial_capital = 100000.0;
        double position_size_pct = 100.0;
        double commission_pct = 0.0;    // e.g. 0.1 = 0.1% per trade
        double slippage_pct = 0.0;      // e.g. 0.05 = 0.05% slippage on fill
        bool   allow_short = false;
    };

    static QJsonObject run(const QVector<OhlcvCandle>& candles,
                           const QJsonArray& entry_conditions, const QString& entry_logic,
                           const QJsonArray& exit_conditions, const QString& exit_logic,
                           const Config& cfg, const QString& timeframe);

    /// Convenience overload for backward compatibility.
    static QJsonObject run(const QVector<OhlcvCandle>& candles,
                           const QJsonArray& entry_conditions, const QString& entry_logic,
                           const QJsonArray& exit_conditions, const QString& exit_logic,
                           double stop_loss_pct, double take_profit_pct, double trailing_stop_pct,
                           double initial_capital, const QString& timeframe,
                           double position_size_pct = 100.0);

    /// Grid search over parameter ranges. Returns JSON array of results sorted
    /// by Sharpe ratio descending.
    ///
    /// `param_ranges` format: [
    ///   {"name":"rs_period","min":5,"max":25,"step":5},
    ///   {"name":"sma_period","min":10,"max":200,"step":10}
    /// ]
    /// condition_serializers: functions that convert a param set → entry/exit conditions
    static QJsonObject grid_search(
        const QVector<OhlcvCandle>& candles,
        const QJsonArray& param_ranges,
        const QJsonArray& base_entry_conditions, const QString& entry_logic,
        const QJsonArray& base_exit_conditions, const QString& exit_logic,
        const Config& cfg, const QString& timeframe,
        int top_n = 20);
};

} // namespace fincept::algo
