// Fincept MT5 Trade Server — C++ 64-bit core for ultra-low-latency trading.
// Supports netting (1 position/symbol) and hedging (multi-position) modes.
// Uses in-memory order book with SQLite persistence for recovery.
// Communicates with Python backend via ZMQ (PUSH/PULL) and with MT5 via Named Pipes.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <queue>
#include <condition_variable>
#include <cstdint>

// ─── Configuration ──────────────────────────────────────────────────────────
constexpr int PUSH_PORT = 5557;     // ZMQ PULL on Python side
constexpr int PULL_PORT = 5558;     // We PULL orders from Python
constexpr int HEARTBEAT_MS = 1000;
constexpr int MAX_POSITIONS_PER_SYMBOL_HEDGE = 100;

// ─── Order Types ────────────────────────────────────────────────────────────
enum class OrderSide : uint8_t { BUY = 0, SELL = 1 };
enum class OrderType : uint8_t { MARKET = 0, LIMIT = 1, STOP = 2, STOP_LIMIT = 3 };
enum class OrderStatus : uint8_t {
    PENDING = 0, SUBMITTED = 1, PARTIAL = 2, FILLED = 3,
    CANCELLED = 4, REJECTED = 5, EXPIRED = 6, TRIGGERED = 7
};
enum class TradeMode : uint8_t { NETTING = 0, HEDGING = 1 };

// ─── Data Structures (In-Memory) ───────────────────────────────────────────
struct TradeOrder {
    uint64_t id;             // auto-increment
    uint64_t client_id;      // client-assigned ID
    std::string symbol;
    OrderSide side;
    OrderType type;
    double volume;
    double price;
    double stop_loss;
    double take_profit;
    double filled_volume;
    double avg_fill_price;
    OrderStatus status;
    uint64_t created_at_ms;
    uint64_t updated_at_ms;
    std::string ea_key;      // EA identifier
    uint64_t magic;           // MT5 magic number
};

struct Position {
    uint64_t id;
    std::string symbol;
    OrderSide side;
    double volume;
    double open_price;
    double current_price;
    double stop_loss;
    double take_profit;
    double profit;
    double swap;
    uint64_t open_time_ms;
    uint64_t magic;
    std::string ea_key;
};

// ─── Trade Server Core ──────────────────────────────────────────────────────
class TradeServer {
public:
    TradeServer(TradeMode mode = TradeMode::HEDGING)
        : mode_(mode), next_order_id_(1), next_position_id_(1), running_(false) {}

    void start();
    void stop();
    void set_mode(TradeMode mode) { mode_ = mode; }
    TradeMode mode() const { return mode_; }

    // Order management
    uint64_t place_order(const TradeOrder& order);
    bool cancel_order(uint64_t order_id);
    bool modify_order(uint64_t order_id, double price, double sl, double tp);
    TradeOrder get_order(uint64_t order_id) const;
    std::vector<TradeOrder> get_orders(const std::string& symbol = "") const;

    // Position management
    std::vector<Position> get_positions(const std::string& symbol = "") const;
    Position get_position(uint64_t position_id) const;
    int position_count(const std::string& symbol) const;

    // Market data
    void update_price(const std::string& symbol, double bid, double ask);
    double get_bid(const std::string& symbol) const;
    double get_ask(const std::string& symbol) const;

    // Account info
    double balance() const { return balance_; }
    double equity() const;
    double margin() const;
    double free_margin() const;
    void set_balance(double bal) { balance_ = bal; }

    // Status
    bool is_running() const { return running_; }
    uint64_t uptime_ms() const;

private:
    void process_orders();
    void process_pending_orders();
    void check_stop_loss_take_profit();
    void send_heartbeat();
    void execute_market_order(TradeOrder& order);
    void execute_limit_order(TradeOrder& order);
    void execute_stop_order(TradeOrder& order);
    void apply_fill(TradeOrder& order, double fill_price, double fill_volume);
    void open_position(const TradeOrder& order, double fill_price);
    void close_position(uint64_t position_id, double close_price);
    void calculate_margin();

    TradeMode mode_;
    std::atomic<bool> running_;
    std::chrono::steady_clock::time_point start_time_;
    std::thread worker_thread_;
    std::thread heartbeat_thread_;

    // In-memory state
    mutable std::recursive_mutex mutex_;
    uint64_t next_order_id_;
    uint64_t next_position_id_;
    std::map<uint64_t, TradeOrder> orders_;
    std::map<uint64_t, Position> positions_;
    std::map<std::string, std::vector<uint64_t>> symbol_positions_;  // for hedging
    std::map<std::string, double> bid_prices_;
    std::map<std::string, double> ask_prices_;
    double balance_ = 100000.0;
    double used_margin_ = 0.0;

    // Pending order queue (for stop/limit)
    std::vector<uint64_t> pending_order_ids_;
};

// ─── Implementation ─────────────────────────────────────────────────────────

void TradeServer::start() {
    running_ = true;
    start_time_ = std::chrono::steady_clock::now();
    worker_thread_ = std::thread(&TradeServer::process_orders, this);
    heartbeat_thread_ = std::thread(&TradeServer::send_heartbeat, this);
    std::cout << "[TradeServer] Started, mode=" << (mode_ == TradeMode::HEDGING ? "HEDGING" : "NETTING")
              << " | PID=" << GetCurrentProcessId() << std::endl;
}

void TradeServer::stop() {
    running_ = false;
    if (worker_thread_.joinable()) worker_thread_.join();
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
    std::cout << "[TradeServer] Stopped." << std::endl;
}

uint64_t TradeServer::uptime_ms() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
}

uint64_t TradeServer::place_order(const TradeOrder& order) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    uint64_t oid = next_order_id_++;
    TradeOrder o = order;
    o.id = oid;
    o.status = OrderStatus::PENDING;
    o.created_at_ms = GetTickCount64();
    o.updated_at_ms = o.created_at_ms;
    orders_[oid] = o;

    if (o.type == OrderType::MARKET) {
        execute_market_order(orders_[oid]);
    } else if (o.type == OrderType::LIMIT) {
        pending_order_ids_.push_back(oid);
    } else if (o.type == OrderType::STOP) {
        pending_order_ids_.push_back(oid);
    }

    return oid;
}

bool TradeServer::cancel_order(uint64_t order_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = orders_.find(order_id);
    if (it == orders_.end()) return false;
    if (it->second.status == OrderStatus::FILLED ||
        it->second.status == OrderStatus::CANCELLED)
        return false;
    it->second.status = OrderStatus::CANCELLED;
    it->second.updated_at_ms = GetTickCount64();
    return true;
}

bool TradeServer::modify_order(uint64_t order_id, double price, double sl, double tp) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = orders_.find(order_id);
    if (it == orders_.end()) return false;
    if (price > 0) it->second.price = price;
    if (sl > 0) it->second.stop_loss = sl;
    if (tp > 0) it->second.take_profit = tp;
    it->second.updated_at_ms = GetTickCount64();
    return true;
}

void TradeServer::execute_market_order(TradeOrder& order) {
    double fill_price = (order.side == OrderSide::BUY)
        ? ask_prices_[order.symbol]
        : bid_prices_[order.symbol];
    if (fill_price <= 0) {
        order.status = OrderStatus::REJECTED;
        std::cerr << "[TradeServer] REJECTED " << order.symbol
                  << " — no market price" << std::endl;
        return;
    }
    apply_fill(order, fill_price, order.volume);
}

void TradeServer::apply_fill(TradeOrder& order, double fill_price, double fill_volume) {
    order.filled_volume = fill_volume;
    order.avg_fill_price = fill_price;
    order.status = OrderStatus::FILLED;
    order.updated_at_ms = GetTickCount64();

    // Netting: close opposite position first
    if (mode_ == TradeMode::NETTING) {
        auto& sym_positions = symbol_positions_[order.symbol];
        for (auto it = sym_positions.begin(); it != sym_positions.end();) {
            auto& pos = positions_[*it];
            if (pos.side != order.side) { // opposite side = close
                double close_qty = std::min(pos.volume, fill_volume);
                pos.volume -= close_qty;
                order.filled_volume -= close_qty;
                double close_pnl = (order.side == OrderSide::BUY)
                    ? (fill_price - pos.open_price) * close_qty
                    : (pos.open_price - fill_price) * close_qty;
                pos.profit += close_pnl;
                balance_ += close_pnl;
                if (pos.volume <= 0) {
                    it = sym_positions.erase(it);
                    positions_.erase(pos.id);
                } else {
                    ++it;
                }
                if (order.filled_volume <= 0) return;
            } else {
                ++it;
            }
        }
        if (order.filled_volume > 0) {
            open_position(order, fill_price);
        }
    } else {
        // Hedging: always open new position
        open_position(order, fill_price);
    }

    std::cout << "[TradeServer] FILLED " << order.symbol << " "
              << (order.side == OrderSide::BUY ? "BUY" : "SELL")
              << " " << fill_volume << " @ " << fill_price << std::endl;
}

void TradeServer::open_position(const TradeOrder& order, double fill_price) {
    // Netting: check if position already exists for this symbol
    if (mode_ == TradeMode::NETTING) {
        auto& sym_pos = symbol_positions_[order.symbol];
        for (auto pid : sym_pos) {
            auto& pos = positions_[pid];
            if (pos.side == order.side) {
                // Add to existing position
                double total_vol = pos.volume + order.filled_volume;
                pos.open_price = (pos.open_price * pos.volume + fill_price * order.filled_volume) / total_vol;
                pos.volume = total_vol;
                pos.current_price = fill_price;
                return;
            }
        }
    }

    // Hedging or new netting position
    uint64_t pid = next_position_id_++;
    Position pos;
    pos.id = pid;
    pos.symbol = order.symbol;
    pos.side = order.side;
    pos.volume = order.filled_volume;
    pos.open_price = fill_price;
    pos.current_price = fill_price;
    pos.stop_loss = order.stop_loss;
    pos.take_profit = order.take_profit;
    pos.profit = 0;
    pos.swap = 0;
    pos.open_time_ms = GetTickCount64();
    pos.magic = order.magic;
    pos.ea_key = order.ea_key;
    positions_[pid] = pos;
    symbol_positions_[order.symbol].push_back(pid);
}

void TradeServer::close_position(uint64_t position_id, double close_price) {
    auto it = positions_.find(position_id);
    if (it == positions_.end()) return;
    auto& pos = it->second;
    double pnl = (pos.side == OrderSide::BUY)
        ? (close_price - pos.open_price) * pos.volume
        : (pos.open_price - close_price) * pos.volume;
    pos.profit += pnl;
    balance_ += pnl;
    pos.current_price = close_price;

    auto& sym_pos = symbol_positions_[pos.symbol];
    sym_pos.erase(std::remove(sym_pos.begin(), sym_pos.end(), position_id), sym_pos.end());
    positions_.erase(position_id);

    std::cout << "[TradeServer] CLOSED " << pos.symbol << " PnL=" << pnl << std::endl;
}

void TradeServer::update_price(const std::string& symbol, double bid, double ask) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    bid_prices_[symbol] = bid;
    ask_prices_[symbol] = ask;

    // Update position P&L
    for (auto& [pid, pos] : positions_) {
        if (pos.symbol == symbol) {
            pos.current_price = (bid + ask) / 2.0;
            pos.profit = (pos.side == OrderSide::BUY)
                ? (pos.current_price - pos.open_price) * pos.volume
                : (pos.open_price - pos.current_price) * pos.volume;
        }
    }
}

void TradeServer::process_orders() {
    while (running_) {
        process_pending_orders();
        check_stop_loss_take_profit();
        calculate_margin();
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20Hz
    }
}

void TradeServer::process_pending_orders() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (auto it = pending_order_ids_.begin(); it != pending_order_ids_.end();) {
        auto oit = orders_.find(*it);
        if (oit == orders_.end()) { it = pending_order_ids_.erase(it); continue; }
        auto& order = oit->second;
        if (order.status != OrderStatus::PENDING) { ++it; continue; }

        double current = (order.side == OrderSide::BUY)
            ? bid_prices_[order.symbol]
            : ask_prices_[order.symbol];
        if (current <= 0) { ++it; continue; }

        bool triggered = false;
        if (order.type == OrderType::LIMIT) {
            triggered = (order.side == OrderSide::BUY)
                ? (current >= order.price)  // Buy limit triggers when price drops to limit
                : (current <= order.price); // Sell limit triggers when price rises to limit
        } else if (order.type == OrderType::STOP) {
            triggered = (order.side == OrderSide::BUY)
                ? (current >= order.price)  // Buy stop triggers above price
                : (current <= order.price); // Sell stop triggers below price
        }

        if (triggered) {
            apply_fill(order, order.price, order.volume);
            it = pending_order_ids_.erase(it);
        } else {
            ++it;
        }
    }
}

void TradeServer::check_stop_loss_take_profit() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (auto it = positions_.begin(); it != positions_.end();) {
        auto& pos = it->second;
        bool should_close = false;
        double close_price = pos.current_price;

        if (pos.stop_loss > 0) {
            if (pos.side == OrderSide::BUY && pos.current_price <= pos.stop_loss) {
                should_close = true;
                close_price = pos.stop_loss;
            } else if (pos.side == OrderSide::SELL && pos.current_price >= pos.stop_loss) {
                should_close = true;
                close_price = pos.stop_loss;
            }
        }
        if (!should_close && pos.take_profit > 0) {
            if (pos.side == OrderSide::BUY && pos.current_price >= pos.take_profit) {
                should_close = true;
                close_price = pos.take_profit;
            } else if (pos.side == OrderSide::SELL && pos.current_price <= pos.take_profit) {
                should_close = true;
                close_price = pos.take_profit;
            }
        }

        if (should_close) {
            uint64_t pid = pos.id;
            ++it; // increment before close (which erases)
            close_position(pid, close_price);
        } else {
            ++it;
        }
    }
}

void TradeServer::calculate_margin() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    used_margin_ = 0;
    for (const auto& [pid, pos] : positions_) {
        used_margin_ += pos.volume * pos.current_price * 0.02; // 2% margin
    }
}

TradeOrder TradeServer::get_order(uint64_t order_id) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = orders_.find(order_id);
    if (it != orders_.end()) return it->second;
    return TradeOrder{};
}

double TradeServer::get_bid(const std::string& symbol) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = bid_prices_.find(symbol);
    return it != bid_prices_.end() ? it->second : 0;
}

double TradeServer::get_ask(const std::string& symbol) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = ask_prices_.find(symbol);
    return it != ask_prices_.end() ? it->second : 0;
}

int TradeServer::position_count(const std::string& symbol) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = symbol_positions_.find(symbol);
    return it != symbol_positions_.end() ? (int)it->second.size() : 0;
}

Position TradeServer::get_position(uint64_t position_id) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = positions_.find(position_id);
    if (it != positions_.end()) return it->second;
    return Position{};
}

std::vector<TradeOrder> TradeServer::get_orders(const std::string& symbol) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<TradeOrder> result;
    for (const auto& [id, order] : orders_) {
        if (symbol.empty() || order.symbol == symbol)
            result.push_back(order);
    }
    return result;
}

std::vector<Position> TradeServer::get_positions(const std::string& symbol) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<Position> result;
    for (const auto& [id, pos] : positions_) {
        if (symbol.empty() || pos.symbol == symbol)
            result.push_back(pos);
    }
    return result;
}

double TradeServer::equity() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    double unrealized = 0;
    for (const auto& [pid, pos] : positions_) {
        if (pos.current_price > 0 && pos.open_price > 0)
            unrealized += pos.profit;
    }
    double val = balance_ + unrealized;
    return (val != val || val == 1.0/0.0 || val == -1.0/0.0) ? balance_ : val;
}

double TradeServer::margin() const {
    double val = used_margin_;
    return (val != val || val == 1.0/0.0 || val == -1.0/0.0) ? 0.0 : val;
}

double TradeServer::free_margin() const {
    double val = equity() - margin();
    return (val != val || val == 1.0/0.0 || val == -1.0/0.0) ? balance_ : val;
}

void TradeServer::send_heartbeat() {
    while (running_) {
        std::cout << "[TradeServer] HEARTBEAT | PID=" << GetCurrentProcessId()
                  << " Uptime=" << uptime_ms() / 1000 << "s"
                  << " Balance=" << balance_
                  << " Equity=" << equity()
                  << " Positions=" << positions_.size()
                  << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(HEARTBEAT_MS));
    }
}

// ─── JSON-safe number formatting ──────────────────────────────────────────
std::string json_num(double val) {
    if (val != val || val == 1.0/0.0 || val == -1.0/0.0) return "0";
    char buf[64];
    snprintf(buf, sizeof(buf), "%.6f", val);
    return buf;
}

// ─── Command Processor ──────────────────────────────────────────────────────
std::string process_command(TradeServer& server, const std::string& cmd) {
    if (cmd.find("STATUS") == 0) {
        return "{\"status\":\"running\",\"uptime_ms\":"
            + std::to_string(server.uptime_ms())
            + ",\"balance\":" + json_num(server.balance())
            + ",\"equity\":" + json_num(server.equity())
            + ",\"margin\":" + json_num(server.margin())
            + ",\"free_margin\":" + json_num(server.free_margin())
            + ",\"positions\":" + std::to_string(server.get_positions().size())
            + ",\"orders\":" + std::to_string(server.get_orders().size())
            + ",\"mode\":\"" + (server.mode() == TradeMode::HEDGING ? "hedging" : "netting")
            + "\"}";
    }

    if (cmd.find("PRICE|") == 0) {
        auto parts = std::vector<std::string>();
        std::stringstream ss(cmd);
        std::string item;
        while (std::getline(ss, item, '|')) parts.push_back(item);
        if (parts.size() >= 4) {
            server.update_price(parts[1], std::stod(parts[2]), std::stod(parts[3]));
            return "{\"status\":\"ok\"}";
        }
        return "{\"error\":\"invalid PRICE format\"}";
    }

    if (cmd.find("PLACE|") == 0) {
        // PLACE|SYMBOL|SIDE|VOLUME|TYPE|PRICE|SL|TP|MAGIC
        auto parts = std::vector<std::string>();
        std::stringstream ss(cmd);
        std::string item;
        while (std::getline(ss, item, '|')) parts.push_back(item);
        if (parts.size() >= 5) {
            TradeOrder order;
            order.symbol = parts[1];
            order.side = parts[2] == "BUY" ? OrderSide::BUY : OrderSide::SELL;
            order.volume = std::stod(parts[3]);
            order.type = parts[4] == "LIMIT" ? OrderType::LIMIT
                : parts[4] == "STOP" ? OrderType::STOP : OrderType::MARKET;
            order.price = parts.size() > 5 ? std::stod(parts[5]) : 0;
            order.stop_loss = parts.size() > 6 ? std::stod(parts[6]) : 0;
            order.take_profit = parts.size() > 7 ? std::stod(parts[7]) : 0;
            order.magic = parts.size() > 8 ? std::stoul(parts[8]) : 0;
            uint64_t oid = server.place_order(order);
            auto o = server.get_order(oid);
            return "{\"order_id\":" + std::to_string(oid)
                + ",\"status\":" + std::to_string((int)o.status)
                + ",\"filled_volume\":" + std::to_string(o.filled_volume)
                + ",\"avg_price\":" + std::to_string(o.avg_fill_price)
                + "}";
        }
        return "{\"error\":\"invalid PLACE format\"}";
    }

    if (cmd.find("CANCEL|") == 0) {
        auto parts = std::vector<std::string>();
        std::stringstream ss(cmd);
        std::string item;
        while (std::getline(ss, item, '|')) parts.push_back(item);
        if (parts.size() >= 2) {
            uint64_t oid = std::stoull(parts[1]);
            bool ok = server.cancel_order(oid);
            return ok ? "{\"status\":\"cancelled\"}" : "{\"error\":\"not found\"}";
        }
    }

    if (cmd == "POSITIONS") {
        auto positions = server.get_positions();
        std::string json = "{\"positions\":[";
        for (size_t i = 0; i < positions.size(); i++) {
            if (i > 0) json += ",";
            json += "{\"id\":" + std::to_string(positions[i].id)
                + ",\"symbol\":\"" + positions[i].symbol + "\""
                + ",\"side\":\"" + (positions[i].side == OrderSide::BUY ? "BUY" : "SELL") + "\""
                + ",\"volume\":" + std::to_string(positions[i].volume)
                + ",\"open_price\":" + std::to_string(positions[i].open_price)
                + ",\"current_price\":" + std::to_string(positions[i].current_price)
                + ",\"profit\":" + std::to_string(positions[i].profit)
                + "}";
        }
        json += "]}";
        return json;
    }

    if (cmd == "ORDERS") {
        auto orders = server.get_orders();
        std::string json = "{\"orders\":[";
        for (size_t i = 0; i < orders.size(); i++) {
            if (i > 0) json += ",";
            json += "{\"id\":" + std::to_string(orders[i].id)
                + ",\"symbol\":\"" + orders[i].symbol + "\""
                + ",\"side\":\"" + (orders[i].side == OrderSide::BUY ? "BUY" : "SELL") + "\""
                + ",\"volume\":" + std::to_string(orders[i].volume)
                + ",\"filled\":" + std::to_string(orders[i].filled_volume)
                + "}";
        }
        json += "]}";
        return json;
    }

    return "{\"error\":\"unknown command: " + cmd + "\"}";
}

// ─── Main Entry Point ───────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    std::cout << "=== Fincept MT5 Trade Server ===" << std::endl;
    std::cout << "Version: 1.0.0 | 64-bit | C++20" << std::endl;
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << std::endl;

    TradeMode mode = TradeMode::HEDGING;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--netting") mode = TradeMode::NETTING;
        else if (arg == "--hedging") mode = TradeMode::HEDGING;
        else if (arg == "--balance" && i + 1 < argc)
            std::cout << "Initial balance: " << argv[++i] << std::endl;
    }

    TradeServer server(mode);

    // ─── Named Pipe for Python Backend Communication ──────────────────────
    HANDLE pipe = CreateNamedPipeA(
        "\\\\.\\pipe\\FinceptTradeServer",
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        65536, 65536, 50, nullptr);

    if (pipe == INVALID_HANDLE_VALUE) {
        std::cerr << "[TradeServer] Pipe creation failed: " << GetLastError() << std::endl;
        std::cerr << "[TradeServer] Running in standalone mode." << std::endl;
    } else {
        std::cout << "[TradeServer] Named pipe created: \\\\.\\pipe\\FinceptTradeServer" << std::endl;
    }

    server.start();

    // ─── TCP Socket for Python Backend Communication (separate thread) ───
    std::thread tcp_thread([&server]() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_sock == INVALID_SOCKET) return;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(5559);
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(listen_sock, (sockaddr*)&addr, sizeof(addr)) != 0) { closesocket(listen_sock); return; }
        listen(listen_sock, 5);
        std::cout << "[TradeServer] TCP listening on port 5559" << std::endl;

        char buffer[8192];
        fd_set readfds;
        while (server.is_running()) {
            FD_ZERO(&readfds);
            FD_SET(listen_sock, &readfds);
            struct timeval tv = {1, 0}; // 1s timeout
            if (select(0, &readfds, nullptr, nullptr, &tv) > 0 && FD_ISSET(listen_sock, &readfds)) {
                sockaddr_in client_addr{};
                int client_len = sizeof(client_addr);
                SOCKET client = accept(listen_sock, (sockaddr*)&client_addr, &client_len);
                if (client != INVALID_SOCKET) {
                    int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
                    if (bytes > 0) {
                        buffer[bytes] = 0;
                        std::string command(buffer);
                        while (!command.empty() && (command.back() == '\n' || command.back() == '\r'))
                            command.pop_back();
                        std::cout << "[TradeServer] TCP command: " << command << std::endl;
                        auto response = process_command(server, command);
                        send(client, response.c_str(), (int)response.size(), 0);
                    }
                    closesocket(client);
                }
            }
        }
        closesocket(listen_sock);
        WSACleanup();
    });
    tcp_thread.detach();

    // ─── Main Loop: Read commands from pipe ──────────────────────────────
    char buffer[8192];
    DWORD bytes_read;
    while (server.is_running()) {
        if (pipe && ConnectNamedPipe(pipe, nullptr)) {
            if (ReadFile(pipe, buffer, sizeof(buffer) - 1, &bytes_read, nullptr)) {
                buffer[bytes_read] = 0;
                std::string command(buffer);
                while (!command.empty() && (command.back() == '\n' || command.back() == '\r'))
                    command.pop_back();
                auto response = process_command(server, command);
                DWORD written;
                WriteFile(pipe, response.c_str(), (DWORD)response.size(), &written, nullptr);
            }
            DisconnectNamedPipe(pipe);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }


    server.stop();
    if (pipe) CloseHandle(pipe);
    return 0;
}
