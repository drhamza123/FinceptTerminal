#include "screens/execution/ExecutionScreen.h"
#include "trading/SmartOrderEngine.h"
#include "ui/theme/Theme.h"
#include <QSplitter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QWebSocket>
#include <QUrl>
#include <QTimer>

namespace fincept::screens {

ExecutionScreen::ExecutionScreen(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* main_splitter = new QSplitter(Qt::Horizontal, this);
    main_splitter->setHandleWidth(1);

    chart_ = new MT5FleetChartPanel(this);
    main_splitter->addWidget(chart_);

    auto* right_splitter = new QSplitter(Qt::Vertical, this);
    right_splitter->setHandleWidth(1);

    orderbook_ = new crypto::CryptoOrderBook(this);
    right_splitter->addWidget(orderbook_);

    orderentry_ = new crypto::CryptoOrderEntry(this);
    right_splitter->addWidget(orderentry_);

    right_splitter->setStretchFactor(0, 3);
    right_splitter->setStretchFactor(1, 2);
    main_splitter->addWidget(right_splitter);

    main_splitter->setSizes({600, 290});
    main_splitter->setStretchFactor(0, 1);
    main_splitter->setStretchFactor(1, 0);
    root->addWidget(main_splitter, 1);

    // ── WebSocket for real-time market data (replaces HTTP polling) ──
    auto* ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(ws, &QWebSocket::textMessageReceived, this, [this](const QString& msg) {
        QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
        if (!doc.isObject()) return;
        auto obj = doc.object();
        QString type = obj["type"].toString();

        if (type == "orderbook" || type == "market_data") {
            auto data = obj["data"].toObject();
            auto bidsArr = data["bids"].toArray();
            auto asksArr = data["asks"].toArray();
            double spread = data["spread"].toDouble();
            double spreadPct = data["spread_pct"].toDouble();
            double price = data["price"].toDouble();

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
            if (price > 0) orderentry_->set_current_price(price);

        } else if (type == "price") {
            double price = obj["price"].toDouble();
            if (price > 0) orderentry_->set_current_price(price);
        }
    });

    // Auto-reconnect with timer
    auto* ws_timer = new QTimer(this);
    connect(ws_timer, &QTimer::timeout, this, [ws]() {
        if (ws->state() != QAbstractSocket::ConnectedState)
            ws->open(QUrl("ws://localhost:8150/ws/market-data?symbol=XAUUSD"));
    });
    ws_timer->start(5000);
    ws->open(QUrl("ws://localhost:8150/ws/market-data?symbol=XAUUSD")); // Connect immediately

    // ── Route ALL orders through ZMQ (low-latency) ──
    auto* engine = new trading::SmartOrderEngine(this);
    engine->connectToGateway("ws://localhost:8150/ws/orders");

    connect(orderentry_, &crypto::CryptoOrderEntry::order_submitted, this,
        [this, engine](const QString& side, const QString& type, double qty, double price,
               double stop, double sl, double tp) {
            // All order types go through ZMQ
            engine->submitOrder("XAUUSD", side, qty, sl, tp);
        });

    connect(orderbook_, &crypto::CryptoOrderBook::price_clicked, this,
        [this](double) {});

    // DOM One-Click: click orderbook price → instant order via ZMQ
    connect(orderbook_, &crypto::CryptoOrderBook::one_click_order, this,
        [this, engine](const QString& side, double, double) {
            engine->submitOrder("XAUUSD", side, 0.1);
        });
}

} // namespace fincept::screens
