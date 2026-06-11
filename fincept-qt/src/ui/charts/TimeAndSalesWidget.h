#pragma once

#include <QTableWidget>
#include <QVector>

namespace fincept::ui {

struct TradePrint {
    double price = 0;
    double qty = 0;
    qint64 time_ms = 0;
    bool is_buy = true;  // true=buy/sell false=sell/bid
};

class TimeAndSalesWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit TimeAndSalesWidget(int max_rows = 200, QWidget* parent = nullptr);

    void add_trade(const TradePrint& trade);
    void add_trades(const QVector<TradePrint>& trades);
    void clear_all();

private:
    int max_rows_;
    void insert_row(int idx, const TradePrint& trade);
};

} // namespace fincept::ui
