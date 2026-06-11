#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QTimer>
#include <QVector>

class QPushButton;

namespace fincept::ui {

class StockScreenerWidget : public QWidget {
    Q_OBJECT
public:
    explicit StockScreenerWidget(QWidget* parent = nullptr);
    void load_data(const QString& market = "US");

signals:
    void symbol_selected(const QString& symbol);

private:
    void apply_filter();
    void fetch_data();
    void populate_row(int row, const QString& symbol, double price, double change,
                      double volume, double rsi, double mcap);

    QLineEdit* filter_input_;
    QComboBox* market_combo_;
    QTableWidget* table_;
    QLabel* count_label_;
    QPushButton* refresh_btn_;
    QTimer* fetch_timer_;

    struct StockRow {
        QString symbol; double price = 0, change = 0, volume = 0, rsi = 50, mcap = 0;
    };
    QVector<StockRow> data_;
};

} // namespace fincept::ui
