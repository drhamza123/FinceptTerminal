#pragma once
// Renko Chart — non-time-based brick chart.
// Each brick is a fixed price movement, ignoring time and volume.
#include "trading/TradingTypes.h"
#include <QWidget>
#include <QVector>

namespace fincept::screens::crypto {

struct RenkoBrick {
    double open, close, high, low;
    bool is_up; // green if close > open
    qint64 timestamp; // timestamp of the last candle that formed this brick
};

class RenkoChart : public QWidget {
    Q_OBJECT
public:
    explicit RenkoChart(QWidget* parent = nullptr);
    void add_candle(const trading::Candle& candle);
    void set_candles(const QVector<trading::Candle>& candles);
    void set_brick_size(double size);
    void clear();
    bool is_active() const { return active_; }
    void set_active(bool a);

signals:
    void brick_added(const RenkoBrick& brick);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize minimumSizeHint() const override { return QSize(200, 200); }

private:
    void compute_bricks();
    void render_bricks(QPainter& p, const QRect& rect);

    double brick_size_ = 5.0; // price units per brick
    QVector<RenkoBrick> bricks_;
    QVector<trading::Candle> candles_;
    bool active_ = false;
    int max_bricks_ = 100;
    double last_close_ = 0;
    double min_price_ = 0, max_price_ = 0;
};

} // namespace fincept::screens::crypto
