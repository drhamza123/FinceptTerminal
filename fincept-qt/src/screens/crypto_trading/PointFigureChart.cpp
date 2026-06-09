#include "screens/crypto_trading/PointFigureChart.h"
#include "trading/TradingTypes.h"
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace fincept::screens::crypto {

PointFigureChart::PointFigureChart(QWidget* p) : QWidget(p) { setMinimumHeight(200); }
void PointFigureChart::clear() { columns_.clear(); candles_.clear(); update(); }
void PointFigureChart::set_box_size(double s) { if (s > 0) box_size_ = s; }
void PointFigureChart::set_reversal(int r) { if (r > 0) reversal_ = r; }
void PointFigureChart::set_active(bool a) { active_ = a; if (a) compute(); update(); }

void PointFigureChart::add_candle(const trading::Candle& c) {
    candles_.append(c); if (active_) { compute(); update(); }
}

void PointFigureChart::set_candles(const QVector<trading::Candle>& c) {
    candles_ = c; if (active_) { columns_.clear(); compute(); update(); }
}

void PointFigureChart::compute() {
    if (candles_.isEmpty() || box_size_ <= 0) return;
    columns_.clear();
    double current = candles_.first().close;
    bool is_up = true;

    for (const auto& c : candles_) {
        double price = c.close;
        double diff = price - current;

        if (columns_.isEmpty()) {
            is_up = diff >= 0;
            int n = std::abs(diff) / box_size_;
            if (n < 1) n = 1;
            PFColumn col; col.price_start = current; col.price_end = current + (is_up ? n * box_size_ : -n * box_size_);
            col.is_x = is_up; col.count = n;
            columns_.append(col);
            current = col.price_end;
            continue;
        }

        auto& last = columns_.last();
        double move = is_up ? (price - current) : (current - price);

        if (move >= box_size_) {
            int n = move / box_size_;
            if (is_up) { last.price_end += n * box_size_; last.count += n; }
            else { last.price_end -= n * box_size_; last.count += n; }
            current = last.price_end;
        } else if (move <= -box_size_ * reversal_) {
            is_up = !is_up;
            int n = (-move) / box_size_;
            PFColumn col; col.price_start = current;
            col.price_end = current + (is_up ? n * box_size_ : -n * box_size_);
            col.is_x = is_up; col.count = n;
            columns_.append(col);
            current = col.price_end;
        }
        if (columns_.size() > max_cols_) columns_.remove(0);
    }
    if (columns_.isEmpty()) return;
    min_px_ = columns_.first().price_start; max_px_ = columns_.first().price_start;
    for (const auto& col : columns_) {
        min_px_ = std::min(min_px_, std::min(col.price_start, col.price_end));
        max_px_ = std::max(max_px_, std::max(col.price_start, col.price_end));
    }
}

void PointFigureChart::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    if (!active_ || columns_.isEmpty()) {
        p.setPen(QColor("#808080")); p.drawText(rect(), Qt::AlignCenter, active_ ? "No P&F data" : "P&F (click to enable)");
        return;
    }
    render(p, rect());
}

void PointFigureChart::render(QPainter& p, const QRect& rect) {
    double pr = max_px_ - min_px_; if (pr <= 0) pr = 1;
    double col_w = (double)rect.width() / std::max((int)columns_.size(), 1);
    if (col_w < 8) col_w = 8;
    int start = std::max(0, (int)(columns_.size() - rect.width() / (int)col_w));
    QFont f("Consolas", 7); p.setFont(f);

    for (int i = start; i < columns_.size(); i++) {
        const auto& col = columns_[i];
        double x = (i - start) * col_w;
        double cell_h = box_size_ / pr * rect.height();
        if (cell_h < 4) cell_h = 4;

        double y_base = rect.height() * (1 - (col.price_start - min_px_) / pr);
        for (int j = 0; j < col.count; j++) {
            double y = y_base - j * cell_h * (col.is_x ? 1 : -1);
            QRectF r(x + 1, y - cell_h / 2, col_w - 2, cell_h);
            p.setPen(QPen(col.is_x ? QColor("#22c55e") : QColor("#ef4444"), 1));
            p.drawText(r, Qt::AlignCenter, col.is_x ? "X" : "O");
        }
    }
}

} // namespace
