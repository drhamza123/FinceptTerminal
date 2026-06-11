#include "ui/charts/MarketHeatmapWidget.h"

#include <QPainter>
#include <QToolTip>
#include <QMouseEvent>
#include <cmath>

namespace fincept::ui {

MarketHeatmapWidget::MarketHeatmapWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(200, 150);
}

void MarketHeatmapWidget::set_data(const QVector<HeatmapCell>& cells) {
    cells_ = cells;
    update();
}

void MarketHeatmapWidget::clear() { cells_.clear(); update(); }

QColor MarketHeatmapWidget::color_for_change(double pct) const {
    if (pct > 0) {
        int intensity = std::min(255, static_cast<int>(pct * 30));
        return QColor(0, std::max(0, 255 - intensity), std::max(0, 100 - intensity));
    } else {
        int intensity = std::min(255, static_cast<int>(std::abs(pct) * 30));
        return QColor(std::min(255, 150 + intensity), std::max(0, 100 - intensity), std::max(0, 50 - intensity));
    }
}

QRectF MarketHeatmapWidget::cell_rect(int idx) const {
    if (cells_.isEmpty()) return {};
    int cols = std::max(1, static_cast<int>(std::sqrt(cells_.size() * 2)));
    int rows = (cells_.size() + cols - 1) / cols;
    double w = width() / static_cast<double>(cols);
    double h = height() / static_cast<double>(rows);
    int col = idx % cols;
    int row = idx / cols;
    return QRectF(col * w, row * h, w, h);
}

void MarketHeatmapWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    for (int i = 0; i < cells_.size(); ++i) {
        QRectF r = cell_rect(i);
        p.fillRect(r, color_for_change(cells_[i].change_pct));

        if (i == hovered_idx_) {
            QPen highlight(Qt::white, 2);
            p.setPen(highlight);
            p.drawRect(r);
        }

        p.setPen(Qt::white);
        QFont f = p.font();
        f.setPixelSize(static_cast<int>(r.width() * 0.18));
        f.setBold(true);
        p.setFont(f);
        p.drawText(r.adjusted(4, 4, -4, -r.height() * 0.5), Qt::AlignLeft | Qt::AlignTop, cells_[i].symbol);

        f.setPixelSize(static_cast<int>(r.width() * 0.14));
        f.setBold(false);
        p.setFont(f);
        QString pct = QString("%1%").arg(cells_[i].change_pct, 0, 'f', 1);
        p.drawText(r.adjusted(4, r.height() * 0.4, -4, -4), Qt::AlignLeft | Qt::AlignBottom, pct);
    }
}

void MarketHeatmapWidget::mouseMoveEvent(QMouseEvent* e) {
    int old = hovered_idx_;
    hovered_idx_ = -1;
    for (int i = 0; i < cells_.size(); ++i) {
        if (cell_rect(i).contains(e->pos())) { hovered_idx_ = i; break; }
    }
    if (hovered_idx_ != old) update();
    if (hovered_idx_ >= 0) {
        const auto& c = cells_[hovered_idx_];
        QToolTip::showText(e->globalPosition().toPoint(),
            QString("%1 (%2)\nChange: %3%\nSector: %4")
                .arg(c.symbol, c.sector).arg(c.change_pct, 0, 'f', 2));
    }
}

} // namespace fincept::ui
