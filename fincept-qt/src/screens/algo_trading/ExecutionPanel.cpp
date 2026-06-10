#include "screens/algo_trading/ExecutionPanel.h"
#include "screens/algo_trading/MT5FleetChartPanel.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QHeaderView>
#include <QGroupBox>

namespace fincept::screens {

ExecutionPanel::ExecutionPanel(MT5FleetChartPanel* chart, QWidget* parent)
    : QWidget(parent), chart_(chart) {
    build_ui();
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &ExecutionPanel::refresh);
    timer_->start(3000);
    QTimer::singleShot(500, this, &ExecutionPanel::refresh);
}

ExecutionPanel::~ExecutionPanel() { timer_->stop(); }

void ExecutionPanel::build_ui() {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(6,6,6,6); root->setSpacing(6);
    setMinimumWidth(260);

    // ── Account Info ──
    auto* acct = new QGroupBox("Account", this);
    auto* ag = new QHBoxLayout(acct);
    balance_lbl_ = new QLabel("$0", this); balance_lbl_->setStyleSheet("color:#22c55e;font-size:13px;font-weight:700;");
    equity_lbl_ = new QLabel("$0", this); equity_lbl_->setStyleSheet("color:#e5e5e5;font-size:13px;font-weight:700;");
    bp_lbl_ = new QLabel("$0", this); bp_lbl_->setStyleSheet("color:#808080;font-size:11px;");
    ag->addWidget(new QLabel("Bal:", this)); ag->addWidget(balance_lbl_);
    ag->addWidget(new QLabel("Eq:", this)); ag->addWidget(equity_lbl_);
    ag->addWidget(new QLabel("BP:", this)); ag->addWidget(bp_lbl_);
    root->addWidget(acct);

    // ── Order Entry ──
    auto* oe = new QGroupBox("Place Order", this);
    auto* of = new QVBoxLayout(oe); of->setSpacing(3);

    auto* r1 = new QHBoxLayout();
    symbol_combo_ = new QComboBox(this); symbol_combo_->addItems({"XAUUSD","XAGUSD","EURUSD","BTCUSD","AAPL","MSFT","SPY","TSLA"});
    side_combo_ = new QComboBox(this); side_combo_->addItems({"BUY","SELL"});
    qty_spin_ = new QDoubleSpinBox(this); qty_spin_->setRange(0.001, 1000); qty_spin_->setValue(0.1); qty_spin_->setDecimals(3);
    r1->addWidget(new QLabel("Sym:",this)); r1->addWidget(symbol_combo_,1);
    r1->addWidget(side_combo_); r1->addWidget(qty_spin_);
    of->addLayout(r1);

    auto* r2 = new QHBoxLayout();
    type_combo_ = new QComboBox(this); type_combo_->addItems({"MARKET","LIMIT","STOP","TRAIL"});
    price_spin_ = new QDoubleSpinBox(this); price_spin_->setRange(0, 1e9); price_spin_->setDecimals(2);
    sl_spin_ = new QDoubleSpinBox(this); sl_spin_->setRange(0, 1e9); sl_spin_->setDecimals(2); sl_spin_->setSuffix(" SL");
    tp_spin_ = new QDoubleSpinBox(this); tp_spin_->setRange(0, 1e9); tp_spin_->setDecimals(2); tp_spin_->setSuffix(" TP");
    trail_spin_ = new QDoubleSpinBox(this); trail_spin_->setRange(0, 1e9); trail_spin_->setDecimals(2); trail_spin_->setSuffix(" Trail");
    r2->addWidget(type_combo_); r2->addWidget(price_spin_); r2->addWidget(sl_spin_);
    of->addLayout(r2);
    auto* r3 = new QHBoxLayout();
    r3->addWidget(tp_spin_); r3->addWidget(trail_spin_);
    of->addLayout(r3);

    auto* btn_row = new QHBoxLayout();
    buy_btn_ = new QPushButton("BUY", this);
    buy_btn_->setStyleSheet("background:#22c55e;color:#080808;font-weight:700;padding:8px;border-radius:4px;");
    connect(buy_btn_, &QPushButton::clicked, this, [this]() { side_combo_->setCurrentText("BUY"); place_order(); });
    btn_row->addWidget(buy_btn_);
    sell_btn_ = new QPushButton("SELL", this);
    sell_btn_->setStyleSheet("background:#ef4444;color:#fff;font-weight:700;padding:8px;border-radius:4px;");
    connect(sell_btn_, &QPushButton::clicked, this, [this]() { side_combo_->setCurrentText("SELL"); place_order(); });
    btn_row->addWidget(sell_btn_);
    refresh_btn_ = new QPushButton("Refresh", this); refresh_btn_->setObjectName("chartToolBtn");
    connect(refresh_btn_, &QPushButton::clicked, this, &ExecutionPanel::refresh);
    btn_row->addWidget(refresh_btn_);
    of->addLayout(btn_row);
    root->addWidget(oe);

    // ── Positions ──
    pos_table_ = new QTableWidget(0, 6, this);
    pos_table_->setHorizontalHeaderLabels({"Symbol","Side","Qty","Entry","P&L","Close"});
    pos_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pos_table_->horizontalHeader()->setStretchLastSection(true);
    pos_table_->verticalHeader()->setVisible(false);
    pos_table_->setMaximumHeight(200);
    root->addWidget(new QLabel("Positions:", this));
    root->addWidget(pos_table_, 1);

    // ── Orders ──
    ord_table_ = new QTableWidget(0, 5, this);
    ord_table_->setHorizontalHeaderLabels({"ID","Symbol","Side","Qty","Cancel"});
    ord_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ord_table_->horizontalHeader()->setStretchLastSection(true);
    ord_table_->verticalHeader()->setVisible(false);
    ord_table_->setMaximumHeight(150);
    root->addWidget(new QLabel("Open Orders:", this));
    root->addWidget(ord_table_);

    scripts_table_ = new QTableWidget(0, 6, this);
    scripts_table_->setHorizontalHeaderLabels({"Name","Lang","Target","Status","Run","Stop"});
    scripts_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    scripts_table_->horizontalHeader()->setStretchLastSection(true);
    scripts_table_->verticalHeader()->setVisible(false);
    scripts_table_->setMaximumHeight(180);
    root->addWidget(new QLabel("Deployed Scripts:", this));
    root->addWidget(scripts_table_);
}

void ExecutionPanel::place_order() {
    QString sym = symbol_combo_->currentText();
    QString side = side_combo_->currentText();
    double qty = qty_spin_->value();
    QString type = type_combo_->currentText();
    double price = price_spin_->value();
    double sl = sl_spin_->value();
    double tp = tp_spin_->value();

    QJsonObject body;
    body["symbol"] = sym;
    body["side"] = side;
    body["volume"] = qty;
    if (type == "LIMIT" || type == "STOP") body["price"] = price;
    body["sl"] = sl;
    body["tp"] = tp;

    QString endpoint = type == "LIMIT" ? "limit" : type == "STOP" ? "stop" : "market";
    if (type == "TRAIL") endpoint = "trailing-stop";

    HttpClient::instance().post(
        QString("/mt5/order/%1").arg(endpoint), body,
        [this, side](Result<QJsonDocument> r) {
            if (r.is_ok()) {
                auto obj = r.value().object();
                if (obj["success"].toBool()) {
                    if (chart_)
                        chart_->set_price(obj["data"].toObject()["price"].toDouble());
                    refresh();
                }
            }
        }, this);
}

void ExecutionPanel::close_position(const QString& symbol) {
    QJsonObject body; body["symbol"] = symbol; body["side"] = "SELL"; body["volume"] = 0;
    HttpClient::instance().post("/mt5/worker/order", body,
        [this](Result<QJsonDocument>) { refresh(); }, this);
}

void ExecutionPanel::cancel_order(const QString& orderId) {
    QJsonObject body; body["order_id"] = orderId;
    HttpClient::instance().post("/mt5/order/cancel", body,
        [this](Result<QJsonDocument>) { refresh(); }, this);
}

void ExecutionPanel::run_script(const QString& deploymentId) {
    HttpClient::instance().post(QString("/execution/scripts/%1/run").arg(deploymentId), {},
        [this](Result<QJsonDocument>) { fetch_scripts(); }, this);
}

void ExecutionPanel::stop_script(const QString& deploymentId) {
    HttpClient::instance().post(QString("/execution/scripts/%1/stop").arg(deploymentId), {},
        [this](Result<QJsonDocument>) { fetch_scripts(); }, this);
}

void ExecutionPanel::refresh() {
    fetch_account();
    fetch_positions();
    fetch_orders();
    fetch_scripts();
}

void ExecutionPanel::fetch_account() {
    HttpClient::instance().get("/mt5/account",
        [this](Result<QJsonDocument> r) {
            if (!r.is_ok()) return;
            auto d = r.value().object()["data"].toObject();
            balance_lbl_->setText(QString("$%1").arg(d["balance"].toDouble(), 0, 'f', 2));
            equity_lbl_->setText(QString("$%1").arg(d["equity"].toDouble(), 0, 'f', 2));
            bp_lbl_->setText(QString("$%1").arg(d["margin_free"].toDouble(), 0, 'f', 0));
        }, this);
}

void ExecutionPanel::fetch_positions() {
    HttpClient::instance().get("/mt5/positions",
        [this](Result<QJsonDocument> r) {
            if (!r.is_ok()) return;
            auto arr = r.value().object()["data"].toArray();
            pos_table_->setRowCount(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                auto p = arr[i].toObject();
                QString sym = p["symbol"].toString();
                pos_table_->setItem(i, 0, new QTableWidgetItem(sym));
                pos_table_->setItem(i, 1, new QTableWidgetItem(p["side"].toString()));
                pos_table_->setItem(i, 2, new QTableWidgetItem(QString::number(p["volume"].toDouble(), 'f', 2)));
                pos_table_->setItem(i, 3, new QTableWidgetItem(QString::number(p["entry"].toDouble(), 'f', 2)));
                auto* pnl = new QTableWidgetItem(QString::number(p["profit"].toDouble(), 'f', 2));
                pnl->setForeground(p["profit"].toDouble() >= 0 ? QColor("#22c55e") : QColor("#ef4444"));
                pos_table_->setItem(i, 4, pnl);
                auto* close_btn = new QPushButton("Close", this);
                close_btn->setStyleSheet("background:#ef4444;color:#fff;padding:2px 8px;font-size:10px;");
                connect(close_btn, &QPushButton::clicked, this, [this, sym]() { close_position(sym); });
                pos_table_->setCellWidget(i, 5, close_btn);
            }
            update_position_markers();
        }, this);
}

void ExecutionPanel::fetch_orders() {
    HttpClient::instance().get("/mt5/orders",
        [this](Result<QJsonDocument> r) {
            if (!r.is_ok()) return;
            auto arr = r.value().object()["data"].toArray();
            ord_table_->setRowCount(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                auto o = arr[i].toObject();
                QString oid = o["order_id"].toString();
                ord_table_->setItem(i, 0, new QTableWidgetItem(oid.left(8)));
                ord_table_->setItem(i, 1, new QTableWidgetItem(o["symbol"].toString()));
                ord_table_->setItem(i, 2, new QTableWidgetItem(o["side"].toString()));
                ord_table_->setItem(i, 3, new QTableWidgetItem(QString::number(o["volume"].toDouble(), 'f', 2)));
                auto* cancel_btn = new QPushButton("X", this);
                cancel_btn->setStyleSheet("background:#ef4444;color:#fff;padding:2px 6px;font-size:9px;");
                connect(cancel_btn, &QPushButton::clicked, this, [this, oid]() { cancel_order(oid); });
                ord_table_->setCellWidget(i, 4, cancel_btn);
            }
        }, this);
}

void ExecutionPanel::fetch_scripts() {
    HttpClient::instance().get("/execution/scripts",
        [this](Result<QJsonDocument> r) {
            if (!r.is_ok() || !scripts_table_) return;
            auto arr = r.value().object()["data"].toArray();
            scripts_table_->setRowCount(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                auto s = arr[i].toObject();
                const QString id = s["id"].toString();
                const QString lang = s["language"].toString().toUpper();
                const QString status = s["status"].toString();
                scripts_table_->setItem(i, 0, new QTableWidgetItem(s["name"].toString()));
                scripts_table_->setItem(i, 1, new QTableWidgetItem(lang));
                scripts_table_->setItem(i, 2, new QTableWidgetItem(s["target"].toString()));
                auto* status_item = new QTableWidgetItem(status);
                status_item->setForeground(status == "running" ? QColor("#22c55e") : QColor("#e5e5e5"));
                scripts_table_->setItem(i, 3, status_item);

                auto* run_btn = new QPushButton(lang == "MQL5" ? "MT5" : "Run", this);
                run_btn->setEnabled(lang != "MQL5");
                run_btn->setStyleSheet("background:#22c55e;color:#080808;padding:2px 8px;font-size:10px;");
                connect(run_btn, &QPushButton::clicked, this, [this, id]() { run_script(id); });
                scripts_table_->setCellWidget(i, 4, run_btn);

                auto* stop_btn = new QPushButton("Stop", this);
                stop_btn->setEnabled(status == "running");
                stop_btn->setStyleSheet("background:#ef4444;color:#fff;padding:2px 8px;font-size:10px;");
                connect(stop_btn, &QPushButton::clicked, this, [this, id]() { stop_script(id); });
                scripts_table_->setCellWidget(i, 5, stop_btn);
            }
        }, this);
}

void ExecutionPanel::update_position_markers() {
    if (!chart_) return;
    // Refresh chart position markers
    chart_->refresh_positions();
}

} // namespace fincept::screens
