#pragma once
#include <atomic>
#include <vector>
#include <optional>
#include <functional>
#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include "trading/TradingTypes.h"
#include "trading/BrokerInterface.h"

namespace fincept::trading {

struct FastOrder {
    QString symbol;
    QString side;
    double volume = 0;
    double sl = 0;
    double tp = 0;
    std::atomic<bool> processed{false};
    std::atomic<bool> filled{false};
    int ticket = 0;
    double fillPrice = 0;
    double latencyMs = 0;
};

class SmartOrderEngine : public QObject {
    Q_OBJECT
  public:
    static SmartOrderEngine& instance();
    explicit SmartOrderEngine(QObject* parent = nullptr);
    ~SmartOrderEngine() override;

    // Low-latency path (WS binary)
    bool submitOrder(const QString& symbol, const QString& side,
                     double volume, double sl = 0, double tp = 0);
    void connectToGateway(const QString& url = "ws://localhost:8150/ws/orders");
    void disconnectFromGateway();

    // Legacy broker path (for UnifiedTrading compatibility)
    ApiResponse<SmartOrderResult> execute(IBroker* broker,
                                           const BrokerCredentials& creds,
                                           const SmartOrder& order);

  signals:
    void orderFilled(int ticket, const QString& symbol, const QString& side,
                     double volume, double fillPrice, double latencyMs);
    void orderRejected(const QString& reason);
    void gatewayConnected();
    void gatewayDisconnected();

  private slots:
    void onConnected();
    void onDisconnected();
    void onBinaryMessage(const QByteArray& message);

  private:
    static constexpr size_t RING_SIZE = 1024;
    struct RingSlot {
        FastOrder* order = nullptr;
        RingSlot() { order = new FastOrder(); }
        ~RingSlot() { delete order; order = nullptr; }
        RingSlot(RingSlot&& other) noexcept : order(other.order) { other.order = nullptr; }
        RingSlot& operator=(RingSlot&& other) noexcept {
            if (this != &other) { delete order; order = other.order; other.order = nullptr; }
            return *this;
        }
        RingSlot(const RingSlot&) = delete;
        RingSlot& operator=(const RingSlot&) = delete;
    };
    std::vector<RingSlot> queue_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
    QWebSocket* socket_ = nullptr;
    void processQueue();
    void startZmq();
};

} // namespace fincept::trading
