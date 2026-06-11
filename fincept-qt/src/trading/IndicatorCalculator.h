#pragma once

#include <QVector>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace fincept::trading {

struct Candle;

struct RsiResult {
    QVector<double> values;
    int period = 14;
};

struct MacdResult {
    QVector<double> macd;
    QVector<double> signal;
    QVector<double> histogram;
};

struct StochResult {
    QVector<double> k;  // fast %K
    QVector<double> d;  // slow %D
};

struct IchimokuResult {
    QVector<double> tenkan;
    QVector<double> kijun;
    QVector<double> senkou_a;
    QVector<double> senkou_b;
    QVector<double> chikou;
};

struct AtrResult {
    QVector<double> values;
};

struct AdxResult {
    QVector<double> adx;
    QVector<double> plus_di;
    QVector<double> minus_di;
};

struct ObvResult {
    QVector<double> values;
};

struct VolumeProfileResult {
    double price_low = 0;
    double price_high = 0;
    double poc_price = 0;   // Point of Control
    double vah = 0;         // Value Area High
    double val = 0;         // Value Area Low
    struct Level { double price; double volume; };
    QVector<Level> levels;
};

struct IndicatorCalculator {

    static RsiResult rsi(const QVector<Candle>& candles, int period = 14) {
        RsiResult out;
        out.period = period;
        if (candles.size() < period + 1) return out;

        out.values.resize(candles.size());

        // First average gain/loss
        double avg_gain = 0, avg_loss = 0;
        for (int i = 1; i <= period; ++i) {
            double diff = candles[i].close - candles[i - 1].close;
            avg_gain += std::max(diff, 0.0);
            avg_loss += std::max(-diff, 0.0);
        }
        avg_gain /= period;
        avg_loss /= period;
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

    static MacdResult macd(const QVector<Candle>& candles,
                           int fast = 12, int slow = 26, int signal = 9) {
        MacdResult out;
        if (candles.size() < slow + signal) return out;

        int n = candles.size();
        auto ema = [&](int period) -> QVector<double> {
            QVector<double> e(n, 0);
            double k = 2.0 / (period + 1);
            double sum = 0;
            for (int i = 0; i < period; ++i) sum += candles[i].close;
            e[period - 1] = sum / period;
            for (int i = period; i < n; ++i)
                e[i] = (candles[i].close - e[i - 1]) * k + e[i - 1];
            return e;
        };

        auto ema_fast = ema(fast);
        auto ema_slow = ema(slow);

        out.macd.resize(n);
        for (int i = 0; i < n; ++i) out.macd[i] = ema_fast[i] - ema_slow[i];

        // Signal line = EMA of MACD
        out.signal.resize(n);
        double sk = 2.0 / (signal + 1);
        double ssum = 0;
        for (int i = 0; i < signal; ++i) ssum += out.macd[i];
        out.signal[signal - 1] = ssum / signal;
        for (int i = signal; i < n; ++i)
            out.signal[i] = (out.macd[i] - out.signal[i - 1]) * sk + out.signal[i - 1];

        out.histogram.resize(n);
        for (int i = 0; i < n; ++i) out.histogram[i] = out.macd[i] - out.signal[i];

        return out;
    }

    static StochResult stochastic(const QVector<Candle>& candles,
                                  int k_period = 14, int k_smooth = 3, int d_period = 3) {
        StochResult out;
        if (candles.size() < k_period + d_period) return out;

        int n = candles.size();
        QVector<double> raw_k(n, 0);

        for (int i = k_period - 1; i < n; ++i) {
            double low = candles[i].low, high = candles[i].high;
            for (int j = i - k_period + 1; j <= i; ++j) {
                low = std::min(low, candles[j].low);
                high = std::max(high, candles[j].high);
            }
            double range = high - low;
            raw_k[i] = range == 0 ? 50 : (candles[i].close - low) / range * 100;
        }

        // Smooth %K
        out.k.resize(n);
        for (int i = k_period + k_smooth - 2; i < n; ++i) {
            double sum = 0;
            for (int j = 0; j < k_smooth; ++j) sum += raw_k[i - j];
            out.k[i] = sum / k_smooth;
        }

        // %D = EMA of %K
        out.d.resize(n);
        double dk = 2.0 / (d_period + 1);
        int d_start = k_period + k_smooth + d_period - 3;
        if (d_start >= n) d_start = k_period + k_smooth - 1;
        double dsum = 0;
        int dc = 0;
        for (int i = d_start - d_period + 1; i <= d_start; ++i) {
            if (i >= 0 && i < n) { dsum += out.k[i]; ++dc; }
        }
        out.d[d_start] = dc > 0 ? dsum / dc : 50;
        for (int i = d_start + 1; i < n; ++i)
            out.d[i] = (out.k[i] - out.d[i - 1]) * dk + out.d[i - 1];

        return out;
    }

    static VolumeProfileResult volume_profile(const QVector<Candle>& candles,
                                              int num_bins = 50) {
        VolumeProfileResult vp;
        if (candles.isEmpty()) return vp;

        vp.price_low = candles[0].low;
        vp.price_high = candles[0].high;
        for (const auto& c : candles) {
            vp.price_low = std::min(vp.price_low, c.low);
            vp.price_high = std::max(vp.price_high, c.high);
        }

        double bin_size = (vp.price_high - vp.price_low) / num_bins;
        if (bin_size <= 0) bin_size = 1;

        QVector<double> volumes(num_bins, 0);
        for (const auto& c : candles) {
            double range = c.high - c.low;
            if (range <= 0) continue;
            double vol_per_price = c.volume / range;
            int start_bin = std::max(0, (int)((c.low - vp.price_low) / bin_size));
            int end_bin = std::min(num_bins - 1, (int)((c.high - vp.price_low) / bin_size));
            for (int b = start_bin; b <= end_bin; ++b) {
                double bin_low = vp.price_low + b * bin_size;
                double bin_high = bin_low + bin_size;
                double overlap_low = std::max(c.low, bin_low);
                double overlap_high = std::min(c.high, bin_high);
                double overlap = std::max(0.0, overlap_high - overlap_low);
                volumes[b] += vol_per_price * overlap;
            }
        }

        vp.levels.resize(num_bins);
        double max_vol = 0;
        int poc_bin = 0;
        double total_vol = 0;
        for (int i = 0; i < num_bins; ++i) {
            double price = vp.price_low + (i + 0.5) * bin_size;
            vp.levels[i] = {price, volumes[i]};
            total_vol += volumes[i];
            if (volumes[i] > max_vol) {
                max_vol = volumes[i];
                poc_bin = i;
            }
        }

        vp.poc_price = vp.price_low + (poc_bin + 0.5) * bin_size;

        // Value Area = 70% of total volume around POC
        double cum_vol = volumes[poc_bin];
        int va_low = poc_bin, va_high = poc_bin;
        double target = total_vol * 0.7;
        while (cum_vol < target && (va_low > 0 || va_high < num_bins - 1)) {
            double next_low = va_low > 0 ? volumes[va_low - 1] : -1;
            double next_high = va_high < num_bins - 1 ? volumes[va_high + 1] : -1;
            if (next_low > next_high && va_low > 0) {
                cum_vol += next_low; --va_low;
            } else if (va_high < num_bins - 1) {
                cum_vol += next_high; ++va_high;
            } else break;
        }

        vp.val = vp.price_low + va_low * bin_size;
        vp.vah = vp.price_low + (va_high + 1) * bin_size;

        return vp;
    }
};

    static IchimokuResult ichimoku(const QVector<Candle>& candles) {
        IchimokuResult r;
        int n = candles.size();
        if (n < 52) return r;
        r.tenkan.resize(n); r.kijun.resize(n);
        r.senkou_a.resize(n); r.senkou_b.resize(n);
        r.chikou.resize(n);
        auto highest = [&](int start, int len) {
            double h = -1e18;
            for (int i = start; i < start + len && i < n; ++i) h = std::max(h, candles[i].high);
            return h;
        };
        auto lowest = [&](int start, int len) {
            double l = 1e18;
            for (int i = start; i < start + len && i < n; ++i) l = std::min(l, candles[i].low);
            return l;
        };
        for (int i = 0; i < n; ++i) {
            r.tenkan[i] = i >= 8 ? (highest(i-8,9)+lowest(i-8,9))/2.0 : 0;
            r.kijun[i] = i >= 25 ? (highest(i-25,26)+lowest(i-25,26))/2.0 : 0;
            r.senkou_a[i] = i >= 25 ? (r.tenkan[i]+r.kijun[i])/2.0 : 0;
            r.senkou_b[i] = i >= 51 ? (highest(i-51,52)+lowest(i-51,52))/2.0 : 0;
            r.chikou[i] = i+26 < n ? candles[i+26].close : 0;
        }
        return r;
    }

    static AtrResult atr(const QVector<Candle>& candles, int period = 14) {
        AtrResult r;
        int n = candles.size(); if (n < period+1) return r;
        r.values.resize(n);
        for (int i = 1; i < n; ++i) {
            double hl = candles[i].high - candles[i].low;
            double hc = std::abs(candles[i].high - candles[i-1].close);
            double lc = std::abs(candles[i].low - candles[i-1].close);
            double tr = std::max({hl, hc, lc});
            r.values[i] = (i <= period) ? (i==1 ? tr : (r.values[i-1]*(period-1)+tr)/period)
                                        : (r.values[i-1]*(period-1)+tr)/period;
        }
        return r;
    }

    static ObvResult obv(const QVector<Candle>& candles) {
        ObvResult r; int n = candles.size(); if (n < 2) return r;
        r.values.resize(n); r.values[0] = 0;
        for (int i = 1; i < n; ++i) {
            if (candles[i].close > candles[i-1].close) r.values[i] = r.values[i-1] + candles[i].volume;
            else if (candles[i].close < candles[i-1].close) r.values[i] = r.values[i-1] - candles[i].volume;
            else r.values[i] = r.values[i-1];
        }
        return r;
    }

    static AdxResult adx(const QVector<Candle>& candles, int period = 14) {
        AdxResult r;
        int n = candles.size();
        if (n < period * 2) return r;
        r.adx.resize(n); r.plus_di.resize(n); r.minus_di.resize(n);
        QVector<double> tr(n, 0), pdm(n, 0), mdm(n, 0);
        for (int i = 1; i < n; ++i) {
            double hl = candles[i].high - candles[i].low;
            double hc = std::abs(candles[i].high - candles[i - 1].close);
            double lc = std::abs(candles[i].low - candles[i - 1].close);
            tr[i] = std::max(hl, std::max(hc, lc));
            double up = candles[i].high - candles[i - 1].high;
            double dn = candles[i - 1].low - candles[i].low;
            pdm[i] = (up > dn && up > 0) ? up : 0;
            mdm[i] = (dn > up && dn > 0) ? dn : 0;
        }
        QVector<double> sp(n, 0), sm(n, 0);
        for (int i = period; i < n; ++i) {
            double at = 0, sp_sum = 0, sm_sum = 0;
            for (int j = i - period + 1; j <= i; ++j) {
                at += tr[j]; sp_sum += pdm[j]; sm_sum += mdm[j];
            }
            sp[i] = 100.0 * sp_sum / (at == 0 ? 1 : at);
            sm[i] = 100.0 * sm_sum / (at == 0 ? 1 : at);
        }
        for (int i = period; i < n; ++i) {
            double sum = 0;
            for (int j = i - period + 1; j <= i; ++j) {
                double di_sum = sp[j] + sm[j];
                double dx = (di_sum == 0) ? 0 : 100.0 * std::abs(sp[j] - sm[j]) / di_sum;
                sum += dx;
            }
            r.adx[i] = sum / period;
            r.plus_di[i] = sp[i];
            r.minus_di[i] = sm[i];
        }
        return r;
    }
};

} // namespace fincept::trading
