#pragma once
// Kagi Chart — price movement chart ignoring time.
// A vertical line thickens when price reverses by reversal_amount.
#include "trading/TradingTypes.h"
#include <QWidget>
#include <QVector>

namespace fincept::screens::crypto {

struct KagiLine {
    double price_start, price_end;
    double high, low;
    bool is_thick; // thick = uptrend, thin = downtrend
    qint64 timestamp;
};

class KagiChart : public QWidget {
    Q_OBJECT
public:
    explicit KagiChart(QWidget* parent = nullptr);
    void add_candle(const trading::Candle& candle);
    void set_candles(const QVector<trading::Candle>& candles);
    void set_reversal(double amount);
    void set_active(bool a);
    void clear();
protected:
    void paintEvent(QPaintEvent* event) override;
    QSize minimumSizeHint() const override { return QSize(200, 200); }
private:
    void compute_lines();
    void render(QPainter& p, const QRect& rect);
    double reversal_amount_ = 5.0;
    QVector<KagiLine> lines_;
    QVector<trading::Candle> candles_;
    bool active_ = false;
    int max_lines_ = 100;
    double min_px_ = 0, max_px_ = 0;
};

} // namespace
