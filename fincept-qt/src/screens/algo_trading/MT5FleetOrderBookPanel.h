// MT5FleetOrderBookPanel.h — Depth of Market / Order Book
#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>

namespace fincept::screens {

class MT5FleetOrderBookPanel : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetOrderBookPanel(QWidget* parent = nullptr);
    ~MT5FleetOrderBookPanel() override;
    void set_symbol(const QString& symbol);

  private slots:
    void refresh_order_book();
    void on_buy_clicked();
    void on_sell_clicked();

  private:
    void build_ui();
    void apply_theme();
    void update_order_book(const QJsonArray& bids, const QJsonArray& asks);

    // UI
    QLabel* symbol_label_ = nullptr;
    QLabel* spread_label_ = nullptr;
    QTableWidget* bids_table_ = nullptr;
    QTableWidget* asks_table_ = nullptr;
    QPushButton* buy_btn_ = nullptr;
    QPushButton* sell_btn_ = nullptr;
    QLabel* last_price_label_ = nullptr;

    // State
    QString current_symbol_ = "XAUUSD";
};

} // namespace fincept::screens
