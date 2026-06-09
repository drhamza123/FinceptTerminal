// Fincept FIX Protocol Bridge — connects trade server to brokers via FIX.
// Eliminates the MT5 GUI dependency by routing orders directly to FIX endpoints.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <sstream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <algorithm>

// ─── FIX Protocol Constants ─────────────────────────────────────────────────
// FIX 4.4 tag numbers
constexpr int TAG_BEGIN_STRING = 8;
constexpr int TAG_BODY_LENGTH = 9;
constexpr int TAG_MSG_TYPE = 35;
constexpr int TAG_SENDER_COMP = 49;
constexpr int TAG_TARGET_COMP = 56;
constexpr int TAG_MSG_SEQ_NUM = 34;
constexpr int TAG_SENDING_TIME = 52;
constexpr int TAG_SYMBOL = 55;
constexpr int TAG_SIDE = 54;
constexpr int TAG_ORDER_QTY = 38;
constexpr int TAG_ORDER_TYPE = 40;
constexpr int TAG_PRICE = 44;
constexpr int TAG_STOP_PX = 99;
constexpr int TAG_CL_ORD_ID = 11;
constexpr int TAG_ORIG_CL_ORD_ID = 41;
constexpr int TAG_ORDER_ID = 37;
constexpr int TAG_EXEC_ID = 17;
constexpr int TAG_EXEC_TYPE = 150;
constexpr int TAG_ORD_STATUS = 39;
constexpr int TAG_LEAVES_QTY = 151;
constexpr int TAG_CUM_QTY = 14;
constexpr int TAG_AVG_PX = 6;
constexpr int TAG_LAST_PX = 31;
constexpr int TAG_LAST_QTY = 32;
constexpr int TAG_TRANSACT_TIME = 60;

constexpr const char* FIX_VERSION = "FIX.4.4";

enum class FIXSide { BUY = 1, SELL = 2 };
enum class FIXOrderType { MARKET = 1, LIMIT = 2, STOP = 3, STOP_LIMIT = 4 };
enum class FIXOrdStatus { NEW = 0, PARTIAL = 1, FILLED = 2, DONE = 3, CANCELLED = 4, REJECTED = 8 };

// ─── FIX Message ───────────────────────────────────────────────────────────
class FIXMessage {
public:
    std::map<int, std::string> fields;

    void set(int tag, const std::string& val) { fields[tag] = val; }
    std::string get(int tag) const {
        auto it = fields.find(tag);
        return it != fields.end() ? it->second : "";
    }

    std::string encode() const {
        std::string msg;
        for (const auto& [tag, val] : fields)
            msg += std::to_string(tag) + "=" + val + "\x01";
        // Recalculate body length
        std::string body = msg;
        std::string header = std::to_string(TAG_BEGIN_STRING) + "=" + FIX_VERSION + "\x01"
            + std::to_string(TAG_BODY_LENGTH) + "=" + std::to_string(body.size()) + "\x01";
        return header + body;
    }

    static FIXMessage decode(const std::string& raw) {
        FIXMessage msg;
        std::stringstream ss(raw);
        std::string pair;
        while (std::getline(ss, pair, '\x01')) {
            if (pair.empty()) continue;
            auto eq = pair.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(pair.substr(0, eq));
                std::string val = pair.substr(eq + 1);
                msg.fields[tag] = val;
            }
        }
        return msg;
    }
};

// ─── FIX Session ───────────────────────────────────────────────────────────
class FIXSession {
public:
    SOCKET sock = INVALID_SOCKET;
    std::string sender_comp;
    std::string target_comp;
    int seq_num = 1;
    bool logged_on = false;
    std::string host;
    int port = 0;

    bool connect(const std::string& h, int p) {
        host = h; port = p;
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) return false;
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
        if (::connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
            closesocket(sock); sock = INVALID_SOCKET; return false;
        }
        return true;
    }

    bool send_msg(const FIXMessage& msg) {
        std::string raw = msg.encode();
        return send(sock, raw.c_str(), (int)raw.size(), 0) > 0;
    }

    FIXMessage recv_msg() {
        char buffer[8192];
        int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = 0;
            return FIXMessage::decode(std::string(buffer));
        }
        return FIXMessage{};
    }

    bool logon(const std::string& sender, const std::string& target) {
        sender_comp = sender;
        target_comp = target;
        FIXMessage logon;
        logon.set(TAG_BEGIN_STRING, FIX_VERSION);
        logon.set(TAG_MSG_TYPE, "A");
        logon.set(TAG_SENDER_COMP, sender);
        logon.set(TAG_TARGET_COMP, target);
        logon.set(TAG_MSG_SEQ_NUM, std::to_string(seq_num++));
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&t), "%Y%m%d-%H:%M:%S");
        logon.set(TAG_SENDING_TIME, ss.str());
        logon.set(98, "0"); // EncryptMethod = None
        logon.set(108, "30"); // HeartBtInt = 30s

        if (!send_msg(logon)) return false;
        auto resp = recv_msg();
        if (resp.get(TAG_MSG_TYPE) == "A") {
            logged_on = true;
            std::cout << "[FIXBridge] Logged on to " << host << ":" << port << std::endl;
            return true;
        }
        return false;
    }

    FIXMessage send_order(const std::string& symbol, FIXSide side, double qty,
                          FIXOrderType type, double price, double stop) {
        FIXMessage order;
        order.set(TAG_BEGIN_STRING, FIX_VERSION);
        order.set(TAG_MSG_TYPE, "D"); // NewOrderSingle
        order.set(TAG_SENDER_COMP, sender_comp);
        order.set(TAG_TARGET_COMP, target_comp);
        order.set(TAG_MSG_SEQ_NUM, std::to_string(seq_num++));
        order.set(TAG_CL_ORD_ID, "FINCEPT-" + std::to_string(GetTickCount64()));
        order.set(TAG_SYMBOL, symbol);
        order.set(TAG_SIDE, std::to_string((int)side));
        order.set(TAG_ORDER_QTY, std::to_string(qty));
        order.set(TAG_ORDER_TYPE, std::to_string((int)type));
        if (price > 0) order.set(TAG_PRICE, std::to_string(price));
        if (stop > 0) order.set(TAG_STOP_PX, std::to_string(stop));
        order.set(TAG_TRANSACT_TIME, std::to_string(GetTickCount64()));

        if (!send_msg(order)) {
            std::cerr << "[FIXBridge] Failed to send order" << std::endl;
            return FIXMessage{};
        }
        return recv_msg();
    }

    void close() {
        if (sock != INVALID_SOCKET) {
            FIXMessage logout;
            logout.set(TAG_BEGIN_STRING, FIX_VERSION);
            logout.set(TAG_MSG_TYPE, "5");
            send_msg(logout);
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        logged_on = false;
    }
};

// ─── FIX Bridge Server ─────────────────────────────────────────────────────
class FIXBridgeServer {
public:
    std::map<std::string, FIXSession> sessions;
    std::mutex mutex_;

    bool add_session(const std::string& name, const std::string& host, int port,
                     const std::string& sender, const std::string& target) {
        std::lock_guard<std::mutex> lock(mutex_);
        FIXSession session;
        if (!session.connect(host, port)) {
            std::cerr << "[FIXBridge] Cannot connect to " << host << ":" << port << std::endl;
            return false;
        }
        if (!session.logon(sender, target)) {
            std::cerr << "[FIXBridge] Logon failed for " << name << std::endl;
            return false;
        }
        sessions[name] = std::move(session);
        return true;
    }

    bool send_order(const std::string& session_name, const std::string& symbol,
                    FIXSide side, double qty, FIXOrderType type, double price, double stop) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions.find(session_name);
        if (it == sessions.end()) return false;
        auto resp = it->second.send_order(symbol, side, qty, type, price, stop);
        if (resp.get(TAG_MSG_TYPE) == "8") { // ExecutionReport
            std::cout << "[FIXBridge] Order " << resp.get(TAG_CL_ORD_ID)
                      << " status=" << resp.get(TAG_ORD_STATUS)
                      << " qty=" << resp.get(TAG_CUM_QTY)
                      << " avg_px=" << resp.get(TAG_AVG_PX)
                      << std::endl;
            return resp.get(TAG_ORD_STATUS) == "2"; // FILLED
        }
        return false;
    }
};

// ─── TCP Server for Trade Server Integration ───────────────────────────────
void run_tcp_server(FIXBridgeServer& bridge, int port = 5560) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) return;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listen_sock, (sockaddr*)&addr, sizeof(addr)) != 0) { closesocket(listen_sock); return; }
    listen(listen_sock, 10);
    std::cout << "[FIXBridge] Management TCP on port " << port << std::endl;

    char buffer[8192];
    while (true) {
        sockaddr_in client_addr{};
        int client_len = sizeof(client_addr);
        SOCKET client = accept(listen_sock, (sockaddr*)&client_addr, &client_len);
        if (client == INVALID_SOCKET) continue;

        int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = 0;
            std::string cmd(buffer);
            while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r'))
                cmd.pop_back();

            std::string response;

            if (cmd.find("CONNECT|") == 0) {
                // CONNECT|name|host|port|sender|target
                auto parts = std::vector<std::string>();
                std::stringstream ss(cmd);
                std::string item;
                while (std::getline(ss, item, '|')) parts.push_back(item);
                if (parts.size() >= 6) {
                    bool ok = bridge.add_session(parts[1], parts[2], std::stoi(parts[3]), parts[4], parts[5]);
                    response = ok ? "{\"status\":\"connected\"}" : "{\"error\":\"connect failed\"}";
                } else response = "{\"error\":\"invalid CONNECT\"}";
            }
            else if (cmd.find("ORDER|") == 0) {
                // ORDER|session|symbol|side|qty|type|price|stop
                auto parts = std::vector<std::string>();
                std::stringstream ss(cmd);
                std::string item;
                while (std::getline(ss, item, '|')) parts.push_back(item);
                if (parts.size() >= 6) {
                    FIXSide side = parts[3] == "BUY" ? FIXSide::BUY : FIXSide::SELL;
                    FIXOrderType type = parts[5] == "LIMIT" ? FIXOrderType::LIMIT
                        : parts[5] == "STOP" ? FIXOrderType::STOP : FIXOrderType::MARKET;
                    double price = parts.size() > 6 ? std::stod(parts[6]) : 0;
                    double stop = parts.size() > 7 ? std::stod(parts[7]) : 0;
                    bool ok = bridge.send_order(parts[1], parts[2], side, std::stod(parts[4]), type, price, stop);
                    response = ok ? "{\"status\":\"filled\"}" : "{\"error\":\"order failed\"}";
                } else response = "{\"error\":\"invalid ORDER\"}";
            }
            else if (cmd == "STATUS") {
                response = "{\"sessions\":" + std::to_string(bridge.sessions.size()) + "}";
            }
            else response = "{\"error\":\"unknown\"}";

            send(client, response.c_str(), (int)response.size(), 0);
        }
        closesocket(client);
    }
    closesocket(listen_sock);
    WSACleanup();
}

int main() {
    std::cout << "=== Fincept FIX Protocol Bridge ===" << std::endl;
    std::cout << "Version: 1.0.0 | 64-bit | FIX 4.4" << std::endl;
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << std::endl;

    FIXBridgeServer bridge;
    std::thread tcp_thread(run_tcp_server, std::ref(bridge), 5560);
    tcp_thread.detach();

    std::cout << "[FIXBridge] Ready. Management port: 5560" << std::endl;
    std::cout << "[FIXBridge] Connect brokers via: CONNECT|name|host|port|sender|target" << std::endl;

    while (true) {
        Sleep(60000); // Heartbeat every 60s
        std::cout << "[FIXBridge] Heartbeat | Sessions: " << bridge.sessions.size() << std::endl;
    }

    return 0;
}
