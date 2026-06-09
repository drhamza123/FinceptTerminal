// src/algo_engine/RegimeDetector.h
#pragma once
#include <QString>
#include <QVector>

namespace fincept::algo {

/// Market regime classification for regime-adaptive strategies.
struct RegimeState {
    enum Type {
        Bull = 0,
        Bear = 1,
        Sideways = 2,
        HighVolatility = 3,
        Unknown = 4
    };
    Type type = Unknown;
    double volatility = 0.0;       // 20-day annualized volatility
    double trend_strength = 0.0;   // R-squared of linear regression 0-1
    double momentum = 0.0;         // 20-day return %
    int    consecutive_bars = 0;   // bars in current regime

    QString name() const;
    static QString type_name(Type t);
};

/// Detects market regime from OHLC price data.
///
/// Uses a combination of:
///   - Trend strength (regression R² on closing prices)
///   - Volatility regime (ATR / close %)
///   - Momentum (N-day return)
class RegimeDetector {
public:
    struct Config {
        int    volatility_period;
        int    trend_period;
        int    momentum_period;
        double high_vol_threshold;
        double trend_strength_min;
        double momentum_bull;
        double momentum_bear;

        static Config defaults() {
            Config c;
            c.volatility_period = 20;
            c.trend_period = 20;
            c.momentum_period = 20;
            c.high_vol_threshold = 0.03;
            c.trend_strength_min = 0.5;
            c.momentum_bull = 0.05;
            c.momentum_bear = -0.05;
            return c;
        }
    };

    explicit RegimeDetector(const Config& cfg = Config::defaults());

    /// Detect regime from a window of OHLC candles (newest last).
    RegimeState detect(const QVector<double>& closes) const;

    /// Rolling detection: call for each new close, maintains state.
    RegimeState update(double close, double high, double low);

    const Config& config() const { return cfg_; }

private:
    Config cfg_;
    QVector<double> closes_;
    QVector<double> highs_;
    QVector<double> lows_;
    RegimeState current_;
};

} // namespace fincept::algo
