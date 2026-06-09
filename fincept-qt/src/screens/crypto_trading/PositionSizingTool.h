#pragma once
// Position Sizing Calculator — on-chart tool that calculates
// position size, risk, and reward based on account equity, risk %, entry, SL, TP.
#include <QWidget>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>

namespace fincept::screens::crypto {

class PositionSizingTool : public QWidget {
    Q_OBJECT
public:
    explicit PositionSizingTool(QWidget* parent = nullptr);

    void set_current_price(double price);
    void set_account_balance(double balance);

signals:
    void order_submitted(const QString& side, double quantity, double sl_price, double tp_price);

private slots:
    void on_calculate();
    void on_buy();
    void on_sell();

private:
    void build_ui();
    void update_results();

    QDoubleSpinBox* account_bal_spin_ = nullptr;
    QDoubleSpinBox* risk_pct_spin_ = nullptr;
    QDoubleSpinBox* entry_price_spin_ = nullptr;
    QDoubleSpinBox* sl_price_spin_ = nullptr;
    QDoubleSpinBox* tp_price_spin_ = nullptr;
    QDoubleSpinBox* rr_ratio_spin_ = nullptr;
    QDoubleSpinBox* position_qty_spin_ = nullptr;

    QLabel* risk_amount_lbl_ = nullptr;
    QLabel* position_value_lbl_ = nullptr;
    QLabel* reward_lbl_ = nullptr;
    QLabel* rr_lbl_ = nullptr;

    double current_price_ = 0;
    double account_balance_ = 10000;
};

} // namespace fincept::screens::crypto
