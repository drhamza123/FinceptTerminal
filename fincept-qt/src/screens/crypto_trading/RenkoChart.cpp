#include "screens/crypto_trading/RenkoChart.h"

#include <QPainter>
#include <algorithm>
#include <cmath>

namespace fincept::screens::crypto {

RenkoChart::RenkoChart(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(200);
}

void RenkoChart::clear() {
    bricks_.clear();
    candles_.clear();
    last_close_ = 0;
    update();
}

void RenkoChart::set_brick_size(double size) {
    if (size > 0 && size != brick_size_) {
        brick_size_ = size;
        bricks_.clear();
        last_close_ = 0;
        compute_bricks();
        update();
    }
}

void RenkoChart::set_active(bool a) {
    active_ = a;
    if (a) compute_bricks();
    update();
}

void RenkoChart::add_candle(const trading::Candle& candle) {
    candles_.append(candle);
    if (active_) {
        compute_bricks();
        update();
    }
}

void RenkoChart::set_candles(const QVector<trading::Candle>& candles) {
    candles_ = candles;
    if (active_) {
        bricks_.clear();
        last_close_ = 0;
        compute_bricks();
        update();
    }
}

void RenkoChart::compute_bricks() {
    if (candles_.isEmpty()) return;

    for (const auto& c : candles_) {
        if (last_close_ == 0) {
            last_close_ = c.close;
            continue;
        }

        double diff = c.close - last_close_;
        int num_bricks = std::abs(diff) / brick_size_;

        for (int i = 0; i < num_bricks && bricks_.size() < max_bricks_; i++) {
            RenkoBrick brick;
            bool going_up = diff > 0;
            brick.is_up = going_up;
            brick.open = last_close_;
            brick.close = last_close_ + (going_up ? brick_size_ : -brick_size_);
            brick.high = std::max(brick.open, brick.close);
            brick.low = std::min(brick.open, brick.close);
            brick.timestamp = c.timestamp;
            bricks_.append(brick);
            last_close_ = brick.close;
        }

        // Remove old bricks if over limit
        while (bricks_.size() > max_bricks_)
            bricks_.remove(0);
    }

    // Update price range
    if (!bricks_.isEmpty()) {
        min_price_ = bricks_.first().low;
        max_price_ = bricks_.first().high;
        for (const auto& b : bricks_) {
            min_price_ = std::min(min_price_, b.low);
            max_price_ = std::max(max_price_, b.high);
        }
    }
}

void RenkoChart::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (!active_ || bricks_.isEmpty()) {
        p.setPen(QColor("#808080"));
        p.drawText(rect(), Qt::AlignCenter, active_ ? "No bricks yet" : "Renko (click to enable)");
        return;
    }

    render_bricks(p, rect());
}

void RenkoChart::render_bricks(QPainter& p, const QRect& rect) {
    double price_range = max_price_ - min_price_;
    if (price_range <= 0) price_range = 1;

    double brick_w = (double)rect.width() / std::min((int)bricks_.size(), max_bricks_);
    if (brick_w < 4) brick_w = 4;

    int start = std::max(0, (int)(bricks_.size() - (rect.width() / (int)brick_w)));

    for (int i = start; i < bricks_.size(); i++) {
        const auto& b = bricks_[i];
        double x = (i - start) * brick_w;
        double y1 = rect.height() * (1 - (b.high - min_price_) / price_range);
        double y2 = rect.height() * (1 - (b.low - min_price_) / price_range);
        double bh = std::abs(y2 - y1);
        if (bh < 2) bh = 2;

        QRectF brick_rect(x + 1, y1, brick_w - 2, bh);

        if (b.is_up) {
            p.setPen(QPen(QColor("#22c55e"), 1));
            p.setBrush(QColor(34, 197, 94, 60));
        } else {
            p.setPen(QPen(QColor("#ef4444"), 1));
            p.setBrush(QColor(239, 68, 68, 60));
        }
        p.drawRect(brick_rect);
    }
}

} // namespace fincept::screens::crypto
