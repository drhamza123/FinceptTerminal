// MT5FleetOrderPanel.cpp — Advanced order panel with REST-backed execution
#include "screens/algo_trading/MT5FleetOrderPanel.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QDoubleValidator>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

namespace fincept::screens {
namespace {
QString ff() { return "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace"; }
QString fmt(double v, int d=4) { return QLocale::system().toString(v, 'f', d); }
}

MT5FleetOrderPanel::MT5FleetOrderPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("fleetOrderPanel"); build_ui(); apply_theme();
    auto* poll = new QTimer(this);
    connect(poll, &QTimer::timeout, this, &MT5FleetOrderPanel::refresh_positions);
    poll->start(3000);
    refresh_positions();
}
MT5FleetOrderPanel::~MT5FleetOrderPanel() = default;

void MT5FleetOrderPanel::build_ui() {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    auto* head = new QWidget(this); head->setObjectName("fleetOrderHead"); head->setFixedHeight(34);
    auto* hl = new QHBoxLayout(head); hl->setContentsMargins(12,0,12,0);
    auto* title = new QLabel("ORDER", head); title->setObjectName("fleetOrderTitle");
    hl->addWidget(title); hl->addStretch();
    root->addWidget(head);

    auto* body = new QWidget(this); body->setObjectName("fleetOrderBody");
    auto* bl = new QVBoxLayout(body); bl->setContentsMargins(14,14,14,14); bl->setSpacing(8);

    auto* sr = new QHBoxLayout;
    symbol_combo_ = new QComboBox(body); symbol_combo_->setObjectName("fleetOrderCombo");
    symbol_combo_->addItems({"XAUUSD","XAGUSD","EURUSD","GBPUSD","USDJPY","BTCUSD","ETHUSD","AAPL","MSFT","TSLA"});
    symbol_combo_->setMinimumWidth(120); symbol_combo_->setFixedHeight(30);
    sr->addWidget(symbol_combo_);

    order_type_combo_ = new QComboBox(body); order_type_combo_->setObjectName("fleetOrderCombo");
    order_type_combo_->addItems({"Market", "Buy Limit", "Sell Limit", "Buy Stop", "Sell Stop",
                                 "OCO (Stop + Limit)", "OTO (Trigger)", "Bracket", "Iceberg"});
    order_type_combo_->setMinimumWidth(150); order_type_combo_->setFixedHeight(30);
    connect(order_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MT5FleetOrderPanel::on_order_type_changed);
    sr->addWidget(order_type_combo_); sr->addStretch();
    bl->addLayout(sr);

    auto* ar = new QHBoxLayout;
    amount_input_ = new QLineEdit(body); amount_input_->setObjectName("fleetOrderInput");
    amount_input_->setPlaceholderText("Volume (lots)"); amount_input_->setFixedHeight(30);
    amount_input_->setValidator(new QDoubleValidator(0,1e12,6,this));
    ar->addWidget(amount_input_,1);
    max_button_ = new QPushButton("MAX", body); max_button_->setObjectName("fleetOrderButton");
    max_button_->setFixedHeight(30); max_button_->setCursor(Qt::PointingHandCursor);
    connect(max_button_, &QPushButton::clicked, this, [this](){ amount_input_->setText(fmt(current_balance_)); });
    ar->addWidget(max_button_);
    bl->addLayout(ar);

    advanced_params_ = new QWidget(body);
    auto* ap = new QVBoxLayout(advanced_params_); ap->setContentsMargins(0,0,0,0); ap->setSpacing(6);
    auto* pr1 = new QHBoxLayout;
    pr1->addWidget(new QLabel("Limit Price:", body));
    limit_price_spin_ = new QDoubleSpinBox(body); limit_price_spin_->setRange(0,1e6); limit_price_spin_->setDecimals(2);
    limit_price_spin_->setObjectName("fleetOrderSpin"); limit_price_spin_->setFixedHeight(28);
    pr1->addWidget(limit_price_spin_,1);
    pr1->addWidget(new QLabel("Stop Price:", body));
    stop_price_spin_ = new QDoubleSpinBox(body); stop_price_spin_->setRange(0,1e6); stop_price_spin_->setDecimals(2);
    stop_price_spin_->setObjectName("fleetOrderSpin"); stop_price_spin_->setFixedHeight(28);
    pr1->addWidget(stop_price_spin_,1);
    ap->addLayout(pr1);

    auto* pr2 = new QHBoxLayout;
    pr2->addWidget(new QLabel("SL:", body));
    sl_spin_ = new QDoubleSpinBox(body); sl_spin_->setRange(0,1e6); sl_spin_->setDecimals(2);
    sl_spin_->setObjectName("fleetOrderSpin"); sl_spin_->setFixedHeight(28);
    pr2->addWidget(sl_spin_,1);
    pr2->addWidget(new QLabel("TP:", body));
    tp_spin_ = new QDoubleSpinBox(body); tp_spin_->setRange(0,1e6); tp_spin_->setDecimals(2);
    tp_spin_->setObjectName("fleetOrderSpin"); tp_spin_->setFixedHeight(28);
    pr2->addWidget(tp_spin_,1);
    pr2->addWidget(new QLabel("Trail:", body));
    trailing_dist_spin_ = new QDoubleSpinBox(body); trailing_dist_spin_->setRange(0,1e6); trailing_dist_spin_->setDecimals(0);
    trailing_dist_spin_->setObjectName("fleetOrderSpin"); trailing_dist_spin_->setFixedHeight(28);
    trailing_dist_spin_->setValue(50);
    pr2->addWidget(trailing_dist_spin_,1);
    trailing_stop_cb_ = new QCheckBox("Trail", body);
    pr2->addWidget(trailing_stop_cb_);
    ap->addLayout(pr2);

    oco_params_ = new QWidget(body);
    auto* oc = new QHBoxLayout(oco_params_); oc->setContentsMargins(0,0,0,0);
    oc->addWidget(new QLabel("OCO Limit:", body));
    oco_limit_spin_ = new QDoubleSpinBox(body); oco_limit_spin_->setRange(0,1e6); oco_limit_spin_->setDecimals(2);
    oco_limit_spin_->setObjectName("fleetOrderSpin"); oco_limit_spin_->setFixedHeight(28);
    oc->addWidget(oco_limit_spin_,1);
    oc->addWidget(new QLabel("OCO Stop:", body));
    oco_stop_spin_ = new QDoubleSpinBox(body); oco_stop_spin_->setRange(0,1e6); oco_stop_spin_->setDecimals(2);
    oco_stop_spin_->setObjectName("fleetOrderSpin"); oco_stop_spin_->setFixedHeight(28);
    oc->addWidget(oco_stop_spin_,1); oc->addStretch();
    oco_params_->hide();
    ap->addWidget(oco_params_);

    auto* oto_w = new QWidget(body);
    auto* ot = new QHBoxLayout(oto_w);
    ot->addWidget(new QLabel("Trigger:", body));
    oto_trigger_combo_ = new QComboBox(body); oto_trigger_combo_->addItems({"Above", "Below"});
    oto_trigger_combo_->setObjectName("fleetOrderCombo"); oto_trigger_combo_->setFixedHeight(28);
    ot->addWidget(oto_trigger_combo_);
    ot->addWidget(new QLabel("Price:", body));
    oto_trigger_price_ = new QDoubleSpinBox(body); oto_trigger_price_->setRange(0,1e6); oto_trigger_price_->setDecimals(2);
    oto_trigger_price_->setObjectName("fleetOrderSpin"); oto_trigger_price_->setFixedHeight(28);
    ot->addWidget(oto_trigger_price_,1); ot->addStretch();
    ap->addWidget(oto_w);
    advanced_params_->hide();
    bl->addWidget(advanced_params_);

    balance_label_ = new QLabel("Balance: —", body); balance_label_->setObjectName("fleetOrderMeta");
    bl->addWidget(balance_label_);
    estimate_label_ = new QLabel("—", body); estimate_label_->setObjectName("fleetOrderEstimate");
    estimate_label_->setFixedHeight(30); bl->addWidget(estimate_label_);

    error_strip_ = new QFrame(body); error_strip_->setObjectName("fleetOrderErrorStrip");
    auto* esl = new QHBoxLayout(error_strip_); esl->setContentsMargins(10,6,10,6);
    auto* esi = new QLabel("!", error_strip_); esi->setObjectName("fleetOrderErrorIcon");
    error_text_ = new QLabel("", error_strip_); error_text_->setObjectName("fleetOrderErrorText");
    esl->addWidget(esi); esl->addWidget(error_text_,1); error_strip_->hide(); bl->addWidget(error_strip_);

    auto* br = new QHBoxLayout;
    buy_button_ = new QPushButton("BUY", body); buy_button_->setObjectName("fleetOrderBuyButton");
    buy_button_->setFixedHeight(36); buy_button_->setCursor(Qt::PointingHandCursor);
    sell_button_ = new QPushButton("SELL", body); sell_button_->setObjectName("fleetOrderSellButton");
    sell_button_->setFixedHeight(36); sell_button_->setCursor(Qt::PointingHandCursor);
    br->addWidget(buy_button_); br->addWidget(sell_button_); bl->addLayout(br);

    order_status_label_ = new QLabel("", body); order_status_label_->setObjectName("fleetOrderMeta");
    bl->addWidget(order_status_label_);
    bl->addStretch();

    auto* pt = new QLabel("OPEN POSITIONS"); pt->setObjectName("fleetOrderMeta");
    bl->addWidget(pt);
    positions_table_ = new QTableWidget(0,7,body); positions_table_->setObjectName("fleetOrderTable");
    positions_table_->setHorizontalHeaderLabels({"Ticket","Symbol","Side","Volume","Entry","P&L","SL/TP"});
    positions_table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    positions_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    positions_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    positions_table_->verticalHeader()->setVisible(false);
    bl->addWidget(positions_table_,1);
    root->addWidget(body,1);

    connect(symbol_combo_, &QComboBox::currentIndexChanged, this, &MT5FleetOrderPanel::on_symbol_changed);
    connect(amount_input_, &QLineEdit::textChanged, this, &MT5FleetOrderPanel::on_amount_changed);
    connect(buy_button_, &QPushButton::clicked, this, [this]{ place_order("BUY"); });
    connect(sell_button_, &QPushButton::clicked, this, [this]{ place_order("SELL"); });
    on_symbol_changed(0);
}

void MT5FleetOrderPanel::apply_theme() {
    setStyleSheet(QString(
        "QWidget#fleetOrderPanel{background:%1;}"
        "QWidget#fleetOrderHead{background:%2;border-bottom:1px solid %3;}"
        "QLabel#fleetOrderTitle{color:%4;font-family:%5;font-size:11px;font-weight:700;letter-spacing:1.4px;background:transparent;}"
        "QWidget#fleetOrderBody{background:%1;}"
        "QComboBox#fleetOrderCombo,QDoubleSpinBox#fleetOrderSpin{background:%2;color:%6;border:1px solid %3;font-family:%5;font-size:10px;padding:0 6px;}"
        "QLineEdit#fleetOrderInput{background:%2;color:%6;border:1px solid %3;font-family:%5;font-size:14px;padding:0 8px;}"
        "QLineEdit#fleetOrderInput:focus{border-color:%4;}"
        "QLabel#fleetOrderMeta{color:%7;font-family:%5;font-size:10px;background:transparent;}"
        "QLabel#fleetOrderEstimate{background:%8;color:%6;border:1px solid %3;font-family:%5;font-size:12px;padding:0 8px;}"
        "QPushButton#fleetOrderButton{background:%8;color:%7;border:1px solid %3;font-family:%5;font-size:11px;font-weight:700;padding:0 14px;}"
        "QPushButton#fleetOrderButton:hover{background:%9;color:%6;border-color:%10;}"
        "QPushButton#fleetOrderBuyButton{background:#22c55e;color:#FFF;border:none;font-family:%5;font-size:14px;font-weight:700;letter-spacing:1px;}"
        "QPushButton#fleetOrderBuyButton:hover{background:#16a34a;}"
        "QPushButton#fleetOrderBuyButton:disabled{background:%8;color:%7;border:1px solid %3;}"
        "QPushButton#fleetOrderSellButton{background:#ef4444;color:#FFF;border:none;font-family:%5;font-size:14px;font-weight:700;letter-spacing:1px;}"
        "QPushButton#fleetOrderSellButton:hover{background:#dc2626;}"
        "QPushButton#fleetOrderSellButton:disabled{background:%8;color:%7;border:1px solid %3;}"
        "QFrame#fleetOrderErrorStrip{background:rgba(220,38,38,0.10);border:1px solid %11;}"
        "QLabel#fleetOrderErrorIcon{color:%11;font-family:%5;font-size:13px;font-weight:700;background:transparent;}"
        "QLabel#fleetOrderErrorText{color:%11;font-family:%5;font-size:11px;background:transparent;}"
        "QTableWidget#fleetOrderTable{background:%1;color:%6;gridline-color:%3;border:1px solid %3;}"
        "QHeaderView::section{background:%2;color:%6;padding:4px 8px;font-size:9px;font-weight:700;border:none;border-bottom:1px solid %3;}"
    ).arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(),
          ui::colors::AMBER(), ff(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_TERTIARY(),
          ui::colors::BG_RAISED(), ui::colors::BG_HOVER(), ui::colors::BORDER_BRIGHT(),
          ui::colors::NEGATIVE()));
}

void MT5FleetOrderPanel::set_symbol(const QString& sym) {
    int idx = symbol_combo_->findText(sym); if(idx>=0) symbol_combo_->setCurrentIndex(idx);
}
void MT5FleetOrderPanel::set_balance(double bal) { current_balance_=bal; update_balance_label(); }

void MT5FleetOrderPanel::refresh_positions() {
    HttpClient::instance().get("/mt5/positions",
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) return;
            auto arr = r.value().object()["data"].toArray();
            positions_table_->setRowCount(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                auto obj = arr[i].toObject();
                positions_table_->setItem(i, 0, new QTableWidgetItem(QString::number(obj["ticket"].toInt())));
                positions_table_->setItem(i, 1, new QTableWidgetItem(obj["symbol"].toString()));
                positions_table_->setItem(i, 2, new QTableWidgetItem(obj["side"].toString()));
                positions_table_->setItem(i, 3, new QTableWidgetItem(fmt(obj["volume"].toDouble(), 2)));
                positions_table_->setItem(i, 4, new QTableWidgetItem(fmt(obj["entry_price"].toDouble())));
                auto* pnl = new QTableWidgetItem(fmt(obj["pnl"].toDouble(), 2));
                pnl->setForeground(obj["pnl"].toDouble() >= 0 ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE()));
                positions_table_->setItem(i, 5, pnl);
                QString sltp = QString("SL:%1 TP:%2").arg(fmt(obj["sl"].toDouble())).arg(fmt(obj["tp"].toDouble()));
                positions_table_->setItem(i, 6, new QTableWidgetItem(sltp));
            }
            order_status_label_->setText(QString("%1 position(s)").arg(arr.size()));
        }, this);
}

void MT5FleetOrderPanel::place_order(const QString& side) {
    bool ok; double v = QLocale::system().toDouble(amount_input_->text(), &ok);
    if (!ok || v <= 0) { show_error("Enter a valid amount."); return; }
    hide_error();

    QString type = order_type_combo_->currentText();
    QString symbol = symbol_combo_->currentText();
    QJsonObject payload;
    payload["symbol"] = symbol;
    payload["side"] = side;
    payload["volume"] = v;

    QString endpoint;
    if (type == "Market") {
        endpoint = "/mt5/order/market";
        if (sl_spin_->value() > 0) payload["sl"] = sl_spin_->value();
        if (tp_spin_->value() > 0) payload["tp"] = tp_spin_->value();
    } else if (type == "Buy Limit" || type == "Sell Limit") {
        endpoint = "/mt5/order/limit";
        payload["price"] = limit_price_spin_->value();
        if (sl_spin_->value() > 0) payload["sl"] = sl_spin_->value();
        if (tp_spin_->value() > 0) payload["tp"] = tp_spin_->value();
    } else if (type == "Buy Stop" || type == "Sell Stop") {
        endpoint = "/mt5/order/stop";
        payload["stop_price"] = stop_price_spin_->value();
        if (limit_price_spin_->value() > 0) payload["limit_price"] = limit_price_spin_->value();
        if (sl_spin_->value() > 0) payload["sl"] = sl_spin_->value();
        if (tp_spin_->value() > 0) payload["tp"] = tp_spin_->value();
    } else if (type.startsWith("OCO")) {
        endpoint = "/mt5/order/oco";
        payload["stop_price"] = oco_stop_spin_->value();
        payload["limit_price"] = oco_limit_spin_->value();
    } else if (type.startsWith("OTO")) {
        endpoint = "/mt5/order/oto";
        payload["trigger_price"] = oto_trigger_price_->value();
        payload["trigger_direction"] = oto_trigger_combo_->currentText().toLower();
        payload["trigger_side"] = (side == "BUY") ? "SELL" : "BUY";
        payload["target_side"] = side;
        payload["target_volume"] = v;
        payload["target_price"] = limit_price_spin_->value();
        if (sl_spin_->value() > 0) payload["sl"] = sl_spin_->value();
        if (tp_spin_->value() > 0) payload["tp"] = tp_spin_->value();
    } else if (type == "Bracket") {
        endpoint = "/mt5/order/bracket";
        payload["entry_price"] = limit_price_spin_->value();
        payload["sl"] = sl_spin_->value();
        payload["tp"] = tp_spin_->value();
    } else if (type == "Iceberg") {
        endpoint = "/mt5/order/iceberg";
        payload["price"] = limit_price_spin_->value();
        payload["visible_volume"] = v * 0.2;
        if (sl_spin_->value() > 0) payload["sl"] = sl_spin_->value();
        if (tp_spin_->value() > 0) payload["tp"] = tp_spin_->value();
    }

    order_status_label_->setText(QString("Placing %1 %2 %3...").arg(type).arg(side).arg(symbol));

    HttpClient::instance().post(endpoint, payload,
        [this, type, side, symbol](Result<QJsonDocument> r) {
            if (r.is_err()) {
                QString err = QString::fromStdString(r.error());
                order_status_label_->setText("Order failed: " + err);
                show_error(err);
                return;
            }
            auto obj = r.value().object();
            if (obj["success"].toBool()) {
                order_status_label_->setText(QString("%1 %2 %3 placed").arg(type).arg(side).arg(symbol));
                amount_input_->clear();
                refresh_positions();
            } else {
                order_status_label_->setText("Order failed: " + obj["error"].toString());
                show_error(obj["error"].toString());
            }
        }, this);

    if (trailing_stop_cb_->isChecked()) {
        QTimer::singleShot(1000, this, [this]() {
            QJsonObject ts;
            ts["ticket"] = 0; // Will be updated when ticket known
            ts["distance"] = trailing_dist_spin_->value();
            HttpClient::instance().post("/mt5/order/trailing-stop", ts,
                [](Result<QJsonDocument>) {}, this);
        });
    }
}

void MT5FleetOrderPanel::on_order_type_changed(int) {
    QString type = order_type_combo_->currentText();
    bool show_advanced = (type != "Market");
    advanced_params_->setVisible(show_advanced);
    oco_params_->setVisible(type.startsWith("OCO"));
    bool bracket = (type == "Bracket");
    sl_spin_->parentWidget()->setVisible(bracket || type.contains("Limit") || type.contains("Stop") || type == "Iceberg");
    tp_spin_->parentWidget()->setVisible(bracket || type.contains("Limit") || type == "Iceberg");
    trailing_stop_cb_->parentWidget()->setVisible(type == "Market" || bracket);
}

void MT5FleetOrderPanel::on_symbol_changed(int idx) {
    if(idx<0) return; current_symbol_=symbol_combo_->currentText();
    update_balance_label(); hide_error();
    HttpClient::instance().get("/mt5/market/ohlc?symbol="+current_symbol_+"&timeframe=H1&count=3",
        [this](Result<QJsonDocument> r){
            if(r.is_err()) return;
            auto d=r.value().object()["data"].toArray();
            if(!d.isEmpty()) {
                double price = d.last().toObject()["close"].toDouble();
                estimate_label_->setText(QString("$%1").arg(price,0,'f',2));
                limit_price_spin_->setValue(price);
                stop_price_spin_->setValue(price * 0.99);
                sl_spin_->setValue(price * 0.98);
                tp_spin_->setValue(price * 1.02);
                oco_limit_spin_->setValue(price * 1.01);
                oco_stop_spin_->setValue(price * 0.99);
                oto_trigger_price_->setValue(price);
            }
        },this);
}

void MT5FleetOrderPanel::on_amount_changed(const QString& s) {
    bool ok; double v=QLocale::system().toDouble(s,&ok);
    buy_button_->setEnabled(ok&&v>0); sell_button_->setEnabled(ok&&v>0);
}

void MT5FleetOrderPanel::update_balance_label() {
    HttpClient::instance().get("/mt5/account",
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) return;
            auto data = r.value().object()["data"].toObject();
            current_balance_ = data["balance"].toDouble();
            balance_label_->setText(QString("Balance: %1 | Equity: %2")
                .arg(fmt(current_balance_)).arg(fmt(data["equity"].toDouble())));
        }, this);
}

void MT5FleetOrderPanel::show_error(const QString& msg) {
    if(error_strip_&&error_text_){error_strip_->show();error_text_->setText(msg);}
    QTimer::singleShot(5000, this, [this](){ hide_error(); });
}
void MT5FleetOrderPanel::hide_error() { if(error_strip_) error_strip_->hide(); }
void MT5FleetOrderPanel::showEvent(QShowEvent* e) { QWidget::showEvent(e); refresh_positions(); }
void MT5FleetOrderPanel::hideEvent(QHideEvent* e) { QWidget::hideEvent(e); }

} // namespace fincept::screens
