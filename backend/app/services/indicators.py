"""Technical indicator calculations for market data endpoints."""
import numpy as np


def sma(data, n):
    s = np.cumsum(data, dtype=float)
    if n >= len(data):
        return np.full_like(data, s[-1] / len(data))
    s[n:] = s[n:] - s[:-n]
    return s / n


def ema(data, n):
    result = np.copy(data).astype(float)
    k = 2.0 / (n + 1)
    for i in range(1, len(data)):
        result[i] = data[i] * k + result[i-1] * (1 - k)
    return result


def wma(data, n):
    weights = np.arange(1, n + 1)
    result = np.zeros_like(data)
    for i in range(n-1, len(data)):
        result[i] = np.dot(data[i-n+1:i+1], weights) / weights.sum()
    return result


def rsi(data, n):
    deltas = np.diff(data)
    gains = np.where(deltas > 0, deltas, 0)
    losses = np.where(deltas < 0, -deltas, 0)
    avg_gain = np.zeros_like(data)
    avg_loss = np.zeros_like(data)
    avg_gain[n] = np.mean(gains[:n])
    avg_loss[n] = np.mean(losses[:n])
    for i in range(n+1, len(data)):
        avg_gain[i] = (avg_gain[i-1] * (n-1) + gains[i-1]) / n
        avg_loss[i] = (avg_loss[i-1] * (n-1) + losses[i-1]) / n
    rs = np.where(avg_loss != 0, avg_gain / avg_loss, 100)
    return 100 - (100 / (1 + rs))


def macd(data, fast=12, slow=26, signal=9):
    ema_fast = ema(data, fast)
    ema_slow = ema(data, slow)
    macd_line = ema_fast - ema_slow
    signal_line = ema(macd_line, signal)
    histogram = macd_line - signal_line
    return macd_line, signal_line, histogram


def stoch(high, low, close, k=14, d=3):
    low_k = np.array([np.min(low[max(0,i-k+1):i+1]) for i in range(len(low))])
    high_k = np.array([np.max(high[max(0,i-k+1):i+1]) for i in range(len(high))])
    k_line = np.where(high_k != low_k, (close - low_k) / (high_k - low_k) * 100, 50)
    d_line = sma(k_line, d)
    return k_line, d_line


def adx(high, low, close, n=14):
    up_move = np.diff(high)
    down_move = np.diff(low)
    plus_dm = np.where((up_move > down_move) & (up_move > 0), up_move, 0)
    minus_dm = np.where((down_move > up_move) & (down_move > 0), down_move, 0)
    tr = np.zeros_like(close)
    tr[0] = high[0] - low[0]
    for i in range(1, len(close)):
        tr[i] = max(high[i]-low[i], abs(high[i]-close[i-1]), abs(low[i]-close[i-1]))
    atr_val = sma(tr, n)
    plus_di = np.where(atr_val != 0, sma(np.append([0], plus_dm), n) / atr_val * 100, 0)
    minus_di = np.where(atr_val != 0, sma(np.append([0], minus_dm), n) / atr_val * 100, 0)
    dx = np.abs(plus_di - minus_di) / np.where(plus_di + minus_di != 0, plus_di + minus_di, 1) * 100
    return sma(dx, n), plus_di, minus_di


def cci(high, low, close, n=20):
    tp = (high + low + close) / 3
    sma_tp = sma(tp, n)
    mad = np.array([np.mean(np.abs(tp[max(0,i-n+1):i+1] - sma_tp[i])) for i in range(len(tp))])
    return np.where(mad != 0, (tp - sma_tp) / (0.015 * mad), 0)


def williams_r(high, low, close, n=14):
    high_n = np.array([np.max(high[max(0,i-n+1):i+1]) for i in range(len(high))])
    low_n = np.array([np.min(low[max(0,i-n+1):i+1]) for i in range(len(low))])
    return np.where(high_n != low_n, (high_n - close) / (high_n - low_n) * -100, -50)


def obv(close, volume):
    obv_vals = np.zeros_like(close)
    for i in range(1, len(close)):
        if close[i] > close[i-1]:
            obv_vals[i] = obv_vals[i-1] + volume[i]
        elif close[i] < close[i-1]:
            obv_vals[i] = obv_vals[i-1] - volume[i]
        else:
            obv_vals[i] = obv_vals[i-1]
    return obv_vals


def vwap(high, low, close, volume):
    """Volume Weighted Average Price (cumulative)."""
    typical = (high + low + close) / 3
    cum_vol = np.cumsum(volume)
    cum_pv = np.cumsum(typical * volume)
    return np.where(cum_vol != 0, cum_pv / cum_vol, typical)


def vwma(close, volume, n=20):
    """Volume Weighted Moving Average."""
    p = np.minimum(n, len(close))
    result = np.zeros_like(close)
    for i in range(p - 1, len(close)):
        result[i] = np.sum(close[i-p+1:i+1] * volume[i-p+1:i+1]) / np.sum(volume[i-p+1:i+1])
    return result


def hma(data, n=20):
    """Hull Moving Average."""
    p = np.minimum(n, len(data))
    half = ema(data, p // 2)
    sqrt_n = ema(data, p)
    hma_raw = 2 * half - sqrt_n
    return ema(hma_raw, int(np.sqrt(p)))


def tema(data, n=20):
    """Triple Exponential Moving Average."""
    p = np.minimum(n, len(data))
    e1 = ema(data, p)
    e2 = ema(e1, p)
    e3 = ema(e2, p)
    return 3 * e1 - 3 * e2 + e3


def dema(data, n=20):
    """Double Exponential Moving Average."""
    p = np.minimum(n, len(data))
    e1 = ema(data, p)
    e2 = ema(e1, p)
    return 2 * e1 - e2


def zlema(data, n=20):
    """Zero Lag Exponential Moving Average."""
    p = np.minimum(n, len(data))
    lag = (p - 1) // 2
    zl_data = data + (data - np.roll(data, lag))
    zl_data[:lag] = data[:lag]
    return ema(zl_data, p)


def alma(data, n=20, offset=0.85, sigma=6):
    """Arnaud Legoux Moving Average."""
    p = np.minimum(n, len(data))
    m = offset * (p - 1)
    s = p / sigma
    weights = np.array([np.exp(-((i - m) ** 2) / (2 * s * s)) for i in range(p)])
    weights /= weights.sum()
    result = np.zeros_like(data)
    for i in range(p - 1, len(data)):
        result[i] = np.dot(data[i-p+1:i+1], weights)
    return result


def kama(data, n=20, fast=2, slow=30):
    """Kaufman Adaptive Moving Average."""
    p = np.minimum(n, len(data))
    result = np.copy(data).astype(float)
    for i in range(p, len(data)):
        change = abs(data[i] - data[i - p])
        volatility = np.sum(np.abs(data[i-p+1:i+1] - data[i-p:i]))
        er = change / volatility if volatility != 0 else 0
        sc = (er * (2.0 / (fast + 1) - 2.0 / (slow + 1)) + 2.0 / (slow + 1)) ** 2
        result[i] = result[i-1] + sc * (data[i] - result[i-1])
    return result


def psar(high, low, accel_init=0.02, accel_max=0.2):
    """Parabolic SAR."""
    n = len(high)
    sar = np.zeros(n)
    ep = np.zeros(n)
    trend = np.ones(n)  # 1 up, -1 down
    af = np.full(n, accel_init)
    
    # First bar
    trend[0] = 1 if high[0] < high[1] else -1
    ep[0] = max(high[0], high[1]) if trend[0] > 0 else min(low[0], low[1])
    sar[0] = low[0] if trend[0] > 0 else high[0]
    
    for i in range(1, n):
        sar[i] = sar[i-1] + af[i-1] * (ep[i-1] - sar[i-1])
        sar[i] = min(sar[i], min(low[i-1:i+1])) if trend[i-1] > 0 else max(sar[i], max(high[i-1:i+1]))
        
        if trend[i-1] > 0 and low[i] < sar[i]:
            trend[i] = -1
            sar[i] = ep[i-1]
            ep[i] = low[i]
            af[i] = accel_init
        elif trend[i-1] < 0 and high[i] > sar[i]:
            trend[i] = 1
            sar[i] = ep[i-1]
            ep[i] = high[i]
            af[i] = accel_init
        else:
            trend[i] = trend[i-1]
            ep[i] = ep[i-1] if trend[i] > 0 else max(ep[i-1], high[i]) if trend[i] < 0 else ep[i-1]
            trend_d = trend[i-1]
            new_high = high[i] > ep[i-1] if trend_d > 0 else False
            new_low = low[i] < ep[i-1] if trend_d < 0 else False
            if new_high or new_low:
                af[i] = min(af[i-1] + accel_init, accel_max)
                ep[i] = high[i] if trend_d > 0 else low[i]
            else:
                af[i] = af[i-1]
                ep[i] = ep[i-1]
    
    return sar, trend


def ichimoku(high, low, close, tenkan=9, kijun=26, senkou=52):
    """Ichimoku Cloud components."""
    n = len(high)
    tenkan_line = np.full(n, np.nan)
    kijun_line = np.full(n, np.nan)
    senkou_a = np.full(n, np.nan)
    senkou_b = np.full(n, np.nan)
    chikou = np.full(n, np.nan)
    
    for i in range(n):
        if i >= tenkan - 1:
            tenkan_line[i] = (np.max(high[i-tenkan+1:i+1]) + np.min(low[i-tenkan+1:i+1])) / 2
        if i >= kijun - 1:
            kijun_line[i] = (np.max(high[i-kijun+1:i+1]) + np.min(low[i-kijun+1:i+1])) / 2
        if i >= senkou - 1:
            senkou_b[i] = (np.max(high[i-senkou+1:i+1]) + np.min(low[i-senkou+1:i+1])) / 2
    # Senkou A is the average of tenkan and kijun, shifted forward
    for i in range(tenkan - 1, n):
        if not np.isnan(tenkan_line[i]) and not np.isnan(kijun_line[i]):
            val = (tenkan_line[i] + kijun_line[i]) / 2
            if i + kijun < n:
                senkou_a[i + kijun] = val
    # Chikou is close shifted backward
    for i in range(n - kijun):
        chikou[i] = close[i + kijun]
    
    return tenkan_line, kijun_line, senkou_a, senkou_b, chikou


def heiken_ashi(open_p, high, low, close):
    """Heiken Ashi candles."""
    ha_close = (open_p + high + low + close) / 4
    ha_open = np.zeros_like(open_p)
    ha_open[0] = open_p[0]
    for i in range(1, len(open_p)):
        ha_open[i] = (ha_open[i-1] + ha_close[i-1]) / 2
    ha_high = np.maximum(high, np.maximum(ha_open, ha_close))
    ha_low = np.minimum(low, np.minimum(ha_open, ha_close))
    return ha_open, ha_high, ha_low, ha_close


def pivot_points(high, low, close, method='standard'):
    """Pivot Points (daily)."""
    n = len(high)
    pp = np.zeros(n)
    r1, r2, r3 = np.zeros(n), np.zeros(n), np.zeros(n)
    s1, s2, s3 = np.zeros(n), np.zeros(n), np.zeros(n)
    
    for i in range(1, n):
        if method == 'standard':
            pp[i] = (high[i-1] + low[i-1] + close[i-1]) / 3
            r1[i] = 2 * pp[i] - low[i-1]
            r2[i] = pp[i] + (high[i-1] - low[i-1])
            r3[i] = high[i-1] + 2 * (pp[i] - low[i-1])
            s1[i] = 2 * pp[i] - high[i-1]
            s2[i] = pp[i] - (high[i-1] - low[i-1])
            s3[i] = low[i-1] - 2 * (high[i-1] - pp[i])
    
    return pp, r1, r2, r3, s1, s2, s3


def linear_regression(data, n=20):
    """Linear Regression line and slope."""
    p = np.minimum(n, len(data))
    x = np.arange(p)
    result = np.zeros_like(data)
    slope = np.zeros_like(data)
    for i in range(p - 1, len(data)):
        y = data[i-p+1:i+1]
        slope[i] = (p * np.sum(x * y) - np.sum(x) * np.sum(y)) / (p * np.sum(x ** 2) - np.sum(x) ** 2)
        intercept = (np.sum(y) - slope[i] * np.sum(x)) / p
        result[i] = intercept + slope[i] * (p - 1)
    return result, slope


def mfi(high, low, close, volume, n=14):
    typical = (high + low + close) / 3
    raw_money = typical * volume
    mf_ratio = np.ones_like(close) * 100
    for i in range(n, len(close)):
        start = i - n + 1
        pos = np.sum(raw_money[start:i+1][typical[start:i+1] > typical[start-1:i]])
        neg = np.sum(raw_money[start:i+1][typical[start:i+1] < typical[start-1:i]])
        mf_ratio[i] = pos / neg if neg != 0 else 100
    return 100 - (100 / (1 + mf_ratio))


def force_index(close, volume, n=13):
    """Elder's Force Index."""
    fi = np.zeros_like(close)
    fi[1:] = (close[1:] - close[:-1]) * volume[1:]
    return ema(fi, np.minimum(n, len(close)))


def chaikin_mf(high, low, close, volume, n=20):
    """Chaikin Money Flow."""
    p = np.minimum(n, len(close))
    mf = np.where(high != low, volume * ((close - low) - (high - close)) / (high - low), 0)
    result = np.zeros_like(close)
    for i in range(p - 1, len(close)):
        result[i] = np.sum(mf[i-p+1:i+1]) / np.sum(volume[i-p+1:i+1]) if np.sum(volume[i-p+1:i+1]) != 0 else 0
    return result


def ease_of_movement(high, low, close, volume, n=14):
    """Ease of Movement."""
    p = np.minimum(n, len(close))
    distance = np.zeros_like(close)
    distance[1:] = (high[1:] + low[1:]) / 2 - (high[:-1] + low[:-1]) / 2
    box_ratio = np.where(volume != 0, volume / (high - low) / 100000000, 0)
    emv = np.where(box_ratio != 0, distance / box_ratio, 0)
    return ema(emv, p)
    typical = (high + low + close) / 3
    raw_money = typical * volume
    mf_ratio = np.ones_like(close) * 100
    for i in range(n, len(close)):
        start = i - n + 1
        pos = np.sum(raw_money[start:i+1][typical[start:i+1] > typical[start-1:i]])
        neg = np.sum(raw_money[start:i+1][typical[start:i+1] < typical[start-1:i]])
        mf_ratio[i] = pos / neg if neg != 0 else 100
    return 100 - (100 / (1 + mf_ratio))


def roc(close, n=12):
    result = np.zeros_like(close)
    result[n:] = (close[n:] - close[:-n]) / close[:-n] * 100
    return result


def atr(high, low, close, n=14):
    tr = np.zeros_like(close)
    tr[0] = high[0] - low[0]
    for i in range(1, len(close)):
        tr[i] = max(high[i]-low[i], abs(high[i]-close[i-1]), abs(low[i]-close[i-1]))
    return sma(tr, n)


def keltner(high, low, close, n=20, mult=1.5):
    middle = ema(close, n)
    atr_val = atr(high, low, close, n)
    upper = middle + mult * atr_val
    lower = middle - mult * atr_val
    return upper, middle, lower


def compute_all(closes, highs, lows, volumes):
    """Compute ALL indicators and return as dict."""
    n = len(closes)
    I = {}
    
    # MAs
    for period, name in [(5,5),(9,9),(10,10),(12,12),(20,20),(21,21),(26,26),(50,50),(200,200)]:
        p = min(period, n)
        I[f'ema{name}'] = ema(closes, p)
        I[f'sma{name}'] = sma(closes, p)
    
    for period, name in [(14,14),(20,20)]:
        p = min(period, n)
        I[f'wma{name}'] = wma(closes, p)
    
    # Oscillators
    for period, name in [(7,7),(14,14),(21,21)]:
        p = min(period, n)
        I[f'rsi{name}'] = rsi(closes, p)
    
    k, d = stoch(highs, lows, closes, 14, 3)
    I['stoch_k'] = k
    I['stoch_d'] = d
    
    I['williams_r'] = williams_r(highs, lows, closes, 14)
    
    for period, name in [(20,20),(50,50)]:
        p = min(period, n)
        I[f'cci{name}'] = cci(highs, lows, closes, p)
    
    I['mfi14'] = mfi(highs, lows, closes, volumes, 14)
    I['roc12'] = roc(closes, 12)
    I['roc25'] = roc(closes, 25)
    
    # Trend
    adx14, di14p, di14n = adx(highs, lows, closes, 14)
    adx21, di21p, di21n = adx(highs, lows, closes, 21)
    I['adx14'] = adx14
    I['plus_di14'] = di14p
    I['minus_di14'] = di14n
    I['adx21'] = adx21
    I['plus_di21'] = di21p
    I['minus_di21'] = di21n
    
    macd_l, macd_s, macd_h = macd(closes)
    I['macd'] = macd_l
    I['macd_signal'] = macd_s
    I['macd_hist'] = macd_h
    
    # Volatility
    I['atr14'] = atr(highs, lows, closes, 14)
    
    bb_sma = sma(closes, min(20, n))
    bb_std = np.array([np.std(closes[max(0,i-19):i+1]) for i in range(n)])
    I['bb_upper'] = bb_sma + 2 * bb_std
    I['bb_lower'] = bb_sma - 2 * bb_std
    I['bb_middle'] = bb_sma
    I['bb_width'] = np.where(bb_sma != 0, (I['bb_upper'] - I['bb_lower']) / bb_sma * 100, 0)
    I['bb_pct_b'] = np.where(I['bb_upper'] != I['bb_lower'], (closes - I['bb_lower']) / (I['bb_upper'] - I['bb_lower']) * 100, 50)
    
    ku, km, kl = keltner(highs, lows, closes)
    I['kc_upper'] = ku
    I['kc_middle'] = km
    I['kc_lower'] = kl
    
    # Volume
    I['obv'] = obv(closes, volumes)
    
    # Advanced MAs
    I['hma20'] = hma(closes, 20)
    I['hma50'] = hma(closes, 50)
    I['tema20'] = tema(closes, 20)
    I['dema20'] = dema(closes, 20)
    I['zlema20'] = zlema(closes, 20)
    I['alma20'] = alma(closes, 20)
    I['kama20'] = kama(closes, 20)
    I['vwma20'] = vwma(closes, volumes, 20)
    I['vwap'] = vwap(highs, lows, closes, volumes)
    
    # Parabolic SAR
    psar_vals, psar_trend = psar(highs, lows)
    I['psar'] = psar_vals
    I['psar_trend'] = psar_trend
    
    # Ichimoku
    tenkan, kijun, senkou_a, senkou_b, chikou = ichimoku(highs, lows, closes)
    I['ichimoku_tenkan'] = tenkan
    I['ichimoku_kijun'] = kijun
    I['ichimoku_senkou_a'] = senkou_a
    I['ichimoku_senkou_b'] = senkou_b
    I['ichimoku_chikou'] = chikou
    
    # Heiken Ashi
    ha_o, ha_h, ha_l, ha_c = heiken_ashi(closes, highs, lows, closes)
    I['ha_open'] = ha_o
    I['ha_high'] = ha_h
    I['ha_low'] = ha_l
    I['ha_close'] = ha_c
    
    # Pivot Points
    pp, r1, r2, r3, s1, s2, s3 = pivot_points(highs, lows, closes)
    I['pivot'] = pp
    I['pivot_r1'] = r1
    I['pivot_r2'] = r2
    I['pivot_r3'] = r3
    I['pivot_s1'] = s1
    I['pivot_s2'] = s2
    I['pivot_s3'] = s3
    
    # Linear Regression
    lr, lr_slope = linear_regression(closes, 20)
    I['linear_reg'] = lr
    I['linear_reg_slope'] = lr_slope
    
    # Additional oscillators
    I['force_index13'] = force_index(closes, volumes, 13)
    I['chaikin_mf20'] = chaikin_mf(highs, lows, closes, volumes, 20)
    I['ease_of_movement14'] = ease_of_movement(highs, lows, closes, volumes, 14)
    
    return I
