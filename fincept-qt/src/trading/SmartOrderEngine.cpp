#include "SmartOrderEngine.h"
#include "trading/OrderSerializer.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QAbstractSocket>
#include <QDebug>
#include <QDateTime>
#include "trading/BrokerInterface.h"
#include <zmq.hpp>
#include <thread>
#include <chrono>
#include <map>

namespace fincept::trading {

static std::unique_ptr<zmq::context_t> g_zmqCtx;
static std::unique_ptr<zmq::socket_t> g_zmqPush;

SmartOrderEngine& SmartOrderEngine::instance() {
    static SmartOrderEngine engine;
    return engine;
}

SmartOrderEngine::SmartOrderEngine(QObject* parent) : QObject(parent) {
    queue_.resize(RING_SIZE);
}

SmartOrderEngine::~SmartOrderEngine() {
    disconnectFromGateway();
}

bool SmartOrderEngine::submitOrder(const QString& symbol, const QString& side,
                                    double volume, double sl, double tp) {
    size_t currentTail = tail_.load(std::memory_order_relaxed);
    size_t nextTail = (currentTail + 1) % RING_SIZE;
    if (nextTail == head_.load(std::memory_order_acquire))
        return false;

    auto& slot = queue_[currentTail];
    slot.order->symbol = symbol;
    slot.order->side = side;
    slot.order->volume = volume;
    slot.order->sl = sl;
    slot.order->tp = tp;
    slot.order->processed.store(false, std::memory_order_release);
    slot.order->filled.store(false, std::memory_order_release);
    slot.order->ticket = 0;
    slot.order->fillPrice = 0;
    slot.order->latencyMs = 0;

    tail_.store(nextTail, std::memory_order_release);
    processQueue();
    return true;
}

void SmartOrderEngine::connectToGateway(const QString& url) {
    if (socket_) {
        socket_->deleteLater();
        socket_ = nullptr;
    }
    socket_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(socket_, &QWebSocket::connected, this, &SmartOrderEngine::onConnected);
    connect(socket_, &QWebSocket::disconnected, this, &SmartOrderEngine::onDisconnected);
    connect(socket_, &QWebSocket::binaryMessageReceived, this, &SmartOrderEngine::onBinaryMessage);
    socket_->open(url);
}

void SmartOrderEngine::disconnectFromGateway() {
    if (socket_) {
        socket_->close();
        socket_->deleteLater();
        socket_ = nullptr;
    }
}

void SmartOrderEngine::onConnected() {
    qDebug() << "SmartOrderEngine: connected to gateway";
    emit gatewayConnected();
    processQueue();
}

void SmartOrderEngine::onDisconnected() {
    qDebug() << "SmartOrderEngine: disconnected from gateway";
    emit gatewayDisconnected();
}

void SmartOrderEngine::onBinaryMessage(const QByteArray& message) {
    QJsonDocument doc = QJsonDocument::fromJson(message);
    QJsonObject obj;
    if (!doc.isNull() && doc.isObject()) {
        obj = doc.object();
    } else {
        try {
            msgpack::object_handle oh = msgpack::unpack(message.constData(), static_cast<std::size_t>(message.size()));
            std::map<std::string, msgpack::object> fields;
            oh.get().convert(fields);
            auto get_string = [&](const char* key) {
                auto it = fields.find(key);
                if (it == fields.end()) return QString();
                try { return QString::fromStdString(it->second.as<std::string>()); } catch (...) { return QString(); }
            };
            auto get_double = [&](const char* key) {
                auto it = fields.find(key);
                if (it == fields.end()) return 0.0;
                try { return it->second.as<double>(); } catch (...) {
                    try { return static_cast<double>(it->second.as<int64_t>()); } catch (...) { return 0.0; }
                }
            };
            auto get_int = [&](const char* key) {
                auto it = fields.find(key);
                if (it == fields.end()) return 0;
                try { return it->second.as<int>(); } catch (...) { return 0; }
            };
            obj["ticket"] = get_int("ticket");
            obj["status"] = get_string("status");
            obj["error"] = get_string("error");
            obj["client_order_id"] = get_string("client_order_id");
            obj["fill_price"] = get_double("fill_price");
            obj["latency_ms"] = get_double("latency_ms");
        } catch (...) {
            emit orderRejected(QStringLiteral("Invalid order gateway response"));
            return;
        }
    }

    int ticket = obj["ticket"].toInt();
    QString status = obj["status"].toString();

    size_t h = head_.load(std::memory_order_acquire);
    if (h != tail_.load(std::memory_order_acquire)) {
        auto& slot = queue_[h];
        if (status == "SUBMITTED") {
            slot.order->latencyMs = obj["latency_ms"].toDouble();
            emit orderSubmitted(obj["client_order_id"].toString(), slot.order->symbol, slot.order->side,
                                slot.order->volume, slot.order->latencyMs);
        } else if (status == "FILLED") {
            slot.order->ticket = ticket;
            slot.order->fillPrice = obj["fill_price"].toDouble();
            slot.order->latencyMs = obj["latency_ms"].toDouble();
            slot.order->filled.store(true, std::memory_order_release);
            emit orderFilled(ticket, slot.order->symbol, slot.order->side,
                             slot.order->volume, slot.order->fillPrice, slot.order->latencyMs);
        } else if (status == "REJECTED") {
            emit orderRejected(obj["error"].toString());
        }
        slot.order->processed.store(true, std::memory_order_release);
        head_.store((h + 1) % RING_SIZE, std::memory_order_release);
    }
}

void SmartOrderEngine::startZmq() {
    if (g_zmqCtx) return;
    qDebug() << "Starting ZMQ push thread...";
    g_zmqCtx = std::make_unique<zmq::context_t>(1);
    g_zmqPush = std::make_unique<zmq::socket_t>(*g_zmqCtx, ZMQ_PUSH);
    g_zmqPush->connect("tcp://127.0.0.1:5557");
    g_zmqPush->set(zmq::sockopt::sndhwm, 1000);
    g_zmqPush->set(zmq::sockopt::linger, 0);

    std::thread([this]() {
        while (true) {
            size_t h = head_.load(std::memory_order_acquire);
            size_t t = tail_.load(std::memory_order_acquire);
            if (h == t) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
                continue;
            }
            auto& slot = queue_[h];
            if (slot.order->processed.load(std::memory_order_acquire)) {
                head_.store((h + 1) % RING_SIZE, std::memory_order_release);
                continue;
            }

            msgpack::sbuffer sbuf;
            UnifiedOrder uo;
            uo.symbol = slot.order->symbol;
            uo.quantity = slot.order->volume;
            uo.price = 0;
            uo.stop_loss = slot.order->sl;
            uo.take_profit = slot.order->tp;
            uo.side = slot.order->side == "BUY" ? OrderSide::Buy : OrderSide::Sell;
            pack_order(uo, sbuf);

            zmq::message_t msg(sbuf.data(), sbuf.size());
            try {
                g_zmqPush->send(msg, zmq::send_flags::dontwait);
                slot.order->processed.store(true, std::memory_order_release);
                head_.store((h + 1) % RING_SIZE, std::memory_order_release);
            } catch (...) { break; }
        }
    }).detach();
}

void SmartOrderEngine::processQueue() {
    if (!socket_ || socket_->state() != QAbstractSocket::ConnectedState) {
        emit orderRejected(QStringLiteral("Order gateway is not connected"));
        head_.store(tail_.load(std::memory_order_acquire), std::memory_order_release);
        return;
    }

    while (head_.load(std::memory_order_acquire) != tail_.load(std::memory_order_acquire)) {
        size_t h = head_.load(std::memory_order_acquire);
        auto& slot = queue_[h];
        if (slot.order->processed.load(std::memory_order_acquire)) {
            head_.store((h + 1) % RING_SIZE, std::memory_order_release);
            continue;
        }

        msgpack::sbuffer sbuf;
        UnifiedOrder uo;
        uo.symbol = slot.order->symbol;
        uo.quantity = slot.order->volume;
        uo.price = 0;
        uo.stop_loss = slot.order->sl;
        uo.take_profit = slot.order->tp;
        uo.side = slot.order->side == "BUY" ? OrderSide::Buy : OrderSide::Sell;
        pack_order(uo, sbuf);

        socket_->sendBinaryMessage(QByteArray(sbuf.data(), static_cast<int>(sbuf.size())));
        break;
    }
}

// Legacy broker routing (for UnifiedTrading compatibility)
ApiResponse<SmartOrderResult> SmartOrderEngine::execute(IBroker* broker,
                                                         const BrokerCredentials& creds,
                                                         const SmartOrder& order) {
    if (!broker) {
        return {false, std::nullopt, QStringLiteral("No broker provided")};
    }

    UnifiedOrder uo;
    uo.symbol = order.symbol;
    uo.side = order.action;
    uo.quantity = order.quantity;
    uo.order_type = order.order_type;
    uo.product_type = order.product_type;

    auto result = broker->place_order(creds, uo);
    if (result.success) {
        SmartOrderResult sr;
        sr.action_taken = true;
        sr.order_id = result.order_id;
        sr.executed_action = order.action;
        sr.executed_quantity = order.quantity;
        return {true, sr, QString()};
    }
    return {false, std::nullopt, result.error};
}

} // namespace fincept::trading
