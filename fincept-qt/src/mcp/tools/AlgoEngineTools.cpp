// AlgoEngineTools.cpp — AI Agent → AlgoEngine bridge MCP tools
#include "mcp/tools/AlgoEngineTools.h"
#include "algo_engine/AlgoEngine.h"
#include "algo_engine/BacktestEngine.h"
#include "algo_engine/RegimeDetector.h"
#include "core/logging/Logger.h"
#include "mcp/ToolSchemaBuilder.h"
#include "services/markets/MarketDataService.h"
#include <condition_variable>
#include <mutex>

namespace fincept::mcp::tools {

using namespace fincept::algo;
using namespace fincept::services;

std::vector<ToolDef> get_algo_engine_tools() {
    std::vector<ToolDef> tools;

    // ── algo_run_backtest ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "algo_run_backtest";
        t.description = "Run backtest with transaction costs, short selling, and grid search optimization.";
        t.category = "analytics";
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Symbol to test (e.g. AAPL)").required()
            .string("timeframe", "Timeframe (1d,4h,1h,15m)").required()
            .number("capital", "Initial capital").default_num(100000.0)
            .number("commission_pct", "Commission % per trade").default_num(0.1)
            .number("slippage_pct", "Slippage % per fill").default_num(0.05)
            .boolean("allow_short", "Allow short selling").default_bool(false)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString symbol = args["symbol"].toString();
            QString tf = args["timeframe"].toString();
            double capital = args["capital"].toDouble(100000.0);
            // Fetch historical data synchronously via blocking callback
            std::mutex mtx;
            std::condition_variable cv;
            bool done = false;
            QJsonObject result;
            auto& mds = MarketDataService::instance();
            mds.fetch_history(symbol, "1y", tf,
                [&](bool ok, QVector<HistoryPoint> history) {
                    if (!ok || history.isEmpty()) {
                        result["error"] = "No data for " + symbol;
                    } else {
                        QVector<OhlcvCandle> candles;
                        for (const auto& hp : history) {
                            OhlcvCandle c;
                            c.open_time = hp.timestamp;
                            c.close_time = hp.timestamp;
                            c.open = hp.open;
                            c.high = hp.high;
                            c.low = hp.low;
                            c.close = hp.close;
                            c.volume = (double)hp.volume;
                            c.is_closed = true;
                            candles.push_back(c);
                        }
                        BacktestEngine::Config cfg;
                        cfg.initial_capital = capital;
                        cfg.commission_pct = args.value("commission_pct").toDouble(0.1);
                        cfg.slippage_pct = args.value("slippage_pct").toDouble(0.05);
                        cfg.allow_short = args.value("allow_short").toBool(false);
                        QJsonArray entry, exit;
                        result = BacktestEngine::run(candles, entry, "AND", exit, "AND", cfg, tf);
                    }
                    std::lock_guard lk(mtx);
                    done = true;
                    cv.notify_one();
                });
            std::unique_lock lk(mtx);
            cv.wait(lk, [&]{ return done; });
            if (!result.value("success").toBool())
                return ToolResult::fail(result.value("error").toString("Backtest failed"));
            return ToolResult::ok_data(result);
        };
        tools.push_back(t);
    }

    // ── algo_get_regime ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "algo_get_regime";
        t.description = "Detect market regime (bull/bear/sideways/high_vol) for a symbol using trend/volatility.";
        t.category = "analytics";
        t.input_schema = ToolSchemaBuilder()
            .string("symbol", "Symbol to analyze (e.g. AAPL, SPY)").required()
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString symbol = args["symbol"].toString();
            std::mutex mtx;
            std::condition_variable cv;
            bool done = false;
            QJsonObject out;
            auto& mds = MarketDataService::instance();
            mds.fetch_history(symbol, "3mo", "1d",
                [&](bool ok, QVector<HistoryPoint> history) {
                    if (ok && history.size() >= 20) {
                        QVector<double> closes;
                        for (const auto& hp : history)
                            closes.append(hp.close);
                        RegimeDetector detector;
                        RegimeState regime = detector.detect(closes);
                        out["symbol"] = symbol;
                        out["regime"] = regime.name();
                        out["volatility"] = regime.volatility;
                        out["trend_strength"] = regime.trend_strength;
                        out["momentum_pct"] = regime.momentum * 100.0;
                    }
                    std::lock_guard lk(mtx);
                    done = true;
                    cv.notify_one();
                });
            std::unique_lock lk(mtx);
            cv.wait(lk, [&]{ return done; });
            if (out.isEmpty())
                return ToolResult::fail("Insufficient data");
            return ToolResult::ok_data(out);
        };
        tools.push_back(t);
    }

    return tools;
}

} // namespace fincept::mcp::tools
