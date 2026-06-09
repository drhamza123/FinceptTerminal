#pragma once
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolGroup.h"
#include "screens/common/IStatefulScreen.h"
#include "screens/crypto_trading/CryptoTypes.h"
#include "trading/TradingTypes.h"
#include <QHideEvent>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QWebSocket>
#include <QWidget>
#include <QSplitter>

#include <atomic>

namespace fincept::screens::crypto {
class CryptoTickerBar;
class CryptoWatchlist;
class CryptoChart;
class CryptoOrderEntry;
class CryptoOrderBook;
class CryptoBottomPanel;
} // namespace fincept::screens::crypto

namespace fincept::screens {

enum class ExecutionMode { CryptoWallet, CryptoTrade, Stocks, MT5, BrokerApi };

class ExecutionScreen : public QWidget, public IStatefulScreen, public IGroupLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit ExecutionScreen(QWidget* parent = nullptr);
    ~ExecutionScreen();

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "execution"; }
    int state_version() const override { return 3; }

    void set_group(SymbolGroup g) override { link_group_ = g; }
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    // Mode switching
    void on_mode_changed(ExecutionMode mode);

    // Wallet (managed by embedded CryptoCenterScreen)

    // Crypto exchange
    void on_exchange_changed(const QString& exchange);
    void on_symbol_selected(const QString& symbol);
    void on_mode_toggled();
    void on_api_clicked();
    void on_order_submitted(const QString& side, const QString& order_type, double qty, double price,
                            double stop_price, double sl, double tp);
    void on_cancel_order(const QString& order_id);
    void on_ob_price_clicked(double price);
    void on_search_requested(const QString& filter);

    // Stocks
    void on_stock_exchange_changed(const QString& exchange);
    void on_stock_symbol_selected(const QString& symbol);
    void on_stock_order_submitted(const QString& side, const QString& order_type, double qty, double price);

    // MT5
    void on_mt5_link_clicked();
    void on_mt5_creds_clicked();
    void on_symbol_submit();

    // Refresh
    void refresh_ticker();
    void refresh_orderbook();
    void refresh_portfolio();
    void refresh_watchlist();
    void refresh_market_info();
    void refresh_candles();
    void refresh_live_data();
    void update_clock();
    void refresh_account();
    void refresh_mt5_link();
    void refresh_deployed_scripts();

  private:
    void setup_ui();
    void setup_timers();
    void init_exchange();
    void load_portfolio();
    void switch_symbol(const QString& symbol);
    void set_mt5_link_state(bool connected, const QString& ea_key = {}, const QString& detail = {});
    void open_market_data_ws();
    void rebuild_mode_panels();

    // Wallet (delegated to embedded CryptoCenterScreen)
    void init_wallet();
    void build_wallet_panel(QWidget* parent);

    // Stocks
    void refresh_stock_quotes();
    void refresh_stock_chart(const QString& symbol);
    void build_stock_panel(QWidget* parent);

    // Crypto async fetches
    void async_fetch_candles(const QString& symbol, const QString& timeframe);
    void async_fetch_live_positions();
    void async_fetch_live_orders();
    void async_fetch_live_balance();
    void async_fetch_my_trades();
    void async_fetch_trading_fees();
    void async_fetch_mark_price();
    void async_set_leverage(int leverage);
    void async_set_margin_mode(const QString& mode);

    // Crypto hub subscriptions
    void hub_subscribe_topics();
    void hub_unsubscribe_topics();
    void apply_feed_mode(bool ws_connected);
    void flush_ws_updates();

    // ── Mode ──
    ExecutionMode execution_mode_ = ExecutionMode::CryptoTrade;
    QPushButton* mode_selector_btn_ = nullptr;
    QMenu* mode_selector_menu_ = nullptr;
    QStackedWidget* mode_stack_ = nullptr;

    // ── Wallet (embedded CryptoCenterScreen) ──
    QWidget* wallet_panel_ = nullptr;

    // ── Crypto exchange ──
    QString exchange_id_ = "kraken";
    QString selected_symbol_ = "BTC/USDT";
    crypto::TradingMode trading_mode_ = crypto::TradingMode::Paper;
    QPushButton* exchange_btn_ = nullptr;
    QMenu* exchange_menu_ = nullptr;
    QLabel* ws_transport_ = nullptr;
    QLabel* clock_label_ = nullptr;

    // ── MT5 ──
    QPushButton* mt5_link_btn_ = nullptr;
    QPushButton* creds_btn_ = nullptr;
    QLabel* mt5_status_ = nullptr;
    QLabel* script_status_ = nullptr;
    bool mt5_connected_ = false;
    QString active_ea_key_;
    QWebSocket* market_ws_ = nullptr;
    bool is_paper_mode_ = true;

    // ── Stocks ──
    QWidget* stock_panel_ = nullptr;
    QString stock_exchange_ = "US";
    QString selected_stock_ = "AAPL";
    QPushButton* stock_exchange_btn_ = nullptr;
    QLineEdit* stock_symbol_input_ = nullptr;
    QLabel* stock_price_lbl_ = nullptr;
    QLabel* stock_change_lbl_ = nullptr;
    QLabel* stock_volume_lbl_ = nullptr;
    QLabel* stock_high_lbl_ = nullptr;
    QLabel* stock_low_lbl_ = nullptr;
    QTreeWidget* stock_watchlist_ = nullptr;

    // ── Command bar widgets ──
    QLineEdit* symbol_input_ = nullptr;
    QPushButton* mode_btn_ = nullptr;
    QPushButton* api_btn_ = nullptr;
    QLabel* ws_status_ = nullptr;
    crypto::CryptoTickerBar* ticker_ = nullptr;
    crypto::CryptoWatchlist* watchlist_ = nullptr;
    crypto::CryptoOrderBook* orderbook_ = nullptr;
    crypto::CryptoOrderEntry* orderentry_ = nullptr;
    crypto::CryptoBottomPanel* bottom_ = nullptr;

    // ── Chart (switchable by mode) ──
    QWidget* chart_container_ = nullptr;
    QWidget* current_chart_ = nullptr;
    crypto::CryptoChart* crypto_chart_ = nullptr;
    class MT5FleetChartPanel* mt5_chart_ = nullptr;
    QSplitter* center_splitter_ = nullptr;
    void swap_chart_for_mode(ExecutionMode mode);

    // ── Wallet display ──
    QLabel* bal_lbl_ = nullptr;
    QLabel* eq_lbl_ = nullptr;
    QLabel* bp_lbl_ = nullptr;
    QLabel* pnl_lbl_ = nullptr;

    // ── Crypto timers ──
    QTimer* ticker_timer_ = nullptr;
    QTimer* ob_timer_ = nullptr;
    QTimer* portfolio_timer_ = nullptr;
    QTimer* watchlist_timer_ = nullptr;
    QTimer* market_info_timer_ = nullptr;
    QTimer* live_data_timer_ = nullptr;
    QTimer* clock_timer_ = nullptr;
    QTimer* ws_flush_timer_ = nullptr;
    QTimer* stock_refresh_timer_ = nullptr;

    // ── Paper trading state ──
    QString portfolio_id_;
    trading::PtPortfolio portfolio_;

    // ── Async fetch guards ──
    std::atomic<bool> candles_fetching_{false};
    std::atomic<int> live_inflight_{0};
    std::atomic<bool> paper_bookkeeping_in_flight_{false};
    bool startup_fetches_done_ = false;

    // ── WS state ──
    int last_ws_state_ = -1;
    int last_ws_status_label_state_ = -1;
    QObject* ws_subscription_owner_ = nullptr;

    // ── WS update coalescing ──
    QHash<QString, trading::TickerData> pending_tickers_;
    trading::TickerData pending_primary_ticker_;
    bool has_pending_primary_ = false;
    trading::OrderBookData pending_orderbook_;
    bool has_pending_orderbook_ = false;
    QVector<trading::Candle> pending_candles_;
    QVector<crypto::TradeEntry> pending_trades_;

    // ── Market info cache ──
    crypto::MarketInfoData market_info_cache_;
    bool initialized_ = false;

    // ── Symbol group (Phase 7) ──
    SymbolGroup link_group_ = SymbolGroup::None;

    // ── Watchlist symbols ──
    QStringList watchlist_symbols_{
        "BTC/USDT",  "ETH/USDT",  "SOL/USDT",  "BNB/USDT",  "XRP/USDT",   "DOGE/USDT", "ADA/USDT",
        "AVAX/USDT", "TON/USDT",  "LINK/USDT", "DOT/USDT",  "MATIC/USDT", "UNI/USDT",  "ATOM/USDT",
        "LTC/USDT",  "BCH/USDT",  "APT/USDT",  "ARB/USDT",  "OP/USDT",    "SUI/USDT",  "TRX/USDT",
        "INJ/USDT",  "NEAR/USDT", "WIF/USDT",  "PEPE/USDT",
    };

    // MT5 watchlist for non-crypto mode
    QStringList mt5_symbols_{
        "XAUUSD","XAGUSD","EURUSD","GBPUSD","USDJPY","BTCUSD","ETHUSD",
        "SPY","QQQ","AAPL","MSFT","NVDA","TSLA","AMZN","GOOGL","META"
    };

    // Stock watchlist symbols
    QStringList stock_symbols_{
        "AAPL","MSFT","GOOGL","AMZN","NVDA","TSLA","META","JPM","V","JNJ",
        "WMT","PG","MA","UNH","HD","DIS","NFLX","ADBE","CRM","PYPL",
        "INTC","AMD","BA","GE","CAT","XOM","CVX","KO","PEP","NKE"
    };


};

} // namespace fincept::screens
