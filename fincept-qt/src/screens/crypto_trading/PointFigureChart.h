#pragma once
// Point & Figure Chart — plots X (rising) and O (falling) columns based on price.
#include "trading/TradingTypes.h"
#include <QWidget>
#include <QVector>

namespace fincept::screens::crypto {

struct PFColumn {
    double price_start, price_end;
    bool is_x; // X = up, O = down
    int count; // number of X or O
};

class PointFigureChart : public QWidget {
    Q_OBJECT
public:
    explicit PointFigureChart(QWidget* parent = nullptr);
    void add_candle(const trading::Candle& candle);
    void set_candles(const QVector<trading::Candle>& candles);
    void set_box_size(double s);
    void set_reversal(int r = 3);
    void set_active(bool a);
    void clear();
protected:
    void paintEvent(QPaintEvent* event) override;
    QSize minimumSizeHint() const override { return QSize(200, 200); }
private:
    void compute();
    void render(QPainter& p, const QRect& rect);
    double box_size_ = 1.0;
    int reversal_ = 3;
    QVector<PFColumn> columns_;
    QVector<trading::Candle> candles_;
    bool active_ = false;
    int max_cols_ = 50;
    double min_px_ = 0, max_px_ = 0;
};

} // namespace
