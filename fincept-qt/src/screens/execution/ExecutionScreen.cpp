#include "screens/execution/ExecutionScreen.h"
#include "ui/theme/Theme.h"
#include "network/http/HttpClient.h"
#include <QSplitter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QLabel>

namespace fincept::screens {

ExecutionScreen::ExecutionScreen(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* main_splitter = new QSplitter(Qt::Horizontal, this);
    main_splitter->setHandleWidth(1);

    // LEFT: Full MT5 Fleet Chart (all indicators, drawing tools, execution panel, scanner)
    chart_ = new MT5FleetChartPanel(this);
    main_splitter->addWidget(chart_);

    // RIGHT: Order Book (top) + Order Entry (bottom) — exact copy from Crypto tab
    auto* right_splitter = new QSplitter(Qt::Vertical, this);
    right_splitter->setHandleWidth(1);

    orderbook_ = new crypto::CryptoOrderBook(this);
    right_splitter->addWidget(orderbook_);

    orderentry_ = new crypto::CryptoOrderEntry(this);
    right_splitter->addWidget(orderentry_);

    right_splitter->setStretchFactor(0, 3);
    right_splitter->setStretchFactor(1, 2);

    main_splitter->addWidget(right_splitter);

    // Same proportions as Crypto tab
    main_splitter->setSizes({600, 290});
    main_splitter->setStretchFactor(0, 1);
    main_splitter->setStretchFactor(1, 0);

    root->addWidget(main_splitter, 1);

    // Wire signals
    connect(orderentry_, &crypto::CryptoOrderEntry::order_submitted, this,
        [this](const QString& side, const QString& type, double qty, double price,
               double stop, double sl, double tp) {
            QJsonObject body;
            body["symbol"] = "XAUUSD";
            body["side"] = side;
            body["volume"] = qty;
            body["sl"] = sl;
            body["tp"] = tp;
            QString endpoint = type == "LIMIT" ? "limit" : type == "STOP" ? "stop" : "market";
            HttpClient::instance().post(
                QString("http://localhost:8150/mt5/order/%1").arg(endpoint), body,
                [](Result<QJsonDocument>) {}, this);
        });

    connect(orderbook_, &crypto::CryptoOrderBook::price_clicked, this,
        [this](double price) {});

    // Auto-fetch orderbook data every 3s (works for forex, stocks, commodities)
    auto* ob_timer = new QTimer(this);
    connect(ob_timer, &QTimer::timeout, this, [this]() {
        HttpClient::instance().get("http://localhost:8150/mt5/market/orderbook?symbol=XAUUSD",
            [this](Result<QJsonDocument> r) {
                if (!r.is_ok()) return;
                auto obj = r.value().object()["data"].toObject();
                auto bidsArr = obj["bids"].toArray();
                auto asksArr = obj["asks"].toArray();
                double spread = obj["spread"].toDouble();
                double spreadPct = obj["spread_pct"].toDouble();
                QVector<QPair<double, double>> bids, asks;
                for (auto v : bidsArr) {
                    auto a = v.toArray();
                    bids.append({a[0].toDouble(), a[1].toDouble()});
                }
                for (auto v : asksArr) {
                    auto a = v.toArray();
                    asks.append({a[0].toDouble(), a[1].toDouble()});
                }
                orderbook_->set_data(bids, asks, spread, spreadPct);
            }, this);
    });
    ob_timer->start(3000);

    // Set current price on order entry (from chart)
    auto* price_timer = new QTimer(this);
    connect(price_timer, &QTimer::timeout, this, [this]() {
        HttpClient::instance().get("http://localhost:8150/mt5/market/ohlc?symbol=XAUUSD&timeframe=H1&count=1",
            [this](Result<QJsonDocument> r) {
                if (!r.is_ok()) return;
                auto arr = r.value().object()["data"].toArray();
                if (!arr.isEmpty()) {
                    double close = arr[0].toObject()["close"].toDouble();
                    orderentry_->set_current_price(close);
                }
            }, this);
    });
    price_timer->start(3000);
}

} // namespace fincept::screens
