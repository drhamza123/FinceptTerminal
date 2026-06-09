#pragma once
#include "trading/TradingTypes.h"
#include <msgpack.hpp>
#include <QDateTime>

namespace fincept::trading {

inline std::string side_to_string(OrderSide side) {
    return side == OrderSide::Buy ? "BUY" : "SELL";
}

inline std::string type_to_string(OrderType type) {
    switch (type) {
        case OrderType::Limit: return "LIMIT";
        case OrderType::StopLoss: return "STOP";
        case OrderType::StopLossLimit: return "STOP_LIMIT";
        default: return "MARKET";
    }
}

inline void pack_order(const UnifiedOrder& order, msgpack::sbuffer& sbuf) {
    msgpack::packer<msgpack::sbuffer> pk(&sbuf);
    pk.pack_map(12);
    pk.pack("symbol");      pk.pack(order.symbol.toStdString());
    pk.pack("side");        pk.pack(side_to_string(order.side));
    pk.pack("type");        pk.pack(type_to_string(order.order_type));
    pk.pack("tif");         pk.pack("GTC");
    pk.pack("volume");      pk.pack(order.quantity);
    pk.pack("price");       pk.pack(order.price);
    pk.pack("sl");          pk.pack(order.stop_loss);
    pk.pack("tp");          pk.pack(order.take_profit);
    pk.pack("stop_loss");   pk.pack(order.stop_loss);
    pk.pack("take_profit"); pk.pack(order.take_profit);
    pk.pack("comment");     pk.pack("FinceptAI");
    pk.pack("ts");          pk.pack(QDateTime::currentMSecsSinceEpoch() / 1000.0);
}

} // namespace fincept::trading
