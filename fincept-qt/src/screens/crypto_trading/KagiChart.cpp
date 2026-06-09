#include "screens/crypto_trading/KagiChart.h"
#include "trading/TradingTypes.h"
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace fincept::screens::crypto {

KagiChart::KagiChart(QWidget* parent) : QWidget(parent) { setMinimumHeight(200); }
void KagiChart::clear() { lines_.clear(); candles_.clear(); update(); }
void KagiChart::set_reversal(double a) { if (a > 0) reversal_amount_ = a; }
void KagiChart::set_active(bool a) { active_ = a; if (a) compute_lines(); update(); }

void KagiChart::add_candle(const trading::Candle& c) {
    candles_.append(c);
    if (active_) { compute_lines(); update(); }
}

void KagiChart::set_candles(const QVector<trading::Candle>& c) {
    candles_ = c;
    if (active_) { lines_.clear(); compute_lines(); update(); }
}

void KagiChart::compute_lines() {
    if (candles_.isEmpty() || reversal_amount_ <= 0) return;
    lines_.clear();
    double current = candles_.first().close;
    bool trend_up = true; // starts assumed up
    double last_extreme = current;

    for (const auto& c : candles_) {
        double price = c.close;
        double diff = price - current;

        if (trend_up) {
            if (diff >= 0) {
                // Continue up
                KagiLine kl; kl.price_start = current; kl.price_end = price;
                kl.high = std::max(current, price); kl.low = std::min(current, price);
                kl.is_thick = true; kl.timestamp = c.timestamp;
                lines_.append(kl);
                current = price;
                last_extreme = std::max(last_extreme, price);
            } else if (std::abs(diff) >= reversal_amount_) {
                // Reverse down
                trend_up = false;
                KagiLine kl; kl.price_start = current; kl.price_end = price;
                kl.high = std::max(current, price); kl.low = std::min(current, price);
                kl.is_thick = false; kl.timestamp = c.timestamp;
                lines_.append(kl);
                current = price;
                last_extreme = price;
            }
        } else {
            if (diff <= 0) {
                KagiLine kl; kl.price_start = current; kl.price_end = price;
                kl.high = std::max(current, price); kl.low = std::min(current, price);
                kl.is_thick = false; kl.timestamp = c.timestamp;
                lines_.append(kl);
                current = price;
                last_extreme = std::min(last_extreme, price);
            } else if (diff >= reversal_amount_) {
                trend_up = true;
                KagiLine kl; kl.price_start = current; kl.price_end = price;
                kl.high = std::max(current, price); kl.low = std::min(current, price);
                kl.is_thick = true; kl.timestamp = c.timestamp;
                lines_.append(kl);
                current = price;
                last_extreme = price;
            }
        }
        if (lines_.size() > max_lines_) lines_.remove(0);
    }
    if (lines_.isEmpty()) return;
    min_px_ = lines_.first().low; max_px_ = lines_.first().high;
    for (const auto& l : lines_) {
        min_px_ = std::min(min_px_, l.low);
        max_px_ = std::max(max_px_, l.high);
    }
}

void KagiChart::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    if (!active_ || lines_.isEmpty()) {
        p.setPen(QColor("#808080"));
        p.drawText(rect(), Qt::AlignCenter, active_ ? "No kagi data" : "Kagi (click to enable)");
        return;
    }
    render(p, rect());
}

void KagiChart::render(QPainter& p, const QRect& rect) {
    double pr = max_px_ - min_px_; if (pr <= 0) pr = 1;
    double line_w = (double)rect.width() / std::max((int)lines_.size(), 1);
    if (line_w < 3) { line_w = 3; }
    int start = std::max(0, (int)(lines_.size() - rect.width() / (int)line_w));

    for (int i = start; i < lines_.size(); i++) {
        const auto& l = lines_[i];
        double x = (i - start) * line_w + line_w / 2;
        double y1 = rect.height() * (1 - (l.price_start - min_px_) / pr);
        double y2 = rect.height() * (1 - (l.price_end - min_px_) / pr);

        QPen pen(l.is_thick ? QColor("#22c55e") : QColor("#ef4444"),
                 l.is_thick ? 3.0 : 1.5);
        p.setPen(pen);
        p.drawLine(QPointF(x, y1), QPointF(x, y2));
    }
}

} // namespace
