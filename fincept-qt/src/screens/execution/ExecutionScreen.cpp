#include "screens/execution/ExecutionScreen.h"
#include "screens/equity_trading/AccountManagementDialog.h"
#include "screens/crypto_trading/CryptoBottomPanel.h"
#include "screens/crypto_trading/CryptoChart.h"
#include "ui/charts/PositionLayer.h"
#include "screens/crypto_trading/CryptoCredentials.h"
#include "screens/crypto_trading/CryptoOrderBook.h"
#include "screens/crypto_trading/CryptoOrderEntry.h"
#include "screens/crypto_trading/CryptoTickerBar.h"
#include "screens/crypto_trading/CryptoWatchlist.h"
#include "screens/algo_trading/MT5FleetChartPanel.h"
#include "screens/algo_trading/MT5FleetMultiChartContainer.h"
#include "screens/backtesting/TickReplayWidget.h"
#include "screens/fno/ChainSubTab.h"
#include "screens/crypto_center/CryptoCenterScreen.h"
#include "screens/crypto_center/HoldingsBar.h"
#include "screens/crypto_center/WalletActionConfirmDialog.h"
#include "services/wallet/WalletService.h"
#include "services/wallet/WalletTypes.h"
#include "trading/SmartOrderEngine.h"
#include "trading/ExchangeService.h"
#include "trading/ExchangeSession.h"
#include "trading/ExchangeSessionManager.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/exchanges/kraken/KrakenWsClient.h"
#include "network/http/HttpClient.h"
#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "ui/theme/StyleSheets.h"
#include "ui/theme/Theme.h"

#include <QCompleter>
#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QTcpSocket>
#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QPointer>
#include <QSplitter>
#include <QStringListModel>
#include <QStyle>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

using namespace fincept::trading;
using namespace fincept::screens::crypto;
using namespace fincept::wallet;

static const QString TAG = "ExecutionScreen";

namespace {
QString execution_ws_url(const QString& path, const QUrlQuery& query = {}) {
    QString base = fincept::AppConfig::instance().api_base_url();
    if (base.contains(":8155")) base.replace(":8155", ":8156");
    QUrl url(base);
    url.setScheme(url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("wss") : QStringLiteral("ws"));
    url.setPath(path);
    url.setQuery(query);
    return url.toString();
}
} // namespace

ExecutionScreen::ExecutionScreen(QWidget* parent) : QWidget(parent) {
    LOG_INFO(TAG, "Constructing ExecutionScreen");
    setObjectName("executionScreen");
    setStyleSheet(ui::styles::crypto_trading_styles());
    setup_ui();
    setup_timers();
    init_wallet();
    LOG_INFO(TAG, "ExecutionScreen construction complete");
}

ExecutionScreen::~ExecutionScreen() {
    LOG_INFO(TAG, "Destroying ExecutionScreen");
}

void ExecutionScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (market_info_timer_) market_info_timer_->start();
    if (clock_timer_) clock_timer_->start();
    if (ws_flush_timer_) ws_flush_timer_->start();
    if (stock_refresh_timer_) stock_refresh_timer_->start();
}

void ExecutionScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (ticker_timer_) ticker_timer_->stop();
    if (ob_timer_) ob_timer_->stop();
    if (portfolio_timer_) portfolio_timer_->stop();
    if (watchlist_timer_) watchlist_timer_->stop();
    if (market_info_timer_) market_info_timer_->stop();
    if (live_data_timer_) live_data_timer_->stop();
    if (clock_timer_) clock_timer_->stop();
    if (ws_flush_timer_) ws_flush_timer_->stop();
    if (stock_refresh_timer_) stock_refresh_timer_->stop();
    last_ws_state_ = -1; last_ws_status_label_state_ = -1;
    pending_tickers_.clear(); pending_primary_ticker_ = {}; has_pending_primary_ = false;
    pending_orderbook_ = {}; has_pending_orderbook_ = false; pending_candles_.clear(); pending_trades_.clear();
    candles_fetching_.store(false); live_inflight_.store(0);
}

// ══════════════════════════════════════════════════════════════════════════════
// UI Setup
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionScreen::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // ── COMMAND BAR (34px) ────────────────────────────────────────────────
    auto* cmd_bar = new QWidget(this);
    cmd_bar->setObjectName("cryptoCommandBar");
    cmd_bar->setFixedHeight(34);
    auto* cmd_layout = new QHBoxLayout(cmd_bar);
    cmd_layout->setContentsMargins(8, 0, 8, 0);
    cmd_layout->setSpacing(6);

    // Mode selector — 5 modes
    mode_selector_btn_ = new QPushButton("CRYPTO TRADE");
    mode_selector_btn_->setObjectName("cryptoExchangeBtn");
    mode_selector_btn_->setFixedHeight(22);
    mode_selector_btn_->setCursor(Qt::PointingHandCursor);
    mode_selector_menu_ = new QMenu(mode_selector_btn_);
    mode_selector_menu_->addAction("Crypto Wallet", this, [this]() { on_mode_changed(ExecutionMode::CryptoWallet); });
    mode_selector_menu_->addAction("Crypto Trade", this, [this]() { on_mode_changed(ExecutionMode::CryptoTrade); });
    mode_selector_menu_->addAction("Stocks", this, [this]() { on_mode_changed(ExecutionMode::Stocks); });
    mode_selector_menu_->addAction("MT5", this, [this]() { on_mode_changed(ExecutionMode::MT5); });
    mode_selector_menu_->addAction("Broker API", this, [this]() { on_mode_changed(ExecutionMode::BrokerApi); });
    mode_selector_btn_->setMenu(mode_selector_menu_);
    cmd_layout->addWidget(mode_selector_btn_);
    cmd_layout->addWidget(new QLabel("|"));

    // Exchange button (crypto mode)
    exchange_btn_ = new QPushButton("KRAKEN");
    exchange_btn_->setObjectName("cryptoExchangeBtn");
    exchange_btn_->setFixedHeight(22);
    exchange_btn_->setCursor(Qt::PointingHandCursor);
    exchange_menu_ = new QMenu(exchange_btn_);
    for (const auto& ex : {"kraken", "hyperliquid"})
        exchange_menu_->addAction(ex, this, [this, ex]() { on_exchange_changed(ex); });
    exchange_btn_->setMenu(exchange_menu_);
    cmd_layout->addWidget(exchange_btn_);

    // Symbol input
    symbol_input_ = new QLineEdit("BTC/USDT");
    symbol_input_->setObjectName("cryptoSymbolInput");
    symbol_input_->setFixedWidth(120);
    symbol_input_->setFixedHeight(22);
    auto* completer = new QCompleter(watchlist_symbols_, symbol_input_);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    symbol_input_->setCompleter(completer);
    connect(symbol_input_, &QLineEdit::returnPressed, this, [this]() {
        if (execution_mode_ == ExecutionMode::CryptoTrade)
            on_symbol_selected(symbol_input_->text().trimmed().toUpper());
        else if (execution_mode_ == ExecutionMode::Stocks)
            on_stock_symbol_selected(symbol_input_->text().trimmed().toUpper());
        else
            on_symbol_submit();
    });
    cmd_layout->addWidget(symbol_input_);
    cmd_layout->addWidget(new QLabel("|"));

    // Ticker bar
    ticker_ = new CryptoTickerBar;
    cmd_layout->addWidget(ticker_, 1);

    // Paper/Live mode toggle
    mode_btn_ = new QPushButton("PAPER");
    mode_btn_->setObjectName("cryptoModeBtn");
    mode_btn_->setProperty("mode", "paper");
    mode_btn_->setCheckable(true);
    mode_btn_->setFixedHeight(22);
    mode_btn_->setCursor(Qt::PointingHandCursor);
    cmd_layout->addWidget(mode_btn_);

    // MT5 link
    mt5_link_btn_ = new QPushButton("MT5 LINK");
    mt5_link_btn_->setStyleSheet("QPushButton{background:#111;color:#e5e5e5;border:1px solid #3a3a48;padding:2px 10px;font-weight:700;font-size:10px;border-radius:3px;}");
    mt5_link_btn_->setFixedHeight(22);
    mt5_link_btn_->setCursor(Qt::PointingHandCursor);
    connect(mt5_link_btn_, &QPushButton::clicked, this, &ExecutionScreen::on_mt5_link_clicked);
    cmd_layout->addWidget(mt5_link_btn_);

    // Credentials
    creds_btn_ = new QPushButton("CREDS");
    creds_btn_->setStyleSheet("QPushButton{background:#111;color:#e5e5e5;border:1px solid #3a3a48;padding:2px 10px;font-weight:700;font-size:10px;border-radius:3px;}");
    creds_btn_->setFixedHeight(22);
    creds_btn_->setCursor(Qt::PointingHandCursor);
    connect(creds_btn_, &QPushButton::clicked, this, [this]() { on_mt5_creds_clicked(); });
    cmd_layout->addWidget(creds_btn_);

    // API button
    api_btn_ = new QPushButton("API");
    api_btn_->setObjectName("cryptoApiBtn");
    api_btn_->setFixedHeight(22);
    api_btn_->setCursor(Qt::PointingHandCursor);
    cmd_layout->addWidget(api_btn_);

    // WS status
    ws_status_ = new QLabel("CONNECTING");
    ws_status_->setObjectName("cryptoWsStatus");
    ws_status_->setProperty("ws", "connecting");
    cmd_layout->addWidget(ws_status_);

    ws_transport_ = new QLabel("NATIVE");
    ws_transport_->setObjectName("cryptoWsTransport");
    cmd_layout->addWidget(ws_transport_);

    mt5_status_ = new QLabel("MT5: --");
    mt5_status_->setObjectName("cryptoWsStatus");
    cmd_layout->addWidget(mt5_status_);

    cmd_layout->addStretch();

    clock_label_ = new QLabel("--:--:--");
    clock_label_->setObjectName("cryptoClock");
    cmd_layout->addWidget(clock_label_);

    main_layout->addWidget(cmd_bar);

    // ── MODE STACK ────────────────────────────────────────────────────────
    mode_stack_ = new QStackedWidget(this);

    // Build all mode panels
    build_wallet_panel(nullptr);
    build_stock_panel(nullptr);
    // Crypto Trade, MT5, Broker API share the existing splitter layout
    auto* trade_panel = new QWidget;
    mode_stack_->addWidget(wallet_panel_);      // 0: CryptoWallet
    mode_stack_->addWidget(trade_panel);         // 1: CryptoTrade
    mode_stack_->addWidget(stock_panel_);        // 2: Stocks

    // Build the shared trading splitter inside trade_panel
    {
        auto* trade_layout = new QVBoxLayout(trade_panel);
        trade_layout->setContentsMargins(0,0,0,0);
        trade_layout->setSpacing(0);

        auto* main_splitter = new QSplitter(Qt::Horizontal);
        main_splitter->setObjectName("cryptoMainSplitter");
        main_splitter->setHandleWidth(1);

        // LEFT: Watchlist
        watchlist_ = new CryptoWatchlist;
        watchlist_->set_symbols(watchlist_symbols_);
        watchlist_->set_active_symbol(selected_symbol_);
        connect(watchlist_, &CryptoWatchlist::symbol_selected, this, &ExecutionScreen::on_symbol_selected);
        connect(watchlist_, &CryptoWatchlist::search_requested, this, &ExecutionScreen::on_search_requested);
        main_splitter->addWidget(watchlist_);

        // CENTER: Chart + Script bar + Bottom Panel
        center_splitter_ = new QSplitter(Qt::Vertical);
        center_splitter_->setObjectName("cryptoCenterSplitter");
        center_splitter_->setHandleWidth(1);

        chart_container_ = new QWidget;
        auto* chart_layout = new QVBoxLayout(chart_container_);
        chart_layout->setContentsMargins(0,0,0,0);
        chart_layout->setSpacing(0);
        // ── Chart header bar with Multi-Chart / Replay / Options toggles ──
        auto* chart_header = new QWidget;
        chart_header->setObjectName("cryptoChartHeader");
        auto* hdr_layout = new QHBoxLayout(chart_header);
        hdr_layout->setContentsMargins(10, 4, 10, 4);
        hdr_layout->setSpacing(2);

        multi_chart_btn_ = new QPushButton("M-CHART");
        multi_chart_btn_->setObjectName("cryptoTfBtn");
        multi_chart_btn_->setCursor(Qt::PointingHandCursor);
        connect(multi_chart_btn_, &QPushButton::clicked, this, &ExecutionScreen::toggle_multi_chart);
        hdr_layout->addWidget(multi_chart_btn_);

        replay_btn_ = new QPushButton("REPLAY");
        replay_btn_->setObjectName("cryptoTfBtn");
        replay_btn_->setCursor(Qt::PointingHandCursor);
        connect(replay_btn_, &QPushButton::clicked, this, &ExecutionScreen::toggle_replay);
        hdr_layout->addWidget(replay_btn_);

        options_btn_ = new QPushButton("OPTIONS");
        options_btn_->setObjectName("cryptoTfBtn");
        options_btn_->setCursor(Qt::PointingHandCursor);
        connect(options_btn_, &QPushButton::clicked, this, &ExecutionScreen::toggle_options);
        hdr_layout->addWidget(options_btn_);

        hdr_layout->addStretch();
        chart_layout->addWidget(chart_header);

        crypto_chart_ = new CryptoChart;
        connect(crypto_chart_, &CryptoChart::position_sl_tp_changed,
                this, [](const QString&, double, double) {
            // TODO: wire to broker API for SL/TP update
        });
        crypto_chart_->hide();
        chart_layout->addWidget(crypto_chart_);
        mt5_chart_ = new MT5FleetChartPanel;
        mt5_chart_->hide();
        chart_layout->addWidget(mt5_chart_);
        current_chart_ = crypto_chart_;
        crypto_chart_->show();
        center_splitter_->addWidget(chart_container_);

        // Script status bar
        auto* script_bar = new QWidget;
        script_bar->setStyleSheet("background:#0f1720;border-top:1px solid #253244;border-bottom:1px solid #253244;");
        script_bar->setFixedHeight(28);
        auto* script_layout = new QHBoxLayout(script_bar);
        script_layout->setContentsMargins(8,0,8,0);
        script_layout->setSpacing(8);
        auto* script_title = new QLabel("DEPLOYED");
        script_title->setStyleSheet("color:#00D66F;font-size:10px;font-weight:800;letter-spacing:1px;");
        script_layout->addWidget(script_title);
        script_status_ = new QLabel("Waiting for scripts...");
        script_status_->setStyleSheet("color:#cbd5e1;font-size:10px;font-weight:600;");
        script_status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        script_layout->addWidget(script_status_, 1);
        center_splitter_->addWidget(script_bar);

        bottom_ = new CryptoBottomPanel;
        center_splitter_->addWidget(bottom_);
        center_splitter_->setStretchFactor(0, 5);
        center_splitter_->setStretchFactor(1, 0);
        center_splitter_->setStretchFactor(2, 2);
        main_splitter->addWidget(center_splitter_);

        // RIGHT: Wallet info + OrderBook + OrderEntry
        auto* right_panel = new QWidget;
        auto* right_layout = new QVBoxLayout(right_panel);
        right_layout->setContentsMargins(0,0,0,0); right_layout->setSpacing(0);

        auto* wallet = new QWidget;
        wallet->setStyleSheet("background:#111;border-bottom:1px solid #2a2a3e;");
        wallet->setFixedHeight(30);
        auto* wl = new QHBoxLayout(wallet); wl->setContentsMargins(6,0,6,0); wl->setSpacing(4);
        auto mk = [&](const QString& text, QLabel*& lbl, const QString& color) {
            auto* c = new QWidget; auto* cl = new QHBoxLayout(c); cl->setContentsMargins(0,0,0,0); cl->setSpacing(2);
            cl->addWidget(new QLabel(text));
            lbl = new QLabel("$0.00"); lbl->setStyleSheet(QString("color:%1;font-weight:700;font-size:11px;").arg(color));
            cl->addWidget(lbl); wl->addWidget(c);
        };
        mk("Bal:", bal_lbl_, "#22c55e"); mk("Eq:", eq_lbl_, "#e5e5e5");
        mk("BP:", bp_lbl_, "#808080"); mk("P&L:", pnl_lbl_, "#ffd700");
        wl->addStretch();
        right_layout->addWidget(wallet);

        auto* right_splitter = new QSplitter(Qt::Vertical);
        right_splitter->setHandleWidth(1);
        right_splitter->setObjectName("cryptoRightSplitter");
        orderbook_ = new CryptoOrderBook;
        right_splitter->addWidget(orderbook_);
        orderentry_ = new CryptoOrderEntry;
        right_splitter->addWidget(orderentry_);
        right_splitter->setStretchFactor(0, 3); right_splitter->setStretchFactor(1, 2);
        right_layout->addWidget(right_splitter, 1);
        main_splitter->addWidget(right_panel);

        main_splitter->setSizes({160, 600, 290});
        main_splitter->setStretchFactor(0, 0);
        main_splitter->setStretchFactor(1, 1);
        main_splitter->setStretchFactor(2, 0);
        trade_layout->addWidget(main_splitter, 1);
    }

    // Add MT5 panel (3) and Broker API panel (4) — same layout, different data
    // Use a QStackedWidget workaround: create a wrapper widget
    auto* mt5_wrapper = new QWidget;
    auto* mt5_l = new QVBoxLayout(mt5_wrapper);
    mt5_l->setContentsMargins(0,0,0,0);
    // Reuse trade_panel's content by taking its layout and putting it in a new wrapper
    // Actually, just use a label-based placeholder that says "Switch to MT5 mode"
    auto* placeholder = new QLabel("Select mode: CRYPTO TRADE or MT5 from the dropdown");
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color:#404040;font-size:14px;");
    mt5_l->addWidget(placeholder);
    mode_stack_->addWidget(mt5_wrapper);   // 3: MT5

    auto* broker_wrapper = new QWidget;
    auto* broker_l = new QVBoxLayout(broker_wrapper);
    auto* broker_placeholder = new QLabel("Enter broker API credentials via the API button");
    broker_placeholder->setAlignment(Qt::AlignCenter);
    broker_placeholder->setStyleSheet("color:#404040;font-size:14px;");
    broker_l->addWidget(broker_placeholder);
    mode_stack_->addWidget(broker_wrapper);  // 4: Broker API

    main_layout->addWidget(mode_stack_, 1);

    // Start in Crypto Trade mode
    mode_stack_->setCurrentIndex(1);

    // ── Signal Connections ────────────────────────────────────────────────
    connect(mode_btn_, &QPushButton::clicked, this, &ExecutionScreen::on_mode_toggled);
    connect(api_btn_, &QPushButton::clicked, this, &ExecutionScreen::on_api_clicked);
    connect(orderentry_, &CryptoOrderEntry::order_submitted, this, &ExecutionScreen::on_order_submitted);
    connect(orderentry_, &CryptoOrderEntry::leverage_changed, this, &ExecutionScreen::async_set_leverage);
    connect(orderentry_, &CryptoOrderEntry::margin_mode_changed, this, &ExecutionScreen::async_set_margin_mode);
    connect(orderbook_, &CryptoOrderBook::price_clicked, this, &ExecutionScreen::on_ob_price_clicked);
    connect(bottom_, &CryptoBottomPanel::cancel_order_requested, this, &ExecutionScreen::on_cancel_order);

    // ── MT5 Market Data WebSocket ──
    market_ws_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(market_ws_, &QWebSocket::textMessageReceived, this, [this](const QString& msg) {
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
            if (data.contains("price"))
                orderentry_->set_current_price(data["price"].toDouble());
        }
    });
    auto* ws_timer = new QTimer(this);
    connect(ws_timer, &QTimer::timeout, this, &ExecutionScreen::open_market_data_ws);
    ws_timer->start(5000);
    open_market_data_ws();

    // Start in Crypto Trade mode
    mode_stack_->setCurrentIndex(1);
}

// ══════════════════════════════════════════════════════════════════════════════
// Wallet Panel — embeds the full Crypto Center (all 7 tabs + HoldingsBar)
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionScreen::build_wallet_panel(QWidget*) {
    wallet_panel_ = new QWidget;
    auto* layout = new QVBoxLayout(wallet_panel_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Embed the full CryptoCenterScreen as a child — gives us all 7 tabs:
    // HOME (holdings, swap, treasury, buyback, supply chart, fee discounts)
    // TRADE (swap panel)
    // ACTIVITY (wallet activity)
    // SETTINGS (Helius key, slippage, mode toggle)
    // STAKE (veFNCPT lock, active locks, tier)
    // MARKETS (prediction markets)
    // ROADMAP (buyback & burn dashboard)
    auto* crypto_center = new CryptoCenterScreen(wallet_panel_);
    layout->addWidget(crypto_center, 1);
}

void ExecutionScreen::init_wallet() {
    // Wallet is managed entirely by CryptoCenterScreen's internal WalletService
    // connections — no additional initialization needed.
}

// ══════════════════════════════════════════════════════════════════════════════
// Stock Trading Panel
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionScreen::build_stock_panel(QWidget*) {
    stock_panel_ = new QWidget;
    auto* layout = new QHBoxLayout(stock_panel_);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);

    // LEFT: Stock watchlist
    auto* left_panel = new QWidget;
    left_panel->setFixedWidth(200);
    auto* left_l = new QVBoxLayout(left_panel);
    left_l->setContentsMargins(0,0,0,0);
    left_l->setSpacing(0);

    stock_exchange_btn_ = new QPushButton("US Stocks");
    stock_exchange_btn_->setStyleSheet("QPushButton{background:#111;color:#e5e5e5;border:1px solid #3a3a48;padding:6px 12px;font-weight:700;font-size:10px;border-radius:0px;}QPushButton:hover{background:#1a1a2e;}");
    stock_exchange_btn_->setFixedHeight(30);
    auto* ex_menu = new QMenu(stock_exchange_btn_);
    ex_menu->addAction("US Stocks", this, [this]() { on_stock_exchange_changed("US"); });
    ex_menu->addAction("Indian Stocks", this, [this]() { on_stock_exchange_changed("IN"); });
    ex_menu->addAction("European Stocks", this, [this]() { on_stock_exchange_changed("EU"); });
    ex_menu->addAction("Crypto", this, [this]() { on_stock_exchange_changed("CRYPTO"); });
    stock_exchange_btn_->setMenu(ex_menu);
    left_l->addWidget(stock_exchange_btn_);

    stock_symbol_input_ = new QLineEdit("AAPL");
    stock_symbol_input_->setStyleSheet("background:#0a0a0a;color:#e5e5e5;border:1px solid #1a1a2e;padding:4px 8px;font-size:11px;");
    stock_symbol_input_->setPlaceholderText("Search symbol...");
    connect(stock_symbol_input_, &QLineEdit::returnPressed, this, [this]() {
        on_stock_symbol_selected(stock_symbol_input_->text().trimmed().toUpper());
    });
    left_l->addWidget(stock_symbol_input_);

    stock_watchlist_ = new QTreeWidget;
    stock_watchlist_->setHeaderLabels({"Symbol", "Price", "Chg%"});
    stock_watchlist_->setAlternatingRowColors(true);
    stock_watchlist_->setRootIsDecorated(false);
    stock_watchlist_->setStyleSheet("QTreeWidget{background:#0a0a0a;color:#e5e5e5;border:none;font-size:11px;}QTreeWidget::item{padding:3px 6px;}QTreeWidget::item:hover{background:#1a1a2e;}QHeaderView::section{background:#111;color:#808080;border:none;padding:3px 6px;font-weight:700;font-size:10px;}");
    stock_watchlist_->setColumnWidth(0, 80);
    stock_watchlist_->setColumnWidth(1, 60);
    stock_watchlist_->setColumnWidth(2, 50);
    for (const auto& sym : stock_symbols_) {
        auto* item = new QTreeWidgetItem(stock_watchlist_);
        item->setText(0, sym);
        item->setText(1, "--");
        item->setText(2, "--");
    }
    connect(stock_watchlist_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        on_stock_symbol_selected(item->text(0));
    });
    left_l->addWidget(stock_watchlist_, 1);

    layout->addWidget(left_panel);

    // CENTER: Stock info
    auto* center = new QWidget;
    auto* center_l = new QVBoxLayout(center);
    center_l->setContentsMargins(12,12,12,12);
    center_l->setSpacing(8);

    auto* stock_header = new QLabel("AAPL — Apple Inc.");
    stock_header->setStyleSheet("color:#e5e5e5;font-size:16px;font-weight:800;");
    center_l->addWidget(stock_header);

    auto* price_row = new QWidget;
    auto* pr_l = new QHBoxLayout(price_row); pr_l->setSpacing(12);
    stock_price_lbl_ = new QLabel("$198.45");
    stock_price_lbl_->setStyleSheet("color:#e5e5e5;font-size:28px;font-weight:700;");
    pr_l->addWidget(stock_price_lbl_);
    stock_change_lbl_ = new QLabel("+2.34 (+1.19%)");
    stock_change_lbl_->setStyleSheet("color:#22c55e;font-size:14px;font-weight:600;");
    pr_l->addWidget(stock_change_lbl_);
    pr_l->addStretch();
    center_l->addWidget(price_row);

    auto* stats_grid = new QWidget;
    auto* stats_l = new QHBoxLayout(stats_grid); stats_l->setSpacing(20);
    auto mk_stat = [](const QString& label, QLabel*& val, const QString& default_val) {
        auto* w = new QWidget;
        auto* l = new QVBoxLayout(w); l->setSpacing(2);
        l->addWidget(new QLabel(label));
        val = new QLabel(default_val);
        val->setStyleSheet("color:#e5e5e5;font-size:13px;font-weight:600;");
        l->addWidget(val);
        return w;
    };
    stats_l->addWidget(mk_stat("Volume", stock_volume_lbl_, "45.2M"));
    stats_l->addWidget(mk_stat("High", stock_high_lbl_, "$199.87"));
    stats_l->addWidget(mk_stat("Low", stock_low_lbl_, "$197.23"));
    stats_l->addStretch();
    center_l->addWidget(stats_grid);

    auto* chart_placeholder = new QLabel("Stock chart area — powered by yfinance data from VPS");
    chart_placeholder->setAlignment(Qt::AlignCenter);
    chart_placeholder->setStyleSheet("color:#404040;font-size:12px;background:#0a0a0a;border:1px solid #1a1a2e;border-radius:4px;padding:40px;");
    center_l->addWidget(chart_placeholder, 1);

    auto* order_row = new QWidget;
    auto* or_l = new QHBoxLayout(order_row);
    auto* buy_btn = new QPushButton("BUY");
    buy_btn->setStyleSheet("QPushButton{background:#22c55e;color:#080808;border:none;padding:8px 24px;font-weight:700;font-size:12px;border-radius:4px;}QPushButton:hover{background:#16a34a;}");
    auto* sell_btn = new QPushButton("SELL");
    sell_btn->setStyleSheet("QPushButton{background:#ef4444;color:#fff;border:none;padding:8px 24px;font-weight:700;font-size:12px;border-radius:4px;}QPushButton:hover{background:#dc2626;}");
    auto* qty_input = new QLineEdit("10");
    qty_input->setFixedWidth(60);
    qty_input->setStyleSheet("background:#0a0a0a;color:#e5e5e5;border:1px solid #1a1a2e;padding:6px 8px;font-size:12px;");
    or_l->addWidget(new QLabel("Qty:"));
    or_l->addWidget(qty_input);
    or_l->addWidget(buy_btn);
    or_l->addWidget(sell_btn);
    connect(buy_btn, &QPushButton::clicked, this, [this, qty_input]() {
        on_stock_order_submitted("BUY", "MARKET", qty_input->text().toDouble(), 0);
    });
    connect(sell_btn, &QPushButton::clicked, this, [this, qty_input]() {
        on_stock_order_submitted("SELL", "MARKET", qty_input->text().toDouble(), 0);
    });
    or_l->addStretch();
    center_l->addWidget(order_row);

    layout->addWidget(center, 2);

    // RIGHT: Order book placeholder
    auto* right = new QWidget;
    right->setFixedWidth(200);
    auto* right_l = new QVBoxLayout(right);
    right_l->setContentsMargins(0,0,0,0);
    auto* ob_header = new QLabel("Order Book");
    ob_header->setStyleSheet("background:#111;color:#808080;padding:6px 8px;font-weight:700;font-size:10px;border-bottom:1px solid #1a1a2e;");
    right_l->addWidget(ob_header);
    auto* ob_list = new QTreeWidget;
    ob_list->setHeaderLabels({"Level", "Bid", "Ask"});
    ob_list->setAlternatingRowColors(true);
    ob_list->setRootIsDecorated(false);
    ob_list->setStyleSheet("QTreeWidget{background:#0a0a0a;color:#e5e5e5;border:none;font-size:11px;}QTreeWidget::item{padding:2px 6px;}QHeaderView::section{background:#111;color:#808080;border:none;padding:2px 6px;font-weight:700;font-size:10px;}");
    for (int i = 1; i <= 10; i++) {
        auto* item = new QTreeWidgetItem(ob_list);
        item->setText(0, QString::number(i));
        item->setText(1, QString::number(198.45 - i * 0.10, 'f', 2));
        item->setText(2, QString::number(198.45 + i * 0.10, 'f', 2));
    }
    right_l->addWidget(ob_list, 1);
    layout->addWidget(right);
}

void ExecutionScreen::on_stock_exchange_changed(const QString& exchange) {
    stock_exchange_ = exchange;
    stock_exchange_btn_->setText(exchange == "US" ? "US Stocks" : exchange == "IN" ? "Indian Stocks" : exchange == "EU" ? "European Stocks" : "Crypto");
    ws_status_->setText(QString("Exchange: %1").arg(exchange));
}

void ExecutionScreen::on_stock_symbol_selected(const QString& symbol) {
    selected_stock_ = symbol;
    stock_symbol_input_->setText(symbol);
    // Highlight in watchlist
    for (int i = 0; i < stock_watchlist_->topLevelItemCount(); i++) {
        auto* item = stock_watchlist_->topLevelItem(i);
        item->setBackground(0, item->text(0) == symbol ? QColor("#1a3a1a") : QColor());
    }
    refresh_stock_quotes();
    ws_status_->setText(QString("Stock: %1").arg(symbol));
}

void ExecutionScreen::on_stock_order_submitted(const QString& side, const QString& order_type, double qty, double price) {
    LOG_INFO(TAG, QString("Stock order: %1 %2 %3 qty=%4").arg(side, order_type, selected_stock_).arg(qty));
    HttpClient::instance().post("/execute/trade", QJsonObject{
        {"symbol", selected_stock_}, {"side", side}, {"type", order_type},
        {"quantity", qty}, {"price", price}
    }, [this, side](Result<QJsonDocument> r) {
        if (r.is_ok()) ws_status_->setText(QString("%1 order placed").arg(side));
        else ws_status_->setText("Order failed: " + QString::fromStdString(r.error()));
    }, this);
}

void ExecutionScreen::refresh_stock_quotes() {
    HttpClient::instance().get("/market/quotes?symbols=" + stock_symbols_.join(','),
        [this](Result<QJsonDocument> r) {
            if (!r.is_ok()) return;
            auto data = r.value().object()["data"].toArray();
            for (const auto& v : data) {
                auto q = v.toObject();
                QString sym = q["symbol"].toString();
                double price = q["price"].toDouble();
                double change = q["change"].toDouble();
                double change_pct = q["change_pct"].toDouble();
                for (int i = 0; i < stock_watchlist_->topLevelItemCount(); i++) {
                    auto* item = stock_watchlist_->topLevelItem(i);
                    if (item->text(0) == sym) {
                        item->setText(1, QString("$%1").arg(price, 0, 'f', 2));
                        item->setText(2, QString("%1%").arg(change_pct, 0, 'f', 2));
                        item->setForeground(2, change >= 0 ? QColor("#22c55e") : QColor("#ef4444"));
                        break;
                    }
                }
                if (sym == selected_stock_) {
                    stock_price_lbl_->setText(QString("$%1").arg(price, 0, 'f', 2));
                    stock_change_lbl_->setText(QString("%1 (%2%)").arg(change, 0, 'f', 2).arg(change_pct, 0, 'f', 2));
                    stock_change_lbl_->setStyleSheet(QString("color:%1;font-size:14px;font-weight:600;").arg(change >= 0 ? "#22c55e" : "#ef4444"));
                    stock_volume_lbl_->setText(q["volume"].toString());
                    stock_high_lbl_->setText(QString("$%1").arg(q["high"].toDouble(), 0, 'f', 2));
                    stock_low_lbl_->setText(QString("$%1").arg(q["low"].toDouble(), 0, 'f', 2));
                }
            }
        }, this);
}

// ══════════════════════════════════════════════════════════════════════════════
// Timers
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionScreen::setup_timers() {
    ticker_timer_ = new QTimer(this);
    connect(ticker_timer_, &QTimer::timeout, this, &ExecutionScreen::refresh_ticker);
    ticker_timer_->setInterval(3000);

    ob_timer_ = new QTimer(this);
    connect(ob_timer_, &QTimer::timeout, this, &ExecutionScreen::refresh_orderbook);
    ob_timer_->setInterval(3000);

    portfolio_timer_ = new QTimer(this);
    connect(portfolio_timer_, &QTimer::timeout, this, &ExecutionScreen::refresh_portfolio);
    portfolio_timer_->setInterval(1500);

    watchlist_timer_ = new QTimer(this);
    connect(watchlist_timer_, &QTimer::timeout, this, &ExecutionScreen::refresh_watchlist);
    watchlist_timer_->setInterval(10000);

    market_info_timer_ = new QTimer(this);
    connect(market_info_timer_, &QTimer::timeout, this, &ExecutionScreen::refresh_market_info);
    market_info_timer_->setInterval(30000);

    live_data_timer_ = new QTimer(this);
    connect(live_data_timer_, &QTimer::timeout, this, &ExecutionScreen::refresh_live_data);

    clock_timer_ = new QTimer(this);
    connect(clock_timer_, &QTimer::timeout, this, &ExecutionScreen::update_clock);
    clock_timer_->setInterval(1000);

    ws_flush_timer_ = new QTimer(this);
    connect(ws_flush_timer_, &QTimer::timeout, this, &ExecutionScreen::flush_ws_updates);
    ws_flush_timer_->setInterval(100);

    stock_refresh_timer_ = new QTimer(this);
    connect(stock_refresh_timer_, &QTimer::timeout, this, &ExecutionScreen::refresh_stock_quotes);
    stock_refresh_timer_->setInterval(5000);

    // MT5 timers
    auto* acct_timer = new QTimer(this);
    connect(acct_timer, &QTimer::timeout, this, &ExecutionScreen::refresh_account);
    acct_timer->start(3000);
    QTimer::singleShot(1000, this, &ExecutionScreen::refresh_account);

    auto* mt5_timer = new QTimer(this);
    connect(mt5_timer, &QTimer::timeout, this, &ExecutionScreen::refresh_mt5_link);
    mt5_timer->start(3000);
    QTimer::singleShot(500, this, &ExecutionScreen::refresh_mt5_link);

    auto* scripts_timer = new QTimer(this);
    connect(scripts_timer, &QTimer::timeout, this, &ExecutionScreen::refresh_deployed_scripts);
    scripts_timer->start(4000);
    QTimer::singleShot(700, this, &ExecutionScreen::refresh_deployed_scripts);

    // Defer crypto init
    QTimer::singleShot(0, this, [this]() {
        init_exchange();
        load_portfolio();
        auto* es = &ExchangeService::instance();
        auto run_initial_fetches = [this]() {
            if (startup_fetches_done_) return;
            startup_fetches_done_ = true;
            async_fetch_candles(selected_symbol_, "1h");
        };
        if (es->wait_for_daemon_ready(0)) {
            run_initial_fetches();
        } else {
            auto conn = std::make_shared<QMetaObject::Connection>();
            *conn = connect(es, &ExchangeService::daemon_ready, this, [conn, run_initial_fetches]() {
                QObject::disconnect(*conn); run_initial_fetches(); });
            QTimer::singleShot(8000, this, [this, conn, run_initial_fetches]() {
                if (startup_fetches_done_) return;
                QObject::disconnect(*conn); run_initial_fetches(); });
        }
    });
}

void ExecutionScreen::update_clock() {
    clock_label_->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
    const int connected = ExchangeService::instance().is_ws_connected() ? 1 : 0;
    if (connected == last_ws_status_label_state_) return;
    last_ws_status_label_state_ = connected;
    ws_status_->setText(connected ? "LIVE" : "OFFLINE");
    ws_status_->setProperty("ws", connected ? "live" : "offline");
    ws_status_->style()->unpolish(ws_status_);
    ws_status_->style()->polish(ws_status_);
}

// ══════════════════════════════════════════════════════════════════════════════
// Mode Switching
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionScreen::on_mode_changed(ExecutionMode mode) {
    execution_mode_ = mode;
    switch (mode) {
        case ExecutionMode::CryptoWallet:
            mode_selector_btn_->setText("CRYPTO WALLET");
            mode_stack_->setCurrentIndex(0);
            exchange_btn_->hide();
            ticker_->hide();
            watchlist_->hide();
            break;
        case ExecutionMode::CryptoTrade:
            mode_selector_btn_->setText("CRYPTO TRADE");
            mode_stack_->setCurrentIndex(1);
            exchange_btn_->show();
            ticker_->show();
            watchlist_->show();
            watchlist_->set_symbols(watchlist_symbols_);
            symbol_input_->setText(selected_symbol_);
            break;
        case ExecutionMode::Stocks:
            mode_selector_btn_->setText("STOCKS");
            mode_stack_->setCurrentIndex(1);  // Reuse Crypto Trade layout
            exchange_btn_->hide();
            ticker_->show();
            watchlist_->show();
            watchlist_->set_symbols(stock_symbols_);
            symbol_input_->setText("AAPL");
            selected_symbol_ = "AAPL";
            on_symbol_selected("AAPL");
            refresh_stock_quotes();
            break;
        case ExecutionMode::MT5:
            mode_selector_btn_->setText("MT5");
            mode_stack_->setCurrentIndex(3);
            exchange_btn_->hide();
            ticker_->show();
            watchlist_->show();
            watchlist_->set_symbols(mt5_symbols_);
            symbol_input_->setText("XAUUSD");
            break;
        case ExecutionMode::BrokerApi:
            mode_selector_btn_->setText("BROKER API");
            mode_stack_->setCurrentIndex(4);
            exchange_btn_->hide();
            ticker_->show();
            watchlist_->show();
            break;
    }
    swap_chart_for_mode(mode);
    ws_status_->setText(QString("Mode: %1").arg(mode_selector_btn_->text()));
}

void ExecutionScreen::swap_chart_for_mode(ExecutionMode mode) {
    if (!crypto_chart_ || !mt5_chart_) return;
    switch (mode) {
        case ExecutionMode::CryptoTrade:
        case ExecutionMode::CryptoWallet:
            crypto_chart_->show();
            mt5_chart_->hide();
            current_chart_ = crypto_chart_;
            break;
        case ExecutionMode::MT5:
            crypto_chart_->hide();
            mt5_chart_->show();
            current_chart_ = mt5_chart_;
            break;
        case ExecutionMode::Stocks:
        case ExecutionMode::BrokerApi:
            crypto_chart_->show();
            mt5_chart_->hide();
            current_chart_ = crypto_chart_;
            break;
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// Crypto Exchange (from merged CryptoTradingScreen)
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionScreen::on_exchange_changed(const QString& exchange) {
    if (exchange == exchange_id_) return;
    LOG_INFO(TAG, QString("Exchange changed: %1 → %2").arg(exchange_id_, exchange));
    exchange_id_ = exchange;
    exchange_btn_->setText(exchange.toUpper());
    ws_transport_->setText(exchange == "kraken" ? "NATIVE" : "DAEMON");
    auto& es = ExchangeService::instance();
    hub_unsubscribe_topics();
    pending_tickers_.clear(); pending_orderbook_ = {}; has_pending_orderbook_ = false;
    pending_primary_ticker_ = {}; has_pending_primary_ = false; pending_candles_.clear(); pending_trades_.clear();
    last_ws_state_ = -1; candles_fetching_.store(false); live_inflight_.store(0);
    es.set_exchange(exchange_id_);
    initialized_ = false;
    init_exchange();
    load_portfolio();
    async_fetch_candles(selected_symbol_, "1h");
    refresh_market_info();
    ScreenStateManager::instance().notify_changed(this);
}

void ExecutionScreen::on_symbol_selected(const QString& symbol) {
    if (symbol.isEmpty() || symbol == selected_symbol_) return;
    switch_symbol(symbol);
    ScreenStateManager::instance().notify_changed(this);
    if (link_group_ != SymbolGroup::None) {
        SymbolRef ref; ref.symbol = symbol; ref.asset_class = QStringLiteral("crypto"); ref.exchange = exchange_id_;
        SymbolContext::instance().set_group_symbol(link_group_, ref, this);
    }
}

void ExecutionScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (!ref.is_valid() || ref.asset_class != QStringLiteral("crypto")) return;
    if (ref.symbol == selected_symbol_) return;
    switch_symbol(ref.symbol);
}

SymbolRef ExecutionScreen::current_symbol() const {
    if (selected_symbol_.isEmpty()) return {};
    SymbolRef r; r.symbol = selected_symbol_; r.asset_class = QStringLiteral("crypto"); r.exchange = exchange_id_;
    return r;
}

void ExecutionScreen::switch_symbol(const QString& symbol) {
    LOG_INFO(TAG, QString("Symbol changed: %1 → %2").arg(selected_symbol_, symbol));
    auto& es = ExchangeService::instance();
    es.unwatch_symbol(selected_symbol_, portfolio_id_);
    selected_symbol_ = symbol; symbol_input_->setText(symbol);
    ticker_->set_symbol(symbol); orderentry_->set_symbol(symbol); watchlist_->set_active_symbol(symbol);
    has_pending_primary_ = false; pending_primary_ticker_ = {};
    has_pending_orderbook_ = false; pending_orderbook_ = {};
    pending_candles_.clear(); pending_trades_.clear(); market_info_cache_ = {};
    es.watch_symbol(selected_symbol_, portfolio_id_); es.set_ws_primary_symbol(symbol);
    hub_subscribe_topics();
    async_fetch_candles(selected_symbol_, "1h");
}

void ExecutionScreen::on_mode_toggled() {
    const bool is_live = mode_btn_->isChecked();
    trading_mode_ = is_live ? TradingMode::Live : TradingMode::Paper;
    mode_btn_->setText(is_live ? "LIVE" : "PAPER");
    mode_btn_->setProperty("mode", is_live ? "live" : "paper");
    mode_btn_->style()->unpolish(mode_btn_); mode_btn_->style()->polish(mode_btn_);
    orderentry_->set_mode(!is_live); bottom_->set_mode(!is_live);
    if (is_live) { live_data_timer_->start(5000); refresh_live_data(); }
    else { live_data_timer_->stop(); refresh_portfolio(); }
}

void ExecutionScreen::on_api_clicked() {
    if (execution_mode_ == ExecutionMode::CryptoTrade) {
        auto* dlg = new CryptoCredentials(exchange_id_, this);
        connect(dlg, &CryptoCredentials::credentials_saved, this,
                [this](const QString& key, const QString& secret, const QString& pw) {
                    ExchangeCredentials creds; creds.api_key = key; creds.secret = secret; creds.password = pw;
                    ExchangeService::instance().set_credentials(creds);
                });
        dlg->exec(); dlg->deleteLater();
    } else {
        auto* dlg = new equity::AccountManagementDialog(this);
        connect(dlg, &equity::AccountManagementDialog::credentials_saved, this,
                [this](const QString& account_id) { ws_status_->setText("Account: " + account_id.left(12)); });
        dlg->exec(); dlg->deleteLater();
    }
}

void ExecutionScreen::on_order_submitted(const QString& side, const QString& order_type,
                                          double qty, double price, double stop_price, double sl, double tp) {
    if (execution_mode_ == ExecutionMode::MT5 || execution_mode_ == ExecutionMode::BrokerApi) {
        if (is_paper_mode_) { ws_status_->setText("Paper mode: order simulated"); return; }
        if (!mt5_connected_) { ws_status_->setText("MT5 not linked"); refresh_mt5_link(); return; }
        const QString sym = symbol_input_->text().trimmed().toUpper();
        int magic = 831846;
        double volume = qty;
        QJsonObject body;
        body["action"] = "market";
        body["symbol"] = sym;
        body["side"] = side;
        body["volume"] = volume;
        body["magic"] = magic;
        if (sl > 0) body["sl"] = sl;
        if (tp > 0) body["tp"] = tp;
        ws_status_->setText("Submitting MT5 order...");

        // Try local MT5 worker first (127.0.0.1:5570), fall back to VPS
        QTcpSocket* local_check = new QTcpSocket(this);
        local_check->connectToHost("127.0.0.1", 5570);
        if (local_check->waitForConnected(500)) {
            QByteArray order_data = QJsonDocument(body).toJson(QJsonDocument::Compact);
            order_data.append('\n');
            local_check->write(order_data);
            local_check->waitForBytesWritten(500);
            local_check->waitForReadyRead(5000);
            QByteArray resp = local_check->readAll();
            local_check->disconnectFromHost();
            local_check->deleteLater();
            QJsonDocument doc = QJsonDocument::fromJson(resp);
            if (!doc.isNull()) {
                auto obj = doc.object();
                int rc = obj["retcode"].toInt();
                if (rc == 10009) { ws_status_->setText("FILLED (local)"); }
                else { ws_status_->setText("Order: " + obj["comment"].toString()); }
                return;
            }
        } else {
            local_check->deleteLater();
        }
        // Fallback to VPS
        HttpClient::instance().post("/mt5/worker/order", body, [this, sym](Result<QJsonDocument> r) {
            if (!r.is_ok()) { ws_status_->setText("Order failed: " + QString::fromStdString(r.error())); return; }
            auto obj = r.value().object();
            int rc = obj["data"].toObject()["retcode"].toInt();
            if (rc == 10009) { ws_status_->setText("FILLED (VPS)"); }
            else { ws_status_->setText("Order: " + obj["data"].toObject()["comment"].toString()); }
        });
        return;
    }

    LOG_INFO(TAG, QString("Order submit: %1 %2 qty=%3 price=%4").arg(side, order_type).arg(qty).arg(price));
    try {
        if (trading_mode_ == TradingMode::Paper) {
            auto ticker = ExchangeService::instance().get_cached_price(selected_symbol_);
            std::optional<double> price_opt;
            if (order_type == "market") price_opt = ticker.last > 0 ? ticker.last : 1000.0;
            else if (price > 0) price_opt = price;
            std::optional<double> stop_opt = stop_price > 0 ? std::optional(stop_price) : std::nullopt;
            auto order = pt_place_order(portfolio_id_, selected_symbol_, side, order_type, qty, price_opt, stop_opt);
            if (order_type == "market") {
                double fill = ticker.last > 0 ? ticker.last : price_opt.value_or(1000.0);
                pt_fill_order(order.id, fill);
            } else {
                OrderMatcher::instance().add_order(order);
                if (sl > 0 || tp > 0) OrderMatcher::instance().set_sl_tp(portfolio_id_, selected_symbol_, order.id, sl, tp);
            }
            refresh_portfolio();
        } else {
            QPointer<ExecutionScreen> self = this;
            (void)QtConcurrent::run([self, side, order_type, qty, price]() {
                if (!self) return;
                ExchangeService::instance().place_exchange_order(self->selected_symbol_, side, order_type, qty, price);
                QMetaObject::invokeMethod(self, [self]() { if (self) self->refresh_live_data(); }, Qt::QueuedConnection);
            });
        }
    } catch (const std::exception& e) { LOG_ERROR(TAG, QString("Order failed: %1").arg(e.what())); }
}

void ExecutionScreen::on_cancel_order(const QString& order_id) {
    if (trading_mode_ == TradingMode::Paper) {
        try { pt_cancel_order(order_id); OrderMatcher::instance().remove_order(order_id); refresh_portfolio(); }
        catch (const std::exception& e) { LOG_ERROR(TAG, QString("Cancel failed: %1").arg(e.what())); }
    } else {
        QPointer<ExecutionScreen> self = this;
        (void)QtConcurrent::run([self, order_id]() {
            if (!self) return;
            ExchangeService::instance().cancel_exchange_order(order_id, self->selected_symbol_);
            QMetaObject::invokeMethod(self, [self]() { if (self) self->refresh_live_data(); }, Qt::QueuedConnection);
        });
    }
}

void ExecutionScreen::on_ob_price_clicked(double price) { orderentry_->set_current_price(price); }

void ExecutionScreen::on_search_requested(const QString& filter) {
    QPointer<ExecutionScreen> self = this;
    (void)QtConcurrent::run([self, filter]() {
        if (!self) return;
        auto markets = ExchangeService::instance().fetch_markets("spot", filter);
        QMetaObject::invokeMethod(self, [self, markets]() { if (self) self->watchlist_->set_search_results(markets); }, Qt::QueuedConnection);
    });
}

void ExecutionScreen::on_symbol_submit() {
    QString sym = symbol_input_->text().trimmed().toUpper();
    if (!sym.isEmpty()) on_symbol_selected(sym);
}

void ExecutionScreen::on_mt5_creds_clicked() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("MT5 Credentials"); dlg->setMinimumWidth(400);
    auto* form = new QFormLayout(dlg);
    auto* login_ed = new QLineEdit(dlg); login_ed->setPlaceholderText("e.g. 831846");
    auto* pass_ed = new QLineEdit(dlg); pass_ed->setEchoMode(QLineEdit::Password); pass_ed->setPlaceholderText("Trading password");
    auto* server_ed = new QLineEdit(dlg); server_ed->setPlaceholderText("e.g. DooTechnology-Demo");
    form->addRow("Login:", login_ed); form->addRow("Password:", pass_ed); form->addRow("Server:", server_ed);
    auto* info_lbl = new QLabel("Local worker will start automatically");
    info_lbl->setStyleSheet("color: #808080; font-size: 10px; font-style: italic;");
    form->addRow(info_lbl);
    auto* btn_row = new QHBoxLayout;
    auto* save_btn = new QPushButton("Save & Connect", dlg); auto* cancel_btn = new QPushButton("Cancel", dlg);
    btn_row->addWidget(save_btn); btn_row->addWidget(cancel_btn); form->addRow(btn_row);
    connect(cancel_btn, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(save_btn, &QPushButton::clicked, dlg, [this, dlg, login_ed, pass_ed, server_ed]() {
        QString login = login_ed->text().trimmed();
        QString password = pass_ed->text();
        QString server = server_ed->text().trimmed();
        if (login.isEmpty()) { QMessageBox::warning(dlg, "Error", "Login required"); return; }
        ws_status_->setText("Setting up MT5 worker...");

        // Kill any existing worker process
        for (auto* p : findChildren<QProcess*>()) { if (p->objectName() == "mt5_worker") { p->kill(); p->deleteLater(); } }

        // Find bundled executables in app directory
        QString app_dir = QCoreApplication::applicationDirPath();
        QString worker_exe = app_dir + "/mt5_worker.exe";
        QString local_exe = app_dir + "/local_server.exe";

        if (!QFile::exists(worker_exe) || !QFile::exists(local_exe)) {
            // Fallback to VPS
            QJsonObject body; body["login"] = login; body["password"] = password; body["server"] = server;
            HttpClient::instance().post("/mt5/configure", body, [this, dlg](Result<QJsonDocument> r) {
                if (r.is_ok()) { ws_status_->setText("MT5 configured (VPS)"); mt5_connected_ = true;
                    QMessageBox::information(dlg, "Success", "MT5 configured on VPS"); dlg->accept(); }
                else { ws_status_->setText("Failed: " + QString::fromStdString(r.error())); }
            }, this);
            return;
        }

        // Launch local server (proxies market data + proxies to VPS for login/AI)
        ws_status_->setText("Starting local server...");
        auto* local_proc = new QProcess(this);
        local_proc->setObjectName("local_server");
        local_proc->start(local_exe, {"8157"});

        // Launch MT5 worker
        ws_status_->setText("Starting MT5 worker...");
        auto* proc = new QProcess(this);
        proc->setObjectName("mt5_worker");
        proc->start(worker_exe);
        if (proc->waitForStarted(5000)) {
            QTimer::singleShot(3000, this, [this, login, password, server, dlg]() {
                QTcpSocket sock;
                sock.connectToHost("127.0.0.1", 5570);
                if (sock.waitForConnected(2000)) {
                    QJsonObject cmd;
                    cmd["action"] = "connect";
                    cmd["login"] = login; cmd["password"] = password; cmd["server"] = server;
                    sock.write(QJsonDocument(cmd).toJson(QJsonDocument::Compact) + "\n");
                    sock.waitForBytesWritten(1000);
                    sock.waitForReadyRead(3000);
                    auto resp = sock.readAll();
                    sock.disconnectFromHost();
                    bool ok = resp.contains("ok");
                    ws_status_->setText(ok ? QString("MT5 Connected!") : QString("MT5: ") + QString(resp.left(50)));
                    mt5_connected_ = ok;
                    if (ok) QMessageBox::information(dlg, "Success", "MT5 connected! Trading locally.");
                    else QMessageBox::warning(dlg, "Error", "Worker started but connection failed: " + resp.left(80));
                } else {
                    ws_status_->setText("Worker not reachable on port 5570");
                    QMessageBox::warning(dlg, "Timeout", "Worker started but not responding on port 5570.");
                }
            });
            dlg->accept();
        } else {
            ws_status_->setText("Worker failed to start");
            QMessageBox::warning(dlg, "Error", "Could not start MT5 worker process.");
        }
    });
    dlg->exec(); dlg->deleteLater();
}

void ExecutionScreen::on_mt5_link_clicked() {
    refresh_mt5_link();
    const QString msg = mt5_connected_
        ? QString("MT5 linked via %1.\n\nLIVE orders route to MT5.").arg(active_ea_key_)
        : "MT5 not linked.\n\nOpen MT5, enable Algo Trading, attach GuardianBridge EA.";
    QMessageBox::information(this, "MT5 Link", msg);
}

// ══════════════════════════════════════════════════════════════════════════════
// Hub / WS (from merged CryptoTradingScreen)
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionScreen::hub_subscribe_topics() {
    if (ws_subscription_owner_) { delete ws_subscription_owner_; ws_subscription_owner_ = nullptr; }
    if (exchange_id_ != "kraken") return;
    auto& session = ExchangeSessionManager::instance();
    auto* sess = session.session(exchange_id_);
    if (!sess) return;
    auto* ws = sess->kraken_ws_client();
    if (!ws) { LOG_WARN(TAG, "kraken_ws_client() returned null"); return; }
    ws_subscription_owner_ = new QObject(this);
    QPointer<ExecutionScreen> self = this;
    connect(ws, &kraken::KrakenWsClient::ticker_received, ws_subscription_owner_,
            [self](const TickerData& t) {
                if (!self || t.symbol.isEmpty()) return;
                self->pending_tickers_[t.symbol] = t;
                if (t.symbol == self->selected_symbol_) { self->pending_primary_ticker_ = t; self->has_pending_primary_ = true; }
            });
    connect(ws, &kraken::KrakenWsClient::orderbook_received, ws_subscription_owner_,
            [self](const OrderBookData& ob) {
                if (!self || ob.symbol != self->selected_symbol_) return;
                self->pending_orderbook_ = ob; self->has_pending_orderbook_ = true;
            });
    connect(ws, &kraken::KrakenWsClient::trade_received, ws_subscription_owner_,
            [self](const TradeData& td) {
                if (!self || td.symbol != self->selected_symbol_) return;
                crypto::TradeEntry e; e.side = td.side; e.price = td.price; e.amount = td.amount;
                self->pending_trades_.append(e);
            });
    connect(ws, &kraken::KrakenWsClient::candle_received, ws_subscription_owner_,
            [self](const QString& sym, const QString&, const Candle& c) {
                if (!self || sym != self->selected_symbol_) return;
                self->pending_candles_.append(c);
            });
    LOG_INFO(TAG, "Direct WS signals connected (kraken)");
}

void ExecutionScreen::hub_unsubscribe_topics() {
    if (ws_subscription_owner_) { delete ws_subscription_owner_; ws_subscription_owner_ = nullptr; }
}

void ExecutionScreen::apply_feed_mode(bool ws_connected) {
    const int desired = ws_connected ? 1 : 0;
    if (desired == last_ws_state_) return;
    last_ws_state_ = desired;
    LOG_INFO(TAG, ws_connected ? "WS connected" : "WS disconnected");
}

void ExecutionScreen::flush_ws_updates() {
    apply_feed_mode(ExchangeService::instance().is_ws_connected());
    if (has_pending_primary_) {
        const auto& t = pending_primary_ticker_;
        ticker_->update_data(t.last, t.percentage, t.high, t.low, t.base_volume, ExchangeService::instance().is_ws_connected());
        if (t.bid > 0 && t.ask > 0) ticker_->update_bid_ask(t.bid, t.ask, std::abs(t.ask - t.bid));
        orderentry_->set_current_price(t.last); has_pending_primary_ = false;
    }
    if (has_pending_orderbook_) {
        const auto& ob = pending_orderbook_;
        orderbook_->set_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
        bottom_->set_depth_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
        has_pending_orderbook_ = false;
    }
    if (!pending_candles_.isEmpty() && execution_mode_ != ExecutionMode::MT5) {
        for (const auto& c : pending_candles_) crypto_chart_->append_candle(c);
        pending_candles_.clear();
    }
    if (!pending_trades_.isEmpty()) {
        for (const auto& entry : pending_trades_) bottom_->add_trade_entry(entry);
        pending_trades_.clear();
    }
    if (!pending_tickers_.isEmpty()) {
        QVector<TickerData> batch; batch.reserve(pending_tickers_.size());
        for (auto it = pending_tickers_.constBegin(); it != pending_tickers_.constEnd(); ++it) batch.append(it.value());
        pending_tickers_.clear();
        watchlist_->update_prices(batch);
        QHash<QString, double> last_prices;
        for (const auto& t : batch) { if (t.last > 0.0) last_prices.insert(t.symbol, t.last); }
        if (!last_prices.isEmpty()) bottom_->update_position_prices(last_prices);
        if (trading_mode_ == TradingMode::Paper && !portfolio_id_.isEmpty()) {
            bool expected = false;
            if (paper_bookkeeping_in_flight_.compare_exchange_strong(expected, true)) {
                QPointer<ExecutionScreen> self = this;
                const QString pid = portfolio_id_;
                (void)QtConcurrent::run([self, pid, batch]() {
                    struct Result { QVector<PtPosition> positions; bool fill_occurred = false; PtPortfolio portfolio; QVector<PtOrder> orders; QVector<PtTrade> trades; PtStats stats; }; Result r;
                    try {
                        for (const auto& ticker : batch) {
                            if (ticker.last <= 0) continue;
                            pt_update_position_price(pid, ticker.symbol, ticker.last);
                            PriceData pd; pd.last = ticker.last; pd.bid = ticker.bid; pd.ask = ticker.ask;
                            OrderMatcher::instance().check_orders(ticker.symbol, pd, pid);
                            OrderMatcher::instance().check_sl_tp_triggers(pid, ticker.symbol, ticker.last);
                        }
                        r.positions = pt_get_positions(pid); r.fill_occurred = pt_get_orders(pid, "open").size() < batch.size();
                        if (r.fill_occurred) { r.portfolio = pt_get_portfolio(pid); r.orders = pt_get_orders(pid); r.trades = pt_get_trades(pid, 50); r.stats = pt_get_stats(pid); }
                    } catch (...) {}
                    if (!self) return;
                    QMetaObject::invokeMethod(self, [self, r]() {
                        if (!self) return;
                        self->paper_bookkeeping_in_flight_.store(false);
                        self->bottom_->set_positions(r.positions);
                        if (r.fill_occurred) { self->portfolio_ = r.portfolio; self->orderentry_->set_balance(r.portfolio.balance); self->bottom_->set_orders(r.orders); self->bottom_->set_trades(r.trades); self->bottom_->set_stats(r.stats); }
                    }, Qt::QueuedConnection);
                });
            }
        }
    }
}

void ExecutionScreen::init_exchange() {
    auto& es = ExchangeService::instance();
    es.set_exchange(exchange_id_);
    if (!es.is_feed_running()) es.start_price_feed(5);
    if (!es.is_ws_active()) {
        const bool ws_spawned = es.start_ws_stream(selected_symbol_, watchlist_symbols_);
        if (!ws_spawned) {
            LOG_ERROR(TAG, "WS stream failed for " + exchange_id_);
            if (ws_status_) { ws_status_->setText("OFFLINE"); ws_status_->setProperty("ws", "offline"); ws_status_->style()->unpolish(ws_status_); ws_status_->style()->polish(ws_status_); }
            last_ws_status_label_state_ = 0;
        }
    }
    hub_subscribe_topics(); initialized_ = true;
}

void ExecutionScreen::load_portfolio() {
    QPointer<ExecutionScreen> self = this;
    const QString exch = exchange_id_;
    (void)QtConcurrent::run([self, exch]() {
        if (!self) return;
        trading::PtPortfolio portfolio;
        try {
            auto existing = pt_find_portfolio("Crypto Paper", exch);
            if (existing) portfolio = *existing;
            else portfolio = pt_create_portfolio("Crypto Paper", DEFAULT_PAPER_BALANCE, "USD", 1.0, "cross", 0.001, exch);
        } catch (...) { LOG_ERROR(TAG, "load_portfolio failed"); return; }
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, portfolio]() {
            if (!self) return;
            self->portfolio_ = portfolio; self->portfolio_id_ = portfolio.id;
            ExchangeService::instance().watch_symbol(self->selected_symbol_, self->portfolio_id_);
            self->refresh_portfolio();
        }, Qt::QueuedConnection);
    });
}

// ══════════════════════════════════════════════════════════════════════════════
// Refresh Functions
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionScreen::refresh_ticker() {
    if (!initialized_) return;
    auto& es = ExchangeService::instance();
    const auto cached = es.get_cached_price(selected_symbol_);
    if (cached.last > 0) {
        ticker_->update_data(cached.last, cached.percentage, cached.high, cached.low, cached.base_volume, es.is_ws_connected());
        if (cached.bid > 0 && cached.ask > 0) ticker_->update_bid_ask(cached.bid, cached.ask, std::abs(cached.ask - cached.bid));
        orderentry_->set_current_price(cached.last); return;
    }
    if (es.is_ws_connected()) return;
    QPointer<ExecutionScreen> self = this;
    (void)QtConcurrent::run([self]() {
        if (!self) return;
        auto ticker = ExchangeService::instance().fetch_ticker(self->selected_symbol_);
        if (!self || ticker.last <= 0) return;
        QMetaObject::invokeMethod(self, [self, ticker]() {
            if (!self || self->selected_symbol_ != ticker.symbol) return;
            auto& es = ExchangeService::instance();
            self->ticker_->update_data(ticker.last, ticker.percentage, ticker.high, ticker.low, ticker.base_volume, es.is_ws_connected());
            if (ticker.bid > 0 && ticker.ask > 0) self->ticker_->update_bid_ask(ticker.bid, ticker.ask, std::abs(ticker.ask - ticker.bid));
            self->orderentry_->set_current_price(ticker.last);
        }, Qt::QueuedConnection);
    });
}

void ExecutionScreen::refresh_orderbook() {
    if (!initialized_) return;
    QPointer<ExecutionScreen> self = this;
    (void)QtConcurrent::run([self]() {
        if (!self) return;
        auto ob = ExchangeService::instance().fetch_orderbook(self->selected_symbol_, OB_MAX_DISPLAY_LEVELS);
        QMetaObject::invokeMethod(self, [self, ob]() {
            if (!self) return;
            self->orderbook_->set_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
            self->bottom_->set_depth_data(ob.bids, ob.asks, ob.spread, ob.spread_pct);
        }, Qt::QueuedConnection);
    });
}

void ExecutionScreen::refresh_portfolio() {
    if (portfolio_id_.isEmpty() || trading_mode_ == TradingMode::Live) return;
    QPointer<ExecutionScreen> self = this;
    const QString pid = portfolio_id_;
    (void)QtConcurrent::run([self, pid]() {
        struct Snapshot { PtPortfolio portfolio; QVector<PtPosition> positions; QVector<PtOrder> orders; QVector<PtTrade> trades; PtStats stats; bool ok = false; }; Snapshot s;
        try { s.portfolio = pt_get_portfolio(pid); s.positions = pt_get_positions(pid); s.orders = pt_get_orders(pid); s.trades = pt_get_trades(pid, 50); s.stats = pt_get_stats(pid); s.ok = true; } catch (...) {}
        if (!self || !s.ok) return;
        QMetaObject::invokeMethod(self, [self, s]() {
            if (!self) return;
            self->portfolio_ = s.portfolio; self->orderentry_->set_balance(s.portfolio.balance);
            self->bottom_->set_positions(s.positions); self->bottom_->set_orders(s.orders); self->bottom_->set_trades(s.trades); self->bottom_->set_stats(s.stats);
        }, Qt::QueuedConnection);
    });
}

void ExecutionScreen::refresh_watchlist() {
    if (!initialized_) return;
    QPointer<ExecutionScreen> self = this;
    (void)QtConcurrent::run([self]() {
        if (!self) return;
        auto tickers = ExchangeService::instance().fetch_tickers(self->watchlist_symbols_);
        QMetaObject::invokeMethod(self, [self, tickers]() { if (self) self->watchlist_->update_prices(tickers); }, Qt::QueuedConnection);
    });
}

void ExecutionScreen::refresh_market_info() {
    if (!initialized_) return;
    QPointer<ExecutionScreen> self = this; const QString symbol = selected_symbol_;
    (void)QtConcurrent::run([self, symbol]() {
        if (!self) return;
        auto fr = ExchangeService::instance().fetch_funding_rate(symbol);
        QMetaObject::invokeMethod(self, [self, symbol, fr]() {
            if (!self || self->selected_symbol_ != symbol) return;
            self->market_info_cache_.funding_rate = fr.funding_rate; self->market_info_cache_.mark_price = fr.mark_price;
            self->market_info_cache_.index_price = fr.index_price; self->market_info_cache_.has_data = true;
            self->ticker_->update_mark_price(fr.mark_price, fr.index_price); self->bottom_->set_market_info(self->market_info_cache_);
        }, Qt::QueuedConnection);
    });
    (void)QtConcurrent::run([self, symbol]() {
        if (!self) return;
        auto oi = ExchangeService::instance().fetch_open_interest(symbol);
        QMetaObject::invokeMethod(self, [self, symbol, oi]() {
            if (!self || self->selected_symbol_ != symbol) return;
            self->market_info_cache_.open_interest = oi.open_interest; self->market_info_cache_.open_interest_value = oi.open_interest_value;
            self->market_info_cache_.has_data = true; self->bottom_->set_market_info(self->market_info_cache_);
        }, Qt::QueuedConnection);
    });
}

void ExecutionScreen::refresh_candles() { async_fetch_candles(selected_symbol_, "1h"); }

void ExecutionScreen::refresh_live_data() {
    if (trading_mode_ != TradingMode::Live) return;
    if (live_inflight_.load() > 0) return;
    live_inflight_.store(4); async_fetch_live_positions(); async_fetch_live_orders(); async_fetch_live_balance(); async_fetch_my_trades();
}

void ExecutionScreen::refresh_account() {
    HttpClient::instance().get("/mt5/account", [this](Result<QJsonDocument> r) {
        if (!r.is_ok()) return;
        auto d = r.value().object()["data"].toObject();
        double bal = d["balance"].toDouble(); double eq = d["equity"].toDouble();
        double bp = d["margin_free"].toDouble(); double pnl = eq - bal;
        if (bal_lbl_) bal_lbl_->setText(QString("$%1").arg(bal, 0, 'f', 2));
        if (eq_lbl_) eq_lbl_->setText(QString("$%1").arg(eq, 0, 'f', 2));
        if (bp_lbl_) bp_lbl_->setText(QString("$%1").arg(bp, 0, 'f', 0));
        if (pnl_lbl_) { pnl_lbl_->setText(QString("%$%1").arg(pnl, 0, 'f', 2)); pnl_lbl_->setStyleSheet(QString("color:%1;font-weight:700;font-size:11px;").arg(pnl >= 0 ? "#22c55e" : "#ef4444")); }
    }, this);
}

void ExecutionScreen::refresh_mt5_link() {
    HttpClient::instance().get("/mt5/ea/connections", [this](Result<QJsonDocument> r) {
        if (!r.is_ok()) { set_mt5_link_state(false, {}, "backend/auth unavailable"); return; }
        const auto data = r.value().object()["data"].toObject(); QString ea_key;
        for (const QString& key : data.keys()) { if (data[key].toObject()["connected"].toBool()) { ea_key = key; break; } }
        set_mt5_link_state(!ea_key.isEmpty(), ea_key);
    }, this);
}

void ExecutionScreen::refresh_deployed_scripts() {
    HttpClient::instance().get("/execution/scripts/status", [this](Result<QJsonDocument> r) {
        if (!script_status_) return;
        if (!r.is_ok()) { script_status_->setText("Execution scripts unavailable"); return; }
        const auto arr = r.value().object()["data"].toArray();
        if (arr.isEmpty()) { script_status_->setText("No scripts deployed"); return; }
        QStringList rows;
        for (const auto& v : arr) {
            const auto obj = v.toObject();
            rows << QString("%1 [%2] %3 | %4%5").arg(obj["name"].toString("Unnamed"), obj["language"].toString().toUpper(), obj["status"].toString(), obj["target"].toString()).arg(obj["mt5_deployed"].toBool() ? " | MT5 deployed" : "");
        }
        script_status_->setText(rows.join("     "));
    }, this);
}

void ExecutionScreen::set_mt5_link_state(bool connected, const QString& ea_key, const QString& detail) {
    mt5_connected_ = connected; active_ea_key_ = connected ? ea_key : QString();
    if (mt5_status_) mt5_status_->setText(connected ? "MT5: LINKED" : "MT5: OFF");
    if (mt5_link_btn_) {
        mt5_link_btn_->setText(connected ? "MT5 LINKED" : "MT5 LINK");
        mt5_link_btn_->setStyleSheet(connected
            ? "QPushButton{background:#11331f;color:#22c55e;border:1px solid #22c55e;padding:2px 10px;font-weight:700;font-size:10px;border-radius:3px;}"
            : "QPushButton{background:#111;color:#e5e5e5;border:1px solid #3a3a48;padding:2px 10px;font-weight:700;font-size:10px;border-radius:3px;}");
    }
    if (!detail.isEmpty() && ws_status_) ws_status_->setText("MT5: " + detail);
}

void ExecutionScreen::open_market_data_ws() {
    if (!market_ws_ || market_ws_->state() == QAbstractSocket::ConnectedState || market_ws_->state() == QAbstractSocket::ConnectingState) return;
    const QString sym = symbol_input_ && !symbol_input_->text().trimmed().isEmpty() ? symbol_input_->text().trimmed().toUpper() : "XAUUSD";
    QUrlQuery q; q.addQueryItem("symbol", sym);
    market_ws_->open(QUrl(execution_ws_url("/ws/market-data", q)));
}

// ══════════════════════════════════════════════════════════════════════════════
// Async Fetches
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionScreen::async_fetch_candles(const QString& symbol, const QString& timeframe) {
    if (candles_fetching_.exchange(true)) return;
    QPointer<ExecutionScreen> self = this;
    (void)QtConcurrent::run([self, symbol, timeframe]() {
        auto candles = ExchangeService::instance().fetch_ohlcv(symbol, timeframe, OHLCV_FETCH_COUNT);
        if (!self) return; self->candles_fetching_ = false;
        QMetaObject::invokeMethod(self, [self, candles]() { if (!self) return; if (self->crypto_chart_) self->crypto_chart_->set_candles(candles); }, Qt::QueuedConnection);
    });
}

void ExecutionScreen::async_fetch_live_positions() {
    QPointer<ExecutionScreen> self = this;
    (void)QtConcurrent::run([self]() {
        if (!self) return;
        auto result = ExchangeService::instance().fetch_positions_live(self->selected_symbol_);
        QMetaObject::invokeMethod(self, [self, result]() {
            if (!self) return;
            if (result.contains("positions")) {
                auto arr = result.value("positions").toArray();
                self->bottom_->set_live_positions(arr);
                // Push positions to chart overlay
                if (self->crypto_chart_) {
                    QVector<fincept::ui::PositionLevel> pl;
                    for (const auto& v : arr) {
                        auto o = v.toObject();
                        fincept::ui::PositionLevel p;
                        p.symbol = o.value("symbol").toString();
                        double amt = o.value("positionAmt").toString().toDouble();
                        if (o.contains("positionAmt")) amt = o.value("positionAmt").toVariant().toDouble();
                        p.side = (amt >= 0) ? "long" : "short";
                        p.quantity = std::abs(amt);
                        p.entry_price = o.value("entryPrice").toVariant().toDouble();
                        p.current_price = o.value("markPrice").toVariant().toDouble();
                        p.pnl = o.value("unRealizedProfit").toVariant().toDouble();
                        if (o.contains("stopLoss")) p.stop_loss = o.value("stopLoss").toVariant().toDouble();
                        if (o.contains("takeProfit")) p.take_profit = o.value("takeProfit").toVariant().toDouble();
                        pl.append(p);
                    }
                    self->crypto_chart_->update_positions(pl);
                }
            }
            self->live_inflight_.fetch_sub(1);
        }, Qt::QueuedConnection);
    });
}

void ExecutionScreen::async_fetch_live_orders() {
    QPointer<ExecutionScreen> self = this;
    (void)QtConcurrent::run([self]() {
        if (!self) return;
        auto result = ExchangeService::instance().fetch_open_orders_live(self->selected_symbol_);
        QMetaObject::invokeMethod(self, [self, result]() { if (!self) return; if (result.contains("orders")) self->bottom_->set_live_orders(result.value("orders").toArray()); self->live_inflight_.fetch_sub(1); }, Qt::QueuedConnection);
    });
}

void ExecutionScreen::async_fetch_live_balance() {
    QPointer<ExecutionScreen> self = this;
    (void)QtConcurrent::run([self]() {
        if (!self) return;
        auto result = ExchangeService::instance().fetch_balance();
        QMetaObject::invokeMethod(self, [self, result]() {
            if (!self) return;
            double total = result.value("total").toObject().value("USDT").toDouble();
            double free = result.value("free").toObject().value("USDT").toDouble();
            double used = result.value("used").toObject().value("USDT").toDouble();
            self->bottom_->set_live_balance(free, total, used); self->orderentry_->set_balance(free); self->live_inflight_.fetch_sub(1);
        }, Qt::QueuedConnection);
    });
}

void ExecutionScreen::async_fetch_my_trades() {
    QPointer<ExecutionScreen> self = this;
    (void)QtConcurrent::run([self]() {
        if (!self) return;
        auto result = ExchangeService::instance().fetch_my_trades(self->selected_symbol_);
        QMetaObject::invokeMethod(self, [self, result]() { if (!self) return; self->bottom_->update_my_trades(result); self->live_inflight_.fetch_sub(1); }, Qt::QueuedConnection);
    });
}

void ExecutionScreen::async_fetch_trading_fees() {
    QPointer<ExecutionScreen> self = this;
    (void)QtConcurrent::run([self]() {
        if (!self) return;
        auto result = ExchangeService::instance().fetch_trading_fees(self->selected_symbol_);
        QMetaObject::invokeMethod(self, [self, result]() { if (!self) return; self->bottom_->update_fees(result); }, Qt::QueuedConnection);
    });
}

void ExecutionScreen::async_fetch_mark_price() {
    QPointer<ExecutionScreen> self = this;
    (void)QtConcurrent::run([self]() {
        if (!self) return;
        auto mp = ExchangeService::instance().fetch_mark_price(self->selected_symbol_);
        QMetaObject::invokeMethod(self, [self, mp]() { if (!self) return; self->ticker_->update_mark_price(mp.mark_price, mp.index_price); }, Qt::QueuedConnection);
    });
}

void ExecutionScreen::async_set_leverage(int leverage) {
    const QString symbol = selected_symbol_;
    (void)QtConcurrent::run([symbol, leverage]() { ExchangeService::instance().set_leverage(symbol, leverage); });
}

void ExecutionScreen::async_set_margin_mode(const QString& mode) {
    const QString symbol = selected_symbol_;
    (void)QtConcurrent::run([symbol, mode]() { ExchangeService::instance().set_margin_mode(symbol, mode); });
}

// ══════════════════════════════════════════════════════════════════════════════
// State
// ══════════════════════════════════════════════════════════════════════════════

QVariantMap ExecutionScreen::save_state() const {
    QVariantMap state;
    state["exchange_id"] = exchange_id_; state["selected_symbol"] = selected_symbol_;
    state["execution_mode"] = static_cast<int>(execution_mode_);
    state["selected_stock"] = selected_stock_; state["stock_exchange"] = stock_exchange_;
    return state;
}

void ExecutionScreen::restore_state(const QVariantMap& state) {
    const int mode = state.value("execution_mode", 1).toInt();
    on_mode_changed(static_cast<ExecutionMode>(mode));
    const QString exch = state.value("exchange_id", "kraken").toString();
    const QString sym = state.value("selected_symbol", "BTC/USDT").toString();
    const bool exch_changed = (exch != exchange_id_); const bool sym_changed = (sym != selected_symbol_);
    if (!exch_changed && !sym_changed) return;
    if (exch_changed) {
        if (sym_changed) { selected_symbol_ = sym; symbol_input_->setText(sym); ticker_->set_symbol(sym); orderentry_->set_symbol(sym); watchlist_->set_active_symbol(sym); }
        on_exchange_changed(exch); return;
    }
    if (sym_changed) on_symbol_selected(sym);
}

// ══════════════════════════════════════════════════════════════════════════════
// Multi-Chart / Bar Replay / Options Chain Toggles
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionScreen::toggle_multi_chart() {
    auto* cl = qobject_cast<QVBoxLayout*>(chart_container_->layout());
    if (!cl) return;
    if (!multi_chart_widget_) {
        multi_chart_widget_ = new MT5FleetMultiChartContainer(chart_container_);
        int idx = cl->indexOf(crypto_chart_);
        if (idx >= 0) cl->insertWidget(idx, multi_chart_widget_);
    } else {
        multi_chart_widget_->deleteLater();
        multi_chart_widget_ = nullptr;
    }
}

void ExecutionScreen::toggle_replay() {
    auto* cl = qobject_cast<QVBoxLayout*>(chart_container_->layout());
    if (!cl) return;
    if (!replay_widget_) {
        replay_widget_ = new TickReplayWidget(chart_container_);
        int idx = cl->indexOf(crypto_chart_);
        if (idx >= 0) cl->insertWidget(idx, replay_widget_);
    } else {
        replay_widget_->deleteLater();
        replay_widget_ = nullptr;
    }
}

void ExecutionScreen::toggle_options() {
    auto* cl = qobject_cast<QVBoxLayout*>(chart_container_->layout());
    if (!cl) return;
    if (!options_widget_) {
        options_widget_ = new fno::ChainSubTab(chart_container_);
        int idx = cl->indexOf(crypto_chart_);
        if (idx >= 0) cl->insertWidget(idx, options_widget_);
    } else {
        options_widget_->deleteLater();
        options_widget_ = nullptr;
    }
}

} // namespace fincept::screens
