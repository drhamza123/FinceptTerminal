#include "screens/execution/ExecutionScreen.h"
#include "trading/SmartOrderEngine.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHBoxLayout>
#include <QUrl>
#include <QDateTime>

namespace fincept::screens {

ExecutionScreen::ExecutionScreen(QWidget* parent) : QWidget(parent) {
    build_ui();

    // ── Market data WebSocket ──
    auto* ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(ws, &QWebSocket::textMessageReceived, this, [this](const QString& msg) {
        QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
        if (!doc.isObject()) return;
        auto obj = doc.object();
        if (obj["type"].toString() == "market_data") {
            auto data = obj["data"].toObject();
            auto bidsArr = data["bids"].toArray();
            auto asksArr = data["asks"].toArray();
            QVector<QPair<double, double>> bids, asks;
            for (auto v : bidsArr) { auto a = v.toArray(); bids.append({a[0].toDouble(), a[1].toDouble()}); }
            for (auto v : asksArr) { auto a = v.toArray(); asks.append({a[0].toDouble(), a[1].toDouble()}); }
            orderbook_->set_data(bids, asks, data["spread"].toDouble(), data["spread_pct"].toDouble());
            if (data.contains("price")) {
                double p = data["price"].toDouble();
                orderentry_->set_current_price(p);
            }
        }
    });
    auto* ws_timer = new QTimer(this);
    connect(ws_timer, &QTimer::timeout, this, [ws]() {
        if (ws->state() != QAbstractSocket::ConnectedState)
            ws->open(QUrl("ws://localhost:8150/ws/market-data?symbol=XAUUSD"));
    });
    ws_timer->start(5000);
    ws->open(QUrl("ws://localhost:8150/ws/market-data?symbol=XAUUSD"));

    // ── ZMQ engine ──
    auto* engine = new trading::SmartOrderEngine(this);
    engine->connectToGateway("ws://localhost:8150/ws/orders");
    connect(orderentry_, &crypto::CryptoOrderEntry::order_submitted, this,
        [this, engine](const QString& side, const QString&, double qty, double, double, double sl, double tp) {
            engine->submitOrder("XAUUSD", side, qty, sl, tp);
        });
    connect(orderbook_, &crypto::CryptoOrderBook::one_click_order, this,
        [this, engine](const QString& side, double, double) {
            engine->submitOrder("XAUUSD", side, 0.1);
        });

    // ── Account refresh ──
    auto* acct_timer = new QTimer(this);
    connect(acct_timer, &QTimer::timeout, this, &ExecutionScreen::refresh_account);
    acct_timer->start(3000);
    QTimer::singleShot(1000, this, &ExecutionScreen::refresh_account);
}

void ExecutionScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Command bar ──
    auto* cmd_bar = new QWidget(this);
    cmd_bar->setObjectName("cryptoCmdBar");
    cmd_bar->setFixedHeight(32);
    auto* cmd = new QHBoxLayout(cmd_bar); cmd->setContentsMargins(8,0,8,0);

    symbol_input_ = new QLineEdit("XAUUSD", this);
    symbol_input_->setObjectName("cryptoSymbolInput");
    connect(symbol_input_, &QLineEdit::returnPressed, this, &ExecutionScreen::on_symbol_submit);
    cmd->addWidget(symbol_input_);

    mode_btn_ = new QPushButton("PAPER", this);
    mode_btn_->setObjectName("cryptoModeBtn");
    mode_btn_->setCheckable(true);
    mode_btn_->setFixedHeight(22);
    mode_btn_->setProperty("mode", "paper");
    connect(mode_btn_, &QPushButton::clicked, this, &ExecutionScreen::on_mode_toggled);
    cmd->addWidget(mode_btn_);

    ws_status_ = new QLabel("WS: --", this);
    ws_status_->setObjectName("cryptoWsStatus");
    cmd->addWidget(ws_status_);
    cmd->addStretch();

    auto* clock_lbl = new QLabel(QDateTime::currentDateTime().toString("HH:mm:ss"), this);
    clock_lbl->setObjectName("cryptoClock");
    auto* clock_timer = new QTimer(this);
    connect(clock_timer, &QTimer::timeout, clock_lbl, [clock_lbl]() {
        clock_lbl->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
    });
    clock_timer->start(1000);
    cmd->addWidget(clock_lbl);

    root->addWidget(cmd_bar);

    // ── Ticker bar ──
    ticker_ = new crypto::CryptoTickerBar(this);
    root->addWidget(ticker_);

    // ── MAIN 3-PANEL SPLITTER (like Crypto tab) ──
    auto* main_splitter = new QSplitter(Qt::Horizontal, this);
    main_splitter->setHandleWidth(1);

    // LEFT: Watchlist
    QStringList symbols = {"XAUUSD","XAGUSD","EURUSD","GBPUSD","USDJPY","BTCUSD","ETHUSD","SPY","QQQ","AAPL","MSFT","TSLA"};
    watchlist_ = new crypto::CryptoWatchlist(this);
    watchlist_->set_symbols(symbols);
    watchlist_->set_active_symbol("XAUUSD");
    connect(watchlist_, &crypto::CryptoWatchlist::symbol_selected, this, &ExecutionScreen::on_symbol_selected);
    main_splitter->addWidget(watchlist_);

    // CENTER: Chart + Bottom Panel
    auto* center_splitter = new QSplitter(Qt::Vertical, this);
    center_splitter->setHandleWidth(1);
    chart_ = new MT5FleetChartPanel(this);
    center_splitter->addWidget(chart_);
    bottom_ = new crypto::CryptoBottomPanel(this);
    center_splitter->addWidget(bottom_);
    center_splitter->setStretchFactor(0, 5);
    center_splitter->setStretchFactor(1, 2);
    main_splitter->addWidget(center_splitter);

    // RIGHT: Wallet + OrderBook + OrderEntry
    auto* right_panel = new QWidget(this);
    auto* right_layout = new QVBoxLayout(right_panel);
    right_layout->setContentsMargins(0,0,0,0); right_layout->setSpacing(0);

    auto* wallet = new QWidget(this);
    wallet->setStyleSheet("background:#111;border-bottom:1px solid #2a2a3e;");
    wallet->setFixedHeight(30);
    auto* wl = new QHBoxLayout(wallet); wl->setContentsMargins(6,0,6,0); wl->setSpacing(4);
    auto mk = [&](const QString& text, QLabel*& lbl, const QString& color) {
        auto* c = new QWidget(this); auto* cl = new QHBoxLayout(c); cl->setContentsMargins(0,0,0,0); cl->setSpacing(2);
        cl->addWidget(new QLabel(text, this));
        lbl = new QLabel("$0.00", this); lbl->setStyleSheet(QString("color:%1;font-weight:700;font-size:11px;").arg(color));
        cl->addWidget(lbl); wl->addWidget(c);
    };
    mk("Bal:", bal_lbl_, "#22c55e"); mk("Eq:", eq_lbl_, "#e5e5e5");
    mk("BP:", bp_lbl_, "#808080"); mk("P&L:", pnl_lbl_, "#ffd700");
    wl->addStretch();
    right_layout->addWidget(wallet);

    auto* right_splitter = new QSplitter(Qt::Vertical, this);
    right_splitter->setHandleWidth(1);
    orderbook_ = new crypto::CryptoOrderBook(this);
    right_splitter->addWidget(orderbook_);
    orderentry_ = new crypto::CryptoOrderEntry(this);
    right_splitter->addWidget(orderentry_);
    right_splitter->setStretchFactor(0, 3); right_splitter->setStretchFactor(1, 2);
    right_layout->addWidget(right_splitter, 1);
    main_splitter->addWidget(right_panel);

    main_splitter->setSizes({160, 600, 290});
    main_splitter->setStretchFactor(0, 0);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setStretchFactor(2, 0);
    root->addWidget(main_splitter, 1);
}

void ExecutionScreen::on_symbol_submit() {
    QString sym = symbol_input_->text().trimmed().toUpper();
    if (!sym.isEmpty()) on_symbol_selected(sym);
}

void ExecutionScreen::on_symbol_selected(const QString& symbol) {
    symbol_input_->setText(symbol);
    orderentry_->set_symbol(symbol);
    watchlist_->set_active_symbol(symbol);
    ws_status_->setText(QString("WS: %1").arg(symbol));
}

void ExecutionScreen::on_mode_toggled() {
    is_paper_mode_ = !is_paper_mode_;
    mode_btn_->setText(is_paper_mode_ ? "PAPER" : "LIVE");
    mode_btn_->setProperty("mode", is_paper_mode_ ? "paper" : "live");
    mode_btn_->style()->unpolish(mode_btn_);
    mode_btn_->style()->polish(mode_btn_);
    ws_status_->setText(is_paper_mode_ ? "Paper Mode" : "LIVE Mode");
}

void ExecutionScreen::refresh_account() {
    HttpClient::instance().get("http://localhost:8150/mt5/account",
        [this](Result<QJsonDocument> r) {
            if (!r.is_ok()) return;
            auto d = r.value().object()["data"].toObject();
            double bal = d["balance"].toDouble();
            double eq = d["equity"].toDouble();
            double bp = d["margin_free"].toDouble();
            double pnl = eq - bal;
            if (bal_lbl_) bal_lbl_->setText(QString("$%1").arg(bal, 0, 'f', 2));
            if (eq_lbl_) eq_lbl_->setText(QString("$%1").arg(eq, 0, 'f', 2));
            if (bp_lbl_) bp_lbl_->setText(QString("$%1").arg(bp, 0, 'f', 0));
            if (pnl_lbl_) {
                pnl_lbl_->setText(QString("%$%1").arg(pnl, 0, 'f', 2));
                pnl_lbl_->setStyleSheet(QString("color:%1;font-weight:700;font-size:11px;")
                    .arg(pnl >= 0 ? "#22c55e" : "#ef4444"));
            }
        }, this);
}

} // namespace fincept::screens
