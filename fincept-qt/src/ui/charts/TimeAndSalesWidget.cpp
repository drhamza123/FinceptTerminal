#include "ui/charts/TimeAndSalesWidget.h"

#include <QDateTime>
#include <QHeaderView>

namespace fincept::ui {

TimeAndSalesWidget::TimeAndSalesWidget(int max_rows, QWidget* parent)
    : QTableWidget(parent), max_rows_(max_rows) {
    setColumnCount(4);
    setHorizontalHeaderLabels({"Time", "Price", "Qty", "Side"});
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    verticalHeader()->hide();
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::NoSelection);
    setAlternatingRowColors(true);
    setFixedHeight(200);
}

void TimeAndSalesWidget::add_trade(const TradePrint& trade) {
    insert_row(0, trade);
    while (rowCount() > max_rows_) removeRow(rowCount() - 1);
}

void TimeAndSalesWidget::add_trades(const QVector<TradePrint>& trades) {
    for (const auto& t : trades) insert_row(0, t);
    while (rowCount() > max_rows_) removeRow(rowCount() - 1);
}

void TimeAndSalesWidget::insert_row(int idx, const TradePrint& trade) {
    insertRow(idx);
    QString ts = QDateTime::fromMSecsSinceEpoch(trade.time_ms).toString("HH:mm:ss.zzz");
    setItem(idx, 0, new QTableWidgetItem(ts));
    auto* price_item = new QTableWidgetItem(QString::number(trade.price, 'f', 2));
    price_item->setForeground(trade.is_buy ? QColor("#089981") : QColor("#f23645"));
    setItem(idx, 1, price_item);
    setItem(idx, 2, new QTableWidgetItem(QString::number(trade.qty, 'f', 4)));
    auto* side_item = new QTableWidgetItem(trade.is_buy ? "BUY" : "SELL");
    side_item->setForeground(trade.is_buy ? QColor("#089981") : QColor("#f23645"));
    setItem(idx, 3, side_item);
}

void TimeAndSalesWidget::clear_all() {
    setRowCount(0);
}

} // namespace fincept::ui
