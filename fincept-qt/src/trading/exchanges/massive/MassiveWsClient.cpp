#include "trading/exchanges/massive/MassiveWsClient.h"
#include "core/logging/Logger.h"
#include "network/websocket/WebSocketClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>

namespace fincept::trading::massive {

static const QString TAG = "MassiveWsClient";

MassiveWsClient::MassiveWsClient(const QString& api_key, QObject* parent)
    : QObject(parent), api_key_(api_key) {
}

MassiveWsClient::~MassiveWsClient() {
    stop();
}

void MassiveWsClient::start(const QString& primary_symbol, const QStringList& all_symbols) {
    if (api_key_.isEmpty()) {
        LOG_WARN(TAG, "Massive API key not set — cannot connect");
        return;
    }

    primary_symbol_ = primary_symbol;
    all_symbols_ = all_symbols;

    // Close any existing connection
    stop();

    ws_ = new fincept::WebSocketClient(this);
    connect(ws_, &fincept::WebSocketClient::connected, this, &MassiveWsClient::on_ws_connected);
    connect(ws_, &fincept::WebSocketClient::disconnected, this, &MassiveWsClient::on_ws_disconnected);
    connect(ws_, &fincept::WebSocketClient::message_received, this, &MassiveWsClient::on_ws_message);
    connect(ws_, &fincept::WebSocketClient::error_occurred, this, &MassiveWsClient::on_ws_error);

    // Subscribe to stock quotes for all symbols
    QString url = build_url("/stocks/Q", all_symbols);
    LOG_INFO(TAG, QString("Connecting to Massive: %1 symbols").arg(all_symbols.size()));
    ws_->connect_to(url);
}

void MassiveWsClient::stop() {
    if (ws_) {
        ws_->disconnect();
        ws_->deleteLater();
        ws_ = nullptr;
    }
    connected_.store(false);
}

void MassiveWsClient::set_primary_symbol(const QString& symbol) {
    primary_symbol_ = symbol;
}

void MassiveWsClient::subscribe(const QString& endpoint, const QStringList& symbols) {
    if (!ws_ || !connected_.load()) return;
    // For Massive, subscriptions are done via URL parameters on connect.
    // To change symbols, we reconnect with the new set.
    LOG_INFO(TAG, QString("Massive subscribe: %1 %2 symbols").arg(endpoint).arg(symbols.size()));
}

QString MassiveWsClient::build_url(const QString& endpoint, const QStringList& symbols) const {
    QUrl url(QStringLiteral("%1%2").arg(WS_BASE_URL, endpoint));
    QUrlQuery query;
    query.addQueryItem("apiKey", api_key_);
    if (!symbols.isEmpty())
        query.addQueryItem("ticker", symbols.join(","));
    url.setQuery(query);
    return url.toString();
}

// ── Slots ───────────────────────────────────────────────────────────────────

void MassiveWsClient::on_ws_connected() {
    connected_.store(true);
    LOG_INFO(TAG, "Massive WebSocket connected");
    emit connection_changed(true);
}

void MassiveWsClient::on_ws_disconnected() {
    connected_.store(false);
    LOG_WARN(TAG, "Massive WebSocket disconnected");
    emit connection_changed(false);
}

void MassiveWsClient::on_ws_message(const QString& message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;
    handle_message(doc.object());
}

void MassiveWsClient::on_ws_error(const QString& error) {
    LOG_WARN(TAG, QString("Massive WebSocket error: %1").arg(error));
}

// ── Message dispatch ────────────────────────────────────────────────────────

void MassiveWsClient::handle_message(const QJsonObject& msg) {
    const QString ev = msg.value("ev").toString();
    if (ev == "Q")
        handle_stock_quote(msg);
    else if (ev == "T")
        handle_stock_trade(msg);
    else if (ev == "A" || ev == "AM")
        handle_stock_aggregate(msg);
    else if (ev == "C")
        handle_forex_quote(msg);
    else if (ev == "XQ")
        handle_crypto_quote(msg);
}

// ── Stock Quote (NBBO) ──────────────────────────────────────────────────────

void MassiveWsClient::handle_stock_quote(const QJsonObject& msg) {
    // Fields: sym, bx, bp, bs, ax, ap, as, t (ms)
    TickerData t;
    t.symbol = msg.value("sym").toString();
    t.bid = msg.value("bp").toDouble();
    t.ask = msg.value("ap").toDouble();
    t.last = (t.bid + t.ask) / 2.0;
    t.timestamp = msg.value("t").toVariant().toLongLong() / 1000;
    emit ticker_received(t);

    // Build orderbook snapshot from single quote level
    if (t.symbol == primary_symbol_) {
        OrderBookData ob;
        ob.symbol = t.symbol;
        ob.bids.append({t.bid, msg.value("bs").toDouble()});
        ob.asks.append({t.ask, msg.value("as").toDouble()});
        ob.spread = t.ask - t.bid;
        ob.spread_pct = t.bid > 0 ? (ob.spread / t.bid) * 100.0 : 0;
        emit orderbook_received(ob);
    }
}

// ── Stock Trade ─────────────────────────────────────────────────────────────

void MassiveWsClient::handle_stock_trade(const QJsonObject& msg) {
    // Fields: sym, p (price), s (size), t (ms)
    TradeData td;
    td.symbol = msg.value("sym").toString();
    td.price = msg.value("p").toDouble();
    td.amount = msg.value("s").toDouble();
    td.timestamp = msg.value("t").toVariant().toLongLong() / 1000;
    emit trade_received(td);

    // Also emit as ticker for price updates
    TickerData t;
    t.symbol = td.symbol;
    t.last = td.price;
    t.timestamp = td.timestamp;
    emit ticker_received(t);
}

// ── Stock Aggregates (OHLC) ─────────────────────────────────────────────────

void MassiveWsClient::handle_stock_aggregate(const QJsonObject& msg) {
    // Fields: sym, o, h, l, c, v, s (start ms), e (end ms)
    Candle c;
    c.open = msg.value("o").toDouble();
    c.high = msg.value("h").toDouble();
    c.low = msg.value("l").toDouble();
    c.close = msg.value("c").toDouble();
    c.volume = msg.value("v").toDouble();
    c.timestamp = msg.value("s").toVariant().toLongLong() / 1000;

    const QString sym = msg.value("sym").toString();
    const QString interval = msg.value("ev").toString() == "AM" ? "1m" : "1s";
    emit candle_received(sym, interval, c);
}

// ── Forex Quote ─────────────────────────────────────────────────────────────

void MassiveWsClient::handle_forex_quote(const QJsonObject& msg) {
    // Fields: p (pair), b (bid), a (ask), t (ms)
    TickerData t;
    t.symbol = msg.value("p").toString();
    t.bid = msg.value("b").toDouble();
    t.ask = msg.value("a").toDouble();
    t.last = (t.bid + t.ask) / 2.0;
    t.timestamp = msg.value("t").toVariant().toLongLong() / 1000;
    emit ticker_received(t);
}

// ── Crypto Quote ────────────────────────────────────────────────────────────

void MassiveWsClient::handle_crypto_quote(const QJsonObject& msg) {
    // Fields: pair, bp (bid price), bs (bid size), ap (ask price), as (ask size), t (ms)
    TickerData t;
    t.symbol = msg.value("pair").toString();
    t.bid = msg.value("bp").toDouble();
    t.ask = msg.value("ap").toDouble();
    t.last = (t.bid + t.ask) / 2.0;
    t.timestamp = msg.value("t").toVariant().toLongLong() / 1000;
    emit ticker_received(t);
}

} // namespace fincept::trading::massive
