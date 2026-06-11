#pragma once

#include <QVector>
#include <cmath>
#include <algorithm>
#include "trading/TradingTypes.h"

namespace fincept::trading {

struct RsiResult { QVector<double> values; int period = 14; };

struct MacdResult {
    QVector<double> macd, signal, histogram;
};

struct StochResult {
    QVector<double> k, d;
};

struct IchimokuResult {
    QVector<double> tenkan, kijun, senkou_a, senkou_b, chikou;
};

struct AtrResult { QVector<double> values; };

struct AdxResult {
    QVector<double> adx, plus_di, minus_di;
};

struct ObvResult { QVector<double> values; };

struct VolumeProfileResult {
    double price_low = 0, price_high = 0, poc_price = 0, vah = 0, val = 0;
    struct Level { double price, volume; };
    QVector<Level> levels;
};

struct IndicatorCalculator {
    static RsiResult rsi(const QVector<Candle>& candles, int period = 14);
    static MacdResult macd(const QVector<Candle>& candles, int fast = 12, int slow = 26, int signal = 9);
    static StochResult stochastic(const QVector<Candle>& candles, int k_period = 14, int k_smooth = 3, int d_period = 3);
    static IchimokuResult ichimoku(const QVector<Candle>& candles);
    static AtrResult atr(const QVector<Candle>& candles, int period = 14);
    static ObvResult obv(const QVector<Candle>& candles);
    static AdxResult adx(const QVector<Candle>& candles, int period = 14);
    static VolumeProfileResult volume_profile(const QVector<Candle>& candles, int num_bins = 50);
};

} // namespace fincept::trading
