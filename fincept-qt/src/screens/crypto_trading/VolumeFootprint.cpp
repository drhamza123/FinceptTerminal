#include "screens/crypto_trading/VolumeFootprint.h"

#include <QPainter>
#include <QDateTime>
#include <algorithm>
#include <cmath>
#include <QDateTime>

namespace fincept::screens::crypto {

VolumeFootprint::VolumeFootprint(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(80);
}

void VolumeFootprint::clear() {
    QMutexLocker lock(&mutex_);
    footprints_.clear();
    max_volume_ = 0;
    update();
}

void VolumeFootprint::add_candle(const trading::Candle& candle) {
    QMutexLocker lock(&mutex_);
    FootprintCandle fp;
    fp.timestamp = candle.timestamp;
    fp.open = candle.open;
    fp.high = candle.high;
    fp.low = candle.low;
    fp.close = candle.close;
    compute_footprint(candle, fp);
    footprints_.append(fp);
    if (footprints_.size() > max_visible_ * 2)
        footprints_.remove(0, footprints_.size() - max_visible_);
    update();
}

void VolumeFootprint::set_candles(const QVector<trading::Candle>& candles) {
    QMutexLocker lock(&mutex_);
    footprints_.clear();
    max_volume_ = 0;
    for (const auto& c : candles) {
        FootprintCandle fp;
        fp.timestamp = c.timestamp;
        fp.open = c.open; fp.high = c.high; fp.low = c.low; fp.close = c.close;
        compute_footprint(c, fp);
        footprints_.append(fp);
    }
    update();
}

void VolumeFootprint::compute_footprint(const trading::Candle& candle, FootprintCandle& fp) {
    // Estimate bid/ask split from candle's OHLC relationship
    // When close > open: buying pressure (bullish), ask volume > bid volume
    // When close < open: selling pressure (bearish), bid volume > ask volume
    double range = candle.high - candle.low;
    if (range <= 0) range = 1;

    double bull_ratio = (candle.close - candle.low) / range;
    double bear_ratio = (candle.high - candle.close) / range;
    if (bull_ratio < 0.1) bull_ratio = 0.1;
    if (bear_ratio < 0.1) bear_ratio = 0.1;
    double total = bull_ratio + bear_ratio;
    bull_ratio /= total;
    bear_ratio /= total;

    fp.total_bid = candle.volume * bear_ratio;
    fp.total_ask = candle.volume * bull_ratio;
    fp.net_delta = fp.total_ask - fp.total_bid;

    // Generate synthetic price levels for histogram rendering
    int num_levels = std::min(10, std::max(3, (int)(range * 10)));
    double step = range / num_levels;
    for (int i = 0; i < num_levels; i++) {
        VolumeLevel vl;
        vl.price = candle.low + step * i;
        double level_bid = fp.total_bid / num_levels;
        double level_ask = fp.total_ask / num_levels;
        // Add some noise for visual realism
        double noise = 0.8 + 0.4 * ((double)(i * 7 % 13) / 13.0);
        if (i < num_levels / 2) {
            level_bid *= noise;
            level_ask *= (2.0 - noise);
        } else {
            level_ask *= noise;
            level_bid *= (2.0 - noise);
        }
        vl.bid_volume = level_bid;
        vl.ask_volume = level_ask;
        vl.delta = level_ask - level_bid;
        vl.total_volume = level_bid + level_ask;
        fp.levels.append(vl);
        if (vl.total_volume > max_volume_) max_volume_ = vl.total_volume;
    }
}

void VolumeFootprint::paintEvent(QPaintEvent*) {
    QMutexLocker lock(&mutex_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (footprints_.isEmpty()) {
        p.setPen(QColor("#808080"));
        p.drawText(rect(), Qt::AlignCenter, "Volume Footprint");
        return;
    }

    int visible = std::min(max_visible_, (int)footprints_.size());
    double bar_width = (double)width() / visible;
    QRectF bars_rect(0, 0, width(), height());

    // Draw last N footprints
    for (int i = footprints_.size() - visible; i < footprints_.size(); i++) {
        const auto& fp = footprints_[i];
        double x = (i - (footprints_.size() - visible)) * bar_width;

        // Background bar colored by delta
        QRectF bar_rect(x, 0, bar_width - 1, height());
        if (fp.net_delta > 0)
            p.fillRect(bar_rect, QColor(34, 197, 94, 30));  // green tint
        else
            p.fillRect(bar_rect, QColor(239, 68, 68, 30));  // red tint

        // Delta line in middle
        double delta_pct = std::abs(fp.net_delta) / std::max(fp.total_bid + fp.total_ask, 1.0);
        double line_h = height() * std::min(delta_pct, 1.0);
        double mid_y = height() / 2.0;

        p.setPen(QPen(fp.net_delta > 0 ? QColor("#22c55e") : QColor("#ef4444"), 2));
        if (fp.net_delta > 0)
            p.drawLine(QPointF(x + bar_width / 2, mid_y), QPointF(x + bar_width / 2, mid_y - line_h / 2));
        else
            p.drawLine(QPointF(x + bar_width / 2, mid_y), QPointF(x + bar_width / 2, mid_y + line_h / 2));

        // Bid/Ask dots
        double bid_h = (fp.total_bid / std::max(max_volume_, 1.0)) * height() * 0.4;
        double ask_h = (fp.total_ask / std::max(max_volume_, 1.0)) * height() * 0.4;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#3b82f6")); // blue = bid
        p.drawRect(QRectF(x + 1, height() - bid_h, bar_width / 2 - 2, bid_h - 1));
        p.setBrush(QColor("#f59e0b")); // amber = ask
        p.drawRect(QRectF(x + bar_width / 2, height() - ask_h, bar_width / 2 - 2, ask_h - 1));
    }
}

} // namespace fincept::screens::crypto
