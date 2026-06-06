// MT5FleetOrderBookPanel.cpp — Depth of Market / Order Book
#include "screens/algo_trading/MT5FleetOrderBookPanel.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QInputDialog>
#include <QMessageBox>

namespace fincept::screens {

MT5FleetOrderBookPanel::MT5FleetOrderBookPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    apply_theme();
    
    // Auto-refresh every 2 seconds
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MT5FleetOrderBookPanel::refresh_order_book);
    timer->start(2000);
    
    refresh_order_book();
}

MT5FleetOrderBookPanel::~MT5FleetOrderBookPanel() = default;

void MT5FleetOrderBookPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setObjectName("orderBookHeader");
    header->setFixedHeight(40);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(12, 0, 12, 0);
    h_layout->setSpacing(8);

    symbol_label_ = new QLabel("XAUUSD", header);
    symbol_label_->setObjectName("orderBookSymbolLabel");
    h_layout->addWidget(symbol_label_);

    last_price_label_ = new QLabel("$0.00", header);
    last_price_label_->setObjectName("orderBookPriceLabel");
    h_layout->addWidget(last_price_label_);

    h_layout->addStretch();

    spread_label_ = new QLabel("Spread: 0.0", header);
    spread_label_->setObjectName("orderBookSpreadLabel");
    h_layout->addWidget(spread_label_);

    root->addWidget(header);

    // Order book tables
    auto* tables_widget = new QWidget(this);
    auto* tables_layout = new QHBoxLayout(tables_widget);
    tables_layout->setContentsMargins(8, 8, 8, 8);
    tables_layout->setSpacing(8);

    // Bids table (left)
    auto* bids_widget = new QWidget(tables_widget);
    auto* bids_layout = new QVBoxLayout(bids_widget);
    bids_layout->setContentsMargins(0, 0, 0, 0);
    bids_layout->setSpacing(4);

    auto* bids_header = new QLabel("BIDS", bids_widget);
    bids_header->setObjectName("orderBookBidsHeader");
    bids_header->setAlignment(Qt::AlignCenter);
    bids_layout->addWidget(bids_header);

    bids_table_ = new QTableWidget(0, 3, bids_widget);
    bids_table_->setObjectName("orderBookBidsTable");
    bids_table_->setHorizontalHeaderLabels({"Price", "Volume", "Total"});
    bids_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    bids_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    bids_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    bids_table_->verticalHeader()->setVisible(false);
    bids_layout->addWidget(bids_table_, 1);

    tables_layout->addWidget(bids_widget, 1);

    // Asks table (right)
    auto* asks_widget = new QWidget(tables_widget);
    auto* asks_layout = new QVBoxLayout(asks_widget);
    asks_layout->setContentsMargins(0, 0, 0, 0);
    asks_layout->setSpacing(4);

    auto* asks_header = new QLabel("ASKS", asks_widget);
    asks_header->setObjectName("orderBookAsksHeader");
    asks_header->setAlignment(Qt::AlignCenter);
    asks_layout->addWidget(asks_header);

    asks_table_ = new QTableWidget(0, 3, asks_widget);
    asks_table_->setObjectName("orderBookAsksTable");
    asks_table_->setHorizontalHeaderLabels({"Price", "Volume", "Total"});
    asks_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    asks_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    asks_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    asks_table_->verticalHeader()->setVisible(false);
    asks_layout->addWidget(asks_table_, 1);

    tables_layout->addWidget(asks_widget, 1);

    root->addWidget(tables_widget, 1);

    // Execution buttons
    auto* buttons_widget = new QWidget(this);
    buttons_widget->setObjectName("orderBookButtons");
    buttons_widget->setFixedHeight(50);
    auto* b_layout = new QHBoxLayout(buttons_widget);
    b_layout->setContentsMargins(12, 8, 12, 8);
    b_layout->setSpacing(12);

    buy_btn_ = new QPushButton("BUY", buttons_widget);
    buy_btn_->setObjectName("orderBookBuyBtn");
    buy_btn_->setFixedHeight(34);
    connect(buy_btn_, &QPushButton::clicked, this, &MT5FleetOrderBookPanel::on_buy_clicked);
    b_layout->addWidget(buy_btn_, 1);

    sell_btn_ = new QPushButton("SELL", buttons_widget);
    sell_btn_->setObjectName("orderBookSellBtn");
    sell_btn_->setFixedHeight(34);
    connect(sell_btn_, &QPushButton::clicked, this, &MT5FleetOrderBookPanel::on_sell_clicked);
    b_layout->addWidget(sell_btn_, 1);

    root->addWidget(buttons_widget);
}

void MT5FleetOrderBookPanel::apply_theme() {
    setStyleSheet(QString(
        "QWidget#orderBookHeader{background:%1;border-bottom:1px solid %2;}"
        "QLabel#orderBookSymbolLabel{color:%3;font-size:14px;font-weight:700;}"
        "QLabel#orderBookPriceLabel{color:%4;font-size:14px;font-weight:600;}"
        "QLabel#orderBookSpreadLabel{color:%5;font-size:11px;}"
        "QLabel#orderBookBidsHeader{color:%4;font-size:11px;font-weight:700;background:%1;padding:4px;}"
        "QLabel#orderBookAsksHeader{color:%6;font-size:11px;font-weight:700;background:%1;padding:4px;}"
        "QTableWidget#orderBookBidsTable{background:%7;color:%3;border:1px solid %2;}"
        "QTableWidget#orderBookAsksTable{background:%7;color:%3;border:1px solid %2;}"
        "QTableWidget::item{padding:2px 4px;font-size:10px;}"
        "QWidget#orderBookButtons{background:%1;border-top:1px solid %2;}"
        "QPushButton#orderBookBuyBtn{background:%4;color:#FFF;border:none;font-size:12px;font-weight:700;}"
        "QPushButton#orderBookBuyBtn:hover{background:#00B85C;}"
        "QPushButton#orderBookSellBtn{background:%6;color:#FFF;border:none;font-size:12px;font-weight:700;}"
        "QPushButton#orderBookSellBtn:hover{background:#FF3333;}"
    ).arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
          ui::colors::POSITIVE(), ui::colors::TEXT_TERTIARY(), ui::colors::NEGATIVE(),
          ui::colors::BG_BASE()));
}

void MT5FleetOrderBookPanel::set_symbol(const QString& symbol) {
    current_symbol_ = symbol;
    symbol_label_->setText(symbol);
    refresh_order_book();
}

void MT5FleetOrderBookPanel::refresh_order_book() {
    HttpClient::instance().get(
        QString("http://localhost:8150/mt5/market/orderbook?symbol=%1").arg(current_symbol_),
        [this](Result<QJsonDocument> result) {
            if (result.is_err()) return;
            auto doc = result.value();
            auto obj = doc.object();
            
            double last_price = obj["last_price"].toDouble();
            double spread = obj["spread"].toDouble();
            
            last_price_label_->setText(QString("$%1").arg(last_price, 0, 'f', 2));
            spread_label_->setText(QString("Spread: %1").arg(spread, 0, 'f', 1));
            
            auto bids = obj["bids"].toArray();
            auto asks = obj["asks"].toArray();
            update_order_book(bids, asks);
        }, this);
}

void MT5FleetOrderBookPanel::update_order_book(const QJsonArray& bids, const QJsonArray& asks) {
    // Update bids table
    bids_table_->setRowCount(qMin(bids.size(), 15));
    double bid_total = 0;
    for (int i = 0; i < qMin(bids.size(), 15); ++i) {
        auto bid = bids[i].toObject();
        double price = bid["price"].toDouble();
        double volume = bid["volume"].toDouble();
        bid_total += volume;
        
        bids_table_->setItem(i, 0, new QTableWidgetItem(QString("$%1").arg(price, 0, 'f', 2)));
        bids_table_->setItem(i, 1, new QTableWidgetItem(QString::number(volume, 'f', 2)));
        bids_table_->setItem(i, 2, new QTableWidgetItem(QString::number(bid_total, 'f', 2)));
        
        // Color bid rows
        for (int col = 0; col < 3; ++col) {
            auto* item = bids_table_->item(i, col);
            if (item) item->setForeground(QColor(ui::colors::POSITIVE()));
        }
    }

    // Update asks table
    asks_table_->setRowCount(qMin(asks.size(), 15));
    double ask_total = 0;
    for (int i = 0; i < qMin(asks.size(), 15); ++i) {
        auto ask = asks[i].toObject();
        double price = ask["price"].toDouble();
        double volume = ask["volume"].toDouble();
        ask_total += volume;
        
        asks_table_->setItem(i, 0, new QTableWidgetItem(QString("$%1").arg(price, 0, 'f', 2)));
        asks_table_->setItem(i, 1, new QTableWidgetItem(QString::number(volume, 'f', 2)));
        asks_table_->setItem(i, 2, new QTableWidgetItem(QString::number(ask_total, 'f', 2)));
        
        // Color ask rows
        for (int col = 0; col < 3; ++col) {
            auto* item = asks_table_->item(i, col);
            if (item) item->setForeground(QColor(ui::colors::NEGATIVE()));
        }
    }
}

void MT5FleetOrderBookPanel::on_buy_clicked() {
    if (asks_table_->rowCount() == 0) return;
    auto* price_item = asks_table_->item(0, 0);
    if (!price_item) return;
    QString price_text = price_item->text().remove("$");
    double price = price_text.toDouble();

    bool ok;
    double volume = QInputDialog::getDouble(this, "Market Buy",
        QString("BUY %1 @ $%2\nVolume (lots):").arg(current_symbol_).arg(price, 0, 'f', 2),
        0.1, 0.01, 100, 2, &ok);
    if (!ok) return;

    QJsonObject payload;
    payload["symbol"] = current_symbol_;
    payload["side"] = "BUY";
    payload["volume"] = volume;

    HttpClient::instance().post("http://localhost:8150/mt5/order/market", payload,
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) { last_price_label_->setText("BUY failed"); return; }
            auto obj = r.value().object();
            if (obj["success"].toBool())
                last_price_label_->setText("BUY order placed");
            else
                last_price_label_->setText("BUY failed: " + obj["error"].toString());
        }, this);
}

void MT5FleetOrderBookPanel::on_sell_clicked() {
    if (bids_table_->rowCount() == 0) return;
    auto* price_item = bids_table_->item(0, 0);
    if (!price_item) return;
    QString price_text = price_item->text().remove("$");
    double price = price_text.toDouble();

    bool ok;
    double volume = QInputDialog::getDouble(this, "Market Sell",
        QString("SELL %1 @ $%2\nVolume (lots):").arg(current_symbol_).arg(price, 0, 'f', 2),
        0.1, 0.01, 100, 2, &ok);
    if (!ok) return;

    QJsonObject payload;
    payload["symbol"] = current_symbol_;
    payload["side"] = "SELL";
    payload["volume"] = volume;

    HttpClient::instance().post("http://localhost:8150/mt5/order/market", payload,
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) { last_price_label_->setText("SELL failed"); return; }
            auto obj = r.value().object();
            if (obj["success"].toBool())
                last_price_label_->setText("SELL order placed");
            else
                last_price_label_->setText("SELL failed: " + obj["error"].toString());
        }, this);
}

} // namespace fincept::screens
