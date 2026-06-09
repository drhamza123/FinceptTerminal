// Fincept Access Server — TCP proxy/load balancer for trade infrastructure.
// Routes client connections to backend services with rate limiting.
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
#include <chrono>

// ─── Rate Limiter ──────────────────────────────────────────────────────────
class RateLimiter {
public:
    std::map<std::string, std::pair<uint64_t, int>> clients_; // ip -> (window_start, count)
    std::mutex mutex_;
    int max_requests_per_second = 10;

    bool allow(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = GetTickCount64();
        auto& entry = clients_[ip];
        if (now - entry.first > 1000) {
            entry.first = now;
            entry.second = 1;
            return true;
        }
        entry.second++;
        return entry.second <= max_requests_per_second;
    }
};

// ─── Backend Router ────────────────────────────────────────────────────────
struct BackendService {
    std::string host;
    int port;
};

class BackendRouter {
public:
    std::map<std::string, std::vector<BackendService>> services_;
    std::mutex mutex_;

    void add_service(const std::string& name, const std::string& host, int port) {
        std::lock_guard<std::mutex> lock(mutex_);
        services_[name].push_back({host, port});
    }

    BackendService get_service(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& list = services_[name];
        if (list.empty()) return {"", 0};
        static size_t round_robin = 0;
        return list[round_robin++ % list.size()];
    }
};

// ─── Access Server ─────────────────────────────────────────────────────────
class AccessServer {
public:
    RateLimiter limiter_;
    BackendRouter router_;
    int listen_port_ = 5562;

    void route_request(SOCKET client, const std::string& client_ip, const std::string& data) {
        // Parse first token to determine backend
        std::string backend = "trade"; // default to trade server on 5559
        if (data.find("BARS|") == 0 || data.find("TICK|") == 0 || data.find("STATS") == 0)
            backend = "history"; // history server on 5561
        else if (data.find("CONNECT|") == 0 || data.find("ORDER|") == 0)
            backend = "fix"; // fix bridge on 5560

        auto svc = router_.get_service(backend);
        if (svc.port == 0) {
            std::string err = "{\"error\":\"backend " + backend + " not available\"}";
            send(client, err.c_str(), (int)err.size(), 0);
            return;
        }

        // Forward to backend
        SOCKET backend_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (backend_sock == INVALID_SOCKET) return;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(svc.port);
        inet_pton(AF_INET, svc.host.c_str(), &addr.sin_addr);

        if (connect(backend_sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
            send(backend_sock, data.c_str(), (int)data.size(), 0);

            char response[8192];
            int bytes = recv(backend_sock, response, sizeof(response) - 1, 0);
            if (bytes > 0) {
                response[bytes] = 0;
                send(client, response, bytes, 0);
            }
            closesocket(backend_sock);
        } else {
            std::string err = "{\"error\":\"backend " + backend + " on " + svc.host + ":" + std::to_string(svc.port) + " refused\"}";
            send(client, err.c_str(), (int)err.size(), 0);
        }
    }

    void start() {
        // Register backend services
        router_.add_service("trade", "127.0.0.1", 5559);
        router_.add_service("fix", "127.0.0.1", 5560);
        router_.add_service("history", "127.0.0.1", 5561);

        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_sock == INVALID_SOCKET) return;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(listen_port_);
        addr.sin_addr.s_addr = INADDR_ANY;
        bind(listen_sock, (sockaddr*)&addr, sizeof(addr));
        listen(listen_sock, 50);
        std::cout << "[AccessServer] Listening on port " << listen_port_ << std::endl;
        std::cout << "[AccessServer] Routes: trade:5559, fix:5560, history:5561" << std::endl;

        char buffer[8192];
        while (true) {
            sockaddr_in client_addr{};
            int client_len = sizeof(client_addr);
            SOCKET client = accept(listen_sock, (sockaddr*)&client_addr, &client_len);
            if (client == INVALID_SOCKET) continue;

            char client_ip[64];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

            if (!limiter_.allow(client_ip)) {
                std::string err = "{\"error\":\"rate limited\"}";
                send(client, err.c_str(), (int)err.size(), 0);
                closesocket(client);
                continue;
            }

            int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
            if (bytes > 0) {
                buffer[bytes] = 0;
                std::string data(buffer);
                while (!data.empty() && (data.back() == '\n' || data.back() == '\r'))
                    data.pop_back();
                route_request(client, client_ip, data);
            }
            closesocket(client);
        }
        closesocket(listen_sock);
        WSACleanup();
    }
};

int main() {
    std::cout << "=== Fincept Access Server ===" << std::endl;
    std::cout << "TCP proxy / load balancer / rate limiter" << std::endl;
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << std::endl;

    AccessServer server;
    server.start();

    return 0;
}
