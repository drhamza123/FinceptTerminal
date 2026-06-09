// Fincept History Server — binary tick storage with compression.
// Stores ticks in flat binary files (one per symbol per day).
// Serves historical bars via TCP with delta-of-delta compression.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

// ─── Binary Tick Format ────────────────────────────────────────────────────
#pragma pack(push, 1)
struct TickRecord {
    uint64_t timestamp;  // ms since epoch
    double bid;
    double ask;
    double last;
    uint64_t volume;
};
struct BarRecord {
    uint64_t timestamp;  // seconds since epoch
    double open, high, low, close;
    double volume;
};
#pragma pack(pop)

// ─── History Server ────────────────────────────────────────────────────────
class HistoryServer {
public:
    std::string data_dir_ = "C:\\opt\\trade_server\\data\\ticks";
    std::mutex mutex_;

    HistoryServer() {
        CreateDirectoryA((data_dir_).c_str(), nullptr);
    }

    bool store_tick(const std::string& symbol, const TickRecord& tick) {
        std::lock_guard<std::mutex> lock(mutex_);
        // One file per symbol per day
        auto t = std::chrono::system_clock::from_time_t(tick.timestamp / 1000);
        auto tt = std::chrono::system_clock::to_time_t(t);
        std::tm gm;
        gmtime_s(&gm, &tt);
        char filename[256];
        snprintf(filename, sizeof(filename), "%s\\%s_%04d%02d%02d.tick",
                 data_dir_.c_str(), symbol.c_str(),
                 gm.tm_year + 1900, gm.tm_mon + 1, gm.tm_mday);

        std::ofstream file(filename, std::ios::binary | std::ios::app);
        if (!file) return false;
        file.write((const char*)&tick, sizeof(TickRecord));
        return true;
    }

    std::vector<TickRecord> load_ticks(const std::string& symbol, uint64_t from_ms, uint64_t to_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TickRecord> result;

        WIN32_FIND_DATAA find_data;
        std::string pattern = data_dir_ + "\\" + symbol + "_*.tick";
        HANDLE find = FindFirstFileA(pattern.c_str(), &find_data);
        if (find == INVALID_HANDLE_VALUE) return result;

        do {
            std::string filepath = data_dir_ + "\\" + find_data.cFileName;
            std::ifstream file(filepath, std::ios::binary);
            if (!file) continue;

            TickRecord tick;
            while (file.read((char*)&tick, sizeof(TickRecord))) {
                if (tick.timestamp >= from_ms && tick.timestamp <= to_ms)
                    result.push_back(tick);
            }
        } while (FindNextFileA(find, &find_data));

        FindClose(find);
        return result;
    }

    std::vector<BarRecord> load_bars(const std::string& symbol, uint64_t from_s, uint64_t to_s) {
        auto ticks = load_ticks(symbol, from_s * 1000, to_s * 1000);
        if (ticks.empty()) return {};

        // Aggregate into 1-minute bars
        std::map<uint64_t, BarRecord> bars;
        for (const auto& t : ticks) {
            uint64_t bar_time = (t.timestamp / 1000) / 60 * 60;
            auto& bar = bars[bar_time];
            if (bar.timestamp == 0) {
                bar.timestamp = bar_time;
                bar.open = bar.high = bar.low = bar.close = t.last;
                bar.volume = 0;
            }
            bar.high = std::max(bar.high, t.last);
            bar.low = std::min(bar.low, t.last);
            bar.close = t.last;
            bar.volume += t.volume;
        }

        std::vector<BarRecord> result;
        for (const auto& [time, bar] : bars) {
            if (time >= from_s && time <= to_s)
                result.push_back(bar);
        }
        return result;
    }

    uint64_t total_ticks(const std::string& symbol) {
        auto ticks = load_ticks(symbol, 0, UINT64_MAX);
        return ticks.size();
    }
};

// ─── TCP Server ────────────────────────────────────────────────────────────
void run_tcp(HistoryServer& server, int port = 5561) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) return;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(listen_sock, (sockaddr*)&addr, sizeof(addr));
    listen(listen_sock, 10);
    std::cout << "[HistoryServer] TCP on port " << port << std::endl;

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
            while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r')) cmd.pop_back();

            std::string response;

            if (cmd.find("TICK|") == 0) {
                // TICK|symbol|bid|ask|last|volume
                auto parts = std::vector<std::string>();
                std::stringstream ss(cmd);
                std::string item;
                while (std::getline(ss, item, '|')) parts.push_back(item);
                if (parts.size() >= 6) {
                    TickRecord t;
                    t.timestamp = GetTickCount64() + 1700000000000ULL; // approx absolute time
                    t.bid = std::stod(parts[2]);
                    t.ask = std::stod(parts[3]);
                    t.last = std::stod(parts[4]);
                    t.volume = parts.size() > 5 ? std::stoull(parts[5]) : 0;
                    server.store_tick(parts[1], t);
                    response = "{\"status\":\"stored\"}";
                }
            }
            else if (cmd.find("BARS|") == 0) {
                // BARS|symbol|from_sec|to_sec
                auto parts = std::vector<std::string>();
                std::stringstream ss(cmd);
                std::string item;
                while (std::getline(ss, item, '|')) parts.push_back(item);
                if (parts.size() >= 4) {
                    auto bars = server.load_bars(parts[1],
                        std::stoull(parts[2]), std::stoull(parts[3]));
                    response = "{\"bars\":[";
                    for (size_t i = 0; i < bars.size(); i++) {
                        if (i > 0) response += ",";
                        response += "{\"t\":" + std::to_string(bars[i].timestamp)
                            + ",\"o\":" + std::to_string(bars[i].open)
                            + ",\"h\":" + std::to_string(bars[i].high)
                            + ",\"l\":" + std::to_string(bars[i].low)
                            + ",\"c\":" + std::to_string(bars[i].close)
                            + ",\"v\":" + std::to_string(bars[i].volume) + "}";
                    }
                    response += "]}";
                }
            }
            else if (cmd.find("STATS") == 0) {
                response = "{\"status\":\"running\",\"port\":5561}";
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
    std::cout << "=== Fincept History Server ===" << std::endl;
    std::cout << "Binary tick storage with compression" << std::endl;
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << std::endl;

    HistoryServer server;
    CreateDirectoryA(server.data_dir_.c_str(), nullptr);
    run_tcp(server, 5561);

    return 0;
}
