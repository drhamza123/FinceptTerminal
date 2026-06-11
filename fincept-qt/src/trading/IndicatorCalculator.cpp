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

IchimokuResult IndicatorCalculator::ichimoku(const QVector<Candle>& candles) {
    IchimokuResult r; int n = candles.size();
    if (n < 52) return r;
    r.tenkan.resize(n); r.kijun.resize(n); r.senkou_a.resize(n); r.senkou_b.resize(n); r.chikou.resize(n);
    auto h = [&](int s, int l) { double v = -1e18; for (int i = s; i < s+l && i < n; ++i) v = std::max(v, candles[i].high); return v; };
    auto l = [&](int s, int l) { double v = 1e18; for (int i = s; i < s+l && i < n; ++i) v = std::min(v, candles[i].low); return v; };
    for (int i = 0; i < n; ++i) {
        r.tenkan[i] = i >= 8 ? (h(i-8,9)+l(i-8,9))/2.0 : 0;
        r.kijun[i] = i >= 25 ? (h(i-25,26)+l(i-25,26))/2.0 : 0;
        r.senkou_a[i] = i >= 25 ? (r.tenkan[i]+r.kijun[i])/2.0 : 0;
        r.senkou_b[i] = i >= 51 ? (h(i-51,52)+l(i-51,52))/2.0 : 0;
        r.chikou[i] = i+26 < n ? candles[i+26].close : 0;
    }
    return r;
}

AtrResult IndicatorCalculator::atr(const QVector<Candle>& candles, int period) {
    AtrResult r; int n = candles.size(); if (n < period+1) return r;
    r.values.resize(n);
    for (int i = 1; i < n; ++i) {
        double hl = candles[i].high - candles[i].low;
        double hc = std::abs(candles[i].high - candles[i-1].close);
        double lc = std::abs(candles[i].low - candles[i-1].close);
        double tr = std::max(hl, std::max(hc, lc));
        r.values[i] = (i <= period) ? (i==1 ? tr : (r.values[i-1]*(period-1)+tr)/period)
                                    : (r.values[i-1]*(period-1)+tr)/period;
    } return r;
}

ObvResult IndicatorCalculator::obv(const QVector<Candle>& candles) {
    ObvResult r; int n = candles.size(); if (n < 2) return r;
    r.values.resize(n); r.values[0] = 0;
    for (int i = 1; i < n; ++i) {
        if (candles[i].close > candles[i-1].close) r.values[i] = r.values[i-1] + candles[i].volume;
        else if (candles[i].close < candles[i-1].close) r.values[i] = r.values[i-1] - candles[i].volume;
        else r.values[i] = r.values[i-1];
    } return r;
}

AdxResult IndicatorCalculator::adx(const QVector<Candle>& candles, int period) {
    AdxResult r; int n = candles.size(); if (n < period*2) return r;
    r.adx.resize(n); r.plus_di.resize(n); r.minus_di.resize(n);
    QVector<double> tr(n,0), pdm(n,0), mdm(n,0);
    for (int i = 1; i < n; ++i) {
        double hl = candles[i].high-candles[i].low, hc = std::abs(candles[i].high-candles[i-1].close), lc = std::abs(candles[i].low-candles[i-1].close);
        tr[i] = std::max(hl, std::max(hc, lc));
        double up = candles[i].high-candles[i-1].high, dn = candles[i-1].low-candles[i].low;
        pdm[i] = (up>dn&&up>0)?up:0; mdm[i] = (dn>up&&dn>0)?dn:0;
    }
    QVector<double> sp(n,0), sm(n,0);
    for (int i = period; i < n; ++i) {
        double at = 0, sp_sum = 0, sm_sum = 0;
        for (int j = i-period+1; j <= i; ++j) { at+=tr[j]; sp_sum+=pdm[j]; sm_sum+=mdm[j]; }
        sp[i] = 100.0*sp_sum/(at==0?1:at); sm[i] = 100.0*sm_sum/(at==0?1:at);
    }
    for (int i = period; i < n; ++i) {
        double sum = 0;
        for (int j = i-period+1; j <= i; ++j) {
            double di = sp[j]+sm[j]; sum += (di==0)?0:100.0*std::abs(sp[j]-sm[j])/di;
        }
        r.adx[i] = sum/period; r.plus_di[i] = sp[i]; r.minus_di[i] = sm[i];
    } return r;
}

VolumeProfileResult IndicatorCalculator::volume_profile(const QVector<Candle>& candles, int num_bins) {
    VolumeProfileResult vp;
    if (candles.isEmpty()) return vp;
    vp.price_low = candles[0].low; vp.price_high = candles[0].high;
    for (const auto& c : candles) { vp.price_low = std::min(vp.price_low, c.low); vp.price_high = std::max(vp.price_high, c.high); }
    double bin_size = (vp.price_high - vp.price_low) / num_bins; if (bin_size <= 0) bin_size = 1;
    QVector<double> volumes(num_bins, 0);
    for (const auto& c : candles) {
        double range = c.high - c.low; if (range <= 0) continue;
        double vpp = c.volume / range;
        int sb = std::max(0, (int)((c.low - vp.price_low) / bin_size));
        int eb = std::min(num_bins - 1, (int)((c.high - vp.price_low) / bin_size));
        for (int b = sb; b <= eb; ++b) {
            double bl = vp.price_low + b * bin_size, bh = bl + bin_size;
            double ol = std::max(c.low, bl), oh = std::min(c.high, bh);
            volumes[b] += vpp * std::max(0.0, oh - ol);
        }
    }
    vp.levels.resize(num_bins); double max_vol = 0; int poc_bin = 0; double total = 0;
    for (int i = 0; i < num_bins; ++i) {
        vp.levels[i] = {vp.price_low + (i+0.5)*bin_size, volumes[i]};
        total += volumes[i]; if (volumes[i] > max_vol) { max_vol = volumes[i]; poc_bin = i; }
    }
    vp.poc_price = vp.price_low + (poc_bin+0.5)*bin_size;
    double cum = volumes[poc_bin]; int vl = poc_bin, vh = poc_bin; double target = total * 0.7;
    while (cum < target && (vl > 0 || vh < num_bins-1)) {
        double nl = vl > 0 ? volumes[vl-1] : -1, nh = vh < num_bins-1 ? volumes[vh+1] : -1;
        if (nl > nh && vl > 0) { cum += nl; --vl; } else if (vh < num_bins-1) { cum += nh; ++vh; } else break;
    }
    vp.val = vp.price_low + vl * bin_size; vp.vah = vp.price_low + (vh+1) * bin_size;
    return vp;
}

} // namespace fincept::trading
