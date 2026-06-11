#include "trading/IndicatorCalculator.h"

namespace fincept::trading {

RsiResult IndicatorCalculator::rsi(const QVector<Candle>& candles, int period) {
    RsiResult out;
    out.period = period;
    if (candles.size() < period + 1) return out;
    out.values.resize(candles.size());
    double avg_gain = 0, avg_loss = 0;
    for (int i = 1; i <= period; ++i) {
        double diff = candles[i].close - candles[i - 1].close;
        avg_gain += std::max(diff, 0.0);
        avg_loss += std::max(-diff, 0.0);
    }
    avg_gain /= period; avg_loss /= period;
    out.values[period] = 100 - (100 / (1 + avg_gain / (avg_loss == 0 ? 1e-10 : avg_loss)));
    for (int i = period + 1; i < candles.size(); ++i) {
        double diff = candles[i].close - candles[i - 1].close;
        avg_gain = (avg_gain * (period - 1) + std::max(diff, 0.0)) / period;
        avg_loss = (avg_loss * (period - 1) + std::max(-diff, 0.0)) / period;
        double rs = avg_gain / (avg_loss == 0 ? 1e-10 : avg_loss);
        out.values[i] = 100 - (100 / (1 + rs));
    }
    return out;
}

MacdResult IndicatorCalculator::macd(const QVector<Candle>& candles, int fast, int slow, int signal) {
    MacdResult out;
    int n = candles.size(); if (n < slow + signal) return out;
    auto ema = [&](int period) -> QVector<double> {
        QVector<double> e(n, 0); double k = 2.0 / (period + 1);
        double sum = 0; for (int i = 0; i < period; ++i) sum += candles[i].close;
        e[period - 1] = sum / period;
        for (int i = period; i < n; ++i) e[i] = (candles[i].close - e[i - 1]) * k + e[i - 1];
        return e;
    };
    auto ema_fast = ema(fast), ema_slow = ema(slow);
    out.macd.resize(n);
    for (int i = 0; i < n; ++i) out.macd[i] = ema_fast[i] - ema_slow[i];
    out.signal.resize(n);
    double sk = 2.0 / (signal + 1), ssum = 0;
    for (int i = 0; i < signal; ++i) ssum += out.macd[i];
    out.signal[signal - 1] = ssum / signal;
    for (int i = signal; i < n; ++i) out.signal[i] = (out.macd[i] - out.signal[i - 1]) * sk + out.signal[i - 1];
    out.histogram.resize(n);
    for (int i = 0; i < n; ++i) out.histogram[i] = out.macd[i] - out.signal[i];
    return out;
}

StochResult IndicatorCalculator::stochastic(const QVector<Candle>& candles, int k_period, int k_smooth, int d_period) {
    StochResult out;
    int n = candles.size(); if (n < k_period + d_period) return out;
    QVector<double> raw_k(n, 0);
    for (int i = k_period - 1; i < n; ++i) {
        double low = candles[i].low, high = candles[i].high;
        for (int j = i - k_period + 1; j <= i; ++j) {
            low = std::min(low, candles[j].low); high = std::max(high, candles[j].high);
        }
        double range = high - low;
        raw_k[i] = range == 0 ? 50 : (candles[i].close - low) / range * 100;
    }
    out.k.resize(n); out.d.resize(n);
    for (int i = k_period + k_smooth - 2; i < n; ++i) {
        double sum = 0; for (int j = 0; j < k_smooth; ++j) sum += raw_k[i - j];
        out.k[i] = sum / k_smooth;
    }
    double dk = 2.0 / (d_period + 1);
    int d_start = std::min(k_period + k_smooth + d_period - 3, n - 1);
    if (d_start >= 0) {
        double dsum = 0; int dc = 0;
        for (int i = std::max(0, d_start - d_period + 1); i <= d_start; ++i) { dsum += out.k[i]; ++dc; }
        out.d[d_start] = dc > 0 ? dsum / dc : 50;
        for (int i = d_start + 1; i < n; ++i) out.d[i] = (out.k[i] - out.d[i - 1]) * dk + out.d[i - 1];
    }
    return out;
}

} // namespace fincept::trading
