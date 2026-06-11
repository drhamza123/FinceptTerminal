#pragma once

#include <QWidget>
#include <QVector>
#include <QString>

namespace fincept::ui {

struct HeatmapCell {
    QString symbol;
    QString sector;
    double change_pct = 0;
    double market_cap = 0;
    QColor color;
};

class MarketHeatmapWidget : public QWidget {
    Q_OBJECT
public:
    explicit MarketHeatmapWidget(QWidget* parent = nullptr);

    void set_data(const QVector<HeatmapCell>& cells);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QVector<HeatmapCell> cells_;
    int hovered_idx_ = -1;
    QRectF cell_rect(int idx) const;
    QColor color_for_change(double pct) const;
};

} // namespace fincept::ui
