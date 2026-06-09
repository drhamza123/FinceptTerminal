#pragma once
// Native Massive.com WebSocket client for real-time US stock market data.
//
// Massive provides NBBO quotes, trades, and per-second/minute aggregates
// for US stocks, forex, crypto, futures, and options via WebSocket.
//
// Endpoints (all at wss://socket.massive.com):
//   /stocks/Q   — NBBO quotes (bid/ask)
//   /stocks/T   — tick-level trades
//   /stocks/A   — per-second OHLC aggregates
//   /stocks/AM  — per-minute OHLC aggregates
//   /forex/C    — forex BBO quotes
//   /crypto/XQ  — crypto quotes
//
// Authentication: apiKey query parameter.
//
// Threading: lives on the thread that owns it. All Qt signals fire there.

#include "trading/TradingTypes.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include <atomic>

namespace fincept {
class WebSocketClient;
}

namespace fincept::trading::massive {

class MassiveWsClient : public QObject {
    Q_OBJECT
  public:
    explicit MassiveWsClient(const QString& api_key, QObject* parent = nullptr);
    ~MassiveWsClient() override;

    /// Connect and subscribe to stock quotes for the given tickers.
    /// `primary_symbol` gets full depth + trades; all symbols get quotes.
    /// Idempotent — subsequent calls re-subscribe.
    void start(const QString& primary_symbol, const QStringList& all_symbols);

    /// Close the connection cleanly.
    void stop();

    /// Swap primary symbol without reconnecting.
    void set_primary_symbol(const QString& symbol);

    bool is_connected() const { return connected_.load(); }

  signals:
    void ticker_received(const fincept::trading::TickerData& ticker);
    void orderbook_received(const fincept::trading::OrderBookData& orderbook);
    void trade_received(const fincept::trading::TradeData& trade);
    void candle_received(const QString& symbol, const QString& interval,
                         const fincept::trading::Candle& candle);
    void connection_changed(bool connected);

  private slots:
    void on_ws_connected();
    void on_ws_disconnected();
    void on_ws_message(const QString& message);
    void on_ws_error(const QString& error);

  private:
    void subscribe(const QString& endpoint, const QStringList& symbols);
    void handle_message(const QJsonObject& msg);

    // Message handlers per event type
    void handle_stock_quote(const QJsonObject& msg);    // ev: Q
    void handle_stock_trade(const QJsonObject& msg);     // ev: T
    void handle_stock_aggregate(const QJsonObject& msg); // ev: A / AM
    void handle_forex_quote(const QJsonObject& msg);     // ev: C
    void handle_crypto_quote(const QJsonObject& msg);    // ev: XQ

    QString build_url(const QString& endpoint, const QStringList& symbols) const;

    fincept::WebSocketClient* ws_ = nullptr;
    QTimer* heartbeat_timer_ = nullptr;

    QString api_key_;
    QString primary_symbol_;
    QStringList all_symbols_;
    std::atomic<bool> connected_{false};

    static constexpr int HEARTBEAT_TIMEOUT_MS = 15000;
    static constexpr const char* WS_BASE_URL = "wss://socket.massive.com";
};

} // namespace fincept::trading::massive
