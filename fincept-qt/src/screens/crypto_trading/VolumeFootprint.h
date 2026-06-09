#pragma once
// Volume Footprint — displays intrabar buy/sell delta as histogram.
// Renders bid/ask volume at each price level per candle.
#include "trading/TradingTypes.h"
#include <QWidget>
#include <QVector>
#include <QHash>
#include <QMutex>

namespace fincept::screens::crypto {

struct VolumeLevel {
    double price;
    double bid_volume;
    double ask_volume;
    double delta; // ask - bid (negative = selling pressure)
    double total_volume;
};

struct FootprintCandle {
    qint64 timestamp;
    double open, high, low, close;
    double total_bid = 0, total_ask = 0;
    double net_delta = 0;
    QVector<VolumeLevel> levels;
};

class VolumeFootprint : public QWidget {
    Q_OBJECT
public:
    explicit VolumeFootprint(QWidget* parent = nullptr);
    void add_candle(const trading::Candle& candle);
    void set_candles(const QVector<trading::Candle>& candles);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize minimumSizeHint() const override { return QSize(100, 80); }

private:
    void compute_footprint(const trading::Candle& candle, FootprintCandle& fp);
    void render_footprint(QPainter& p, const FootprintCandle& fp, const QRect& rect);
    void render_histogram(QPainter& p, const FootprintCandle& fp, const QRect& rect);

    QVector<FootprintCandle> footprints_;
    QMutex mutex_;
    int max_visible_ = 50;
    double max_volume_ = 0;
    double min_price_ = 0, max_price_ = 0;
};

} // namespace fincept::screens::crypto
