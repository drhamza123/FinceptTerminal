#include "screens/crypto_trading/PositionSizingTool.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <cmath>

namespace fincept::screens::crypto {

PositionSizingTool::PositionSizingTool(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PositionSizingTool::set_current_price(double price) {
    current_price_ = price;
    if (entry_price_spin_) entry_price_spin_->setValue(price);
}

void PositionSizingTool::set_account_balance(double balance) {
    account_balance_ = balance;
    if (account_bal_spin_) account_bal_spin_->setValue(balance);
}

void PositionSizingTool::build_ui() {
    setStyleSheet("QDoubleSpinBox{background:#0a0a0a;color:#e5e5e5;border:1px solid #1a1a2e;padding:4px;font-size:11px;}QLabel{color:#cbd5e1;font-size:10px;}");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto* title = new QLabel("POSITION SIZING");
    title->setStyleSheet("color:#22c55e;font-size:11px;font-weight:800;letter-spacing:1px;");
    root->addWidget(title);

    auto* form = new QFormLayout;
    form->setSpacing(4);
    form->setContentsMargins(0, 0, 0, 0);

    account_bal_spin_ = new QDoubleSpinBox;
    account_bal_spin_->setRange(100, 1e9);
    account_bal_spin_->setValue(10000);
    account_bal_spin_->setPrefix("$ ");
    account_bal_spin_->setDecimals(0);
    form->addRow("Account:", account_bal_spin_);

    risk_pct_spin_ = new QDoubleSpinBox;
    risk_pct_spin_->setRange(0.1, 100);
    risk_pct_spin_->setValue(2.0);
    risk_pct_spin_->setSuffix(" %");
    risk_pct_spin_->setDecimals(1);
    form->addRow("Risk:", risk_pct_spin_);

    entry_price_spin_ = new QDoubleSpinBox;
    entry_price_spin_->setRange(0.01, 1e9);
    entry_price_spin_->setDecimals(2);
    form->addRow("Entry:", entry_price_spin_);

    sl_price_spin_ = new QDoubleSpinBox;
    sl_price_spin_->setRange(0.01, 1e9);
    sl_price_spin_->setDecimals(2);
    form->addRow("Stop Loss:", sl_price_spin_);

    tp_price_spin_ = new QDoubleSpinBox;
    tp_price_spin_->setRange(0.01, 1e9);
    tp_price_spin_->setDecimals(2);
    form->addRow("Take Profit:", tp_price_spin_);

    root->addLayout(form);

    // Results
    auto* results = new QFrame;
    results->setStyleSheet("QFrame{background:#0a0a0a;border:1px solid #1a1a2e;border-radius:3px;}");
    auto* rl = new QVBoxLayout(results);
    rl->setSpacing(2);
    rl->setContentsMargins(8, 6, 8, 6);

    auto make_result = [](const QString& label, QLabel*& val, const QString& color) {
        auto* row = new QWidget;
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(new QLabel(label));
        val = new QLabel("--");
        val->setStyleSheet(QString("color:%1;font-weight:700;font-size:12px;").arg(color));
        hl->addWidget(val);
        hl->addStretch();
        return row;
    };

    rl->addWidget(make_result("Risk $:", risk_amount_lbl_, "#ef4444"));
    rl->addWidget(make_result("Position:", position_value_lbl_, "#e5e5e5"));
    rl->addWidget(make_result("Reward:", reward_lbl_, "#22c55e"));
    rl->addWidget(make_result("R:R:", rr_lbl_, "#f59e0b"));

    position_qty_spin_ = new QDoubleSpinBox;
    position_qty_spin_->setRange(0, 1e9);
    position_qty_spin_->setDecimals(4);
    position_qty_spin_->setStyleSheet("QDoubleSpinBox{background:#111;color:#e5e5e5;border:1px solid #22c55e;padding:6px;font-size:14px;font-weight:700;}");
    auto* qty_row = new QWidget;
    auto* qty_l = new QHBoxLayout(qty_row);
    qty_l->addWidget(new QLabel("Quantity:"));
    qty_l->addWidget(position_qty_spin_, 1);
    rl->addWidget(qty_row);

    root->addWidget(results);

    // Buttons
    auto* btn_row = new QHBoxLayout;
    auto* calc_btn = new QPushButton("CALCULATE");
    calc_btn->setStyleSheet("QPushButton{background:#f59e0b;color:#080808;border:none;padding:6px;font-weight:700;font-size:10px;border-radius:3px;}QPushButton:hover{background:#d97706;}");
    connect(calc_btn, &QPushButton::clicked, this, &PositionSizingTool::on_calculate);
    btn_row->addWidget(calc_btn);

    auto* buy_btn = new QPushButton("BUY");
    buy_btn->setStyleSheet("QPushButton{background:#22c55e;color:#080808;border:none;padding:6px;font-weight:700;font-size:10px;border-radius:3px;}QPushButton:hover{background:#16a34a;}");
    connect(buy_btn, &QPushButton::clicked, this, &PositionSizingTool::on_buy);
    btn_row->addWidget(buy_btn);

    auto* sell_btn = new QPushButton("SELL");
    sell_btn->setStyleSheet("QPushButton{background:#ef4444;color:#fff;border:none;padding:6px;font-weight:700;font-size:10px;border-radius:3px;}QPushButton:hover{background:#dc2626;}");
    connect(sell_btn, &QPushButton::clicked, this, &PositionSizingTool::on_sell);
    btn_row->addWidget(sell_btn);

    root->addLayout(btn_row);

    // Connect signals for auto-calculate
    connect(account_bal_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PositionSizingTool::on_calculate);
    connect(risk_pct_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PositionSizingTool::on_calculate);
    connect(entry_price_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PositionSizingTool::on_calculate);
    connect(sl_price_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PositionSizingTool::on_calculate);
    connect(tp_price_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PositionSizingTool::on_calculate);
}

void PositionSizingTool::on_calculate() {
    update_results();
}

void PositionSizingTool::update_results() {
    double bal = account_bal_spin_->value();
    double risk_pct = risk_pct_spin_->value() / 100.0;
    double entry = entry_price_spin_->value();
    double sl = sl_price_spin_->value();
    double tp = tp_price_spin_->value();

    if (entry <= 0 || sl <= 0 || entry == sl) return;

    double risk_amount = bal * risk_pct;
    double risk_per_unit = std::abs(entry - sl);
    double quantity = risk_amount / risk_per_unit;

    position_qty_spin_->setValue(quantity);

    double position_value = quantity * entry;
    double reward = tp > 0 ? std::abs(tp - entry) * quantity : 0;
    double rr = (tp > 0 && sl > 0) ? std::abs(tp - entry) / std::abs(entry - sl) : 0;

    if (risk_amount_lbl_) risk_amount_lbl_->setText(QString("$%1").arg(risk_amount, 0, 'f', 2));
    if (position_value_lbl_) position_value_lbl_->setText(QString("$%1").arg(position_value, 0, 'f', 2));
    if (reward_lbl_) reward_lbl_->setText(QString("$%1").arg(reward, 0, 'f', 2));
    if (rr_lbl_) rr_lbl_->setText(QString("1:%1").arg(rr, 0, 'f', 2));
}

void PositionSizingTool::on_buy() {
    emit order_submitted("BUY", position_qty_spin_->value(), sl_price_spin_->value(), tp_price_spin_->value());
}

void PositionSizingTool::on_sell() {
    emit order_submitted("SELL", position_qty_spin_->value(), sl_price_spin_->value(), tp_price_spin_->value());
}

} // namespace fincept::screens::crypto
