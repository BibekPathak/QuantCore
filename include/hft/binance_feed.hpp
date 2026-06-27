#pragma once

#include "hft/order.hpp"
#include <json.hpp>
#include <curl/curl.h>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <cstdio>

namespace hft {

using json = nlohmann::json;

struct BinanceConfig {
    std::string symbol = "btcusdt";
    std::string base_url = "wss://stream.binance.com:9443/ws";
    bool subscribe_depth = false;
    bool subscribe_ticker = true;
    bool subscribe_trades = false;
};

class BinanceFeed {
public:
    BinanceFeed(BinanceConfig cfg = {})
        : config_(std::move(cfg))
        , sequence_(0)
    {}

    ~BinanceFeed() {
        disconnect();
    }

    bool connect() {
        if (connected_) return true;

        curl_global_init(CURL_GLOBAL_ALL);
        curl_ = curl_easy_init();
        if (!curl_) return false;

        std::string url = config_.base_url + "/" + config_.symbol;
        if (config_.subscribe_ticker) {
            url += "@ticker";
        }
        if (config_.subscribe_depth) {
            url += "/" + config_.symbol + "@depth";
        }

        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_, CURLOPT_CONNECT_ONLY, 2L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl_, CURLOPT_USERAGENT, "hft/1.0");

        CURLcode res = curl_easy_perform(curl_);
        if (res != CURLE_OK) {
            std::fprintf(stderr, "BinanceFeed: WebSocket connect failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl_);
            curl_ = nullptr;
            return false;
        }

        connected_ = true;
        running_ = true;
        feed_thread_ = std::thread(&BinanceFeed::feed_loop, this);
        return true;
    }

    void disconnect() {
        running_ = false;
        if (feed_thread_.joinable()) {
            feed_thread_.join();
        }
        if (curl_) {
            curl_easy_cleanup(curl_);
            curl_ = nullptr;
        }
        connected_ = false;
        curl_global_cleanup();
    }

    bool has_next() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return !tick_queue_.empty();
    }

    MarketTick next() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        cv_.wait(lock, [this] { return !tick_queue_.empty() || !running_; });
        if (tick_queue_.empty()) return {};
        MarketTick tick = tick_queue_.front();
        tick_queue_.pop();
        return tick;
    }

    std::optional<MarketTick> try_next() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (tick_queue_.empty()) return std::nullopt;
        MarketTick tick = tick_queue_.front();
        tick_queue_.pop();
        return tick;
    }

    bool is_connected() const { return connected_; }
    uint64_t ticks_received() const { return sequence_; }

    static int64_t parse_price_for_test(const std::string& s) { return parse_price(s); }
    static uint64_t parse_qty_for_test(const std::string& s) { return parse_qty(s); }

private:
    void feed_loop() {
        char buf[65536];
        size_t received;
        const struct curl_ws_frame* meta = nullptr;

        while (running_) {
            CURLcode res = curl_ws_recv(curl_, buf, sizeof(buf) - 1, &received, &meta);
            if (res != CURLE_OK) {
                if (running_) {
                    std::fprintf(stderr, "BinanceFeed: recv error: %s\n", curl_easy_strerror(res));
                }
                break;
            }

            if (received > 0) {
                buf[received] = '\0';
                process_message(std::string(buf, received));
            }
        }

        connected_ = false;
    }

    void process_message(const std::string& msg) {
        try {
            json j = json::parse(msg);
            if (!j.contains("e")) return;

            std::string event_type = j["e"];

            if (event_type == "24hrTicker") {
                process_ticker(j);
            } else if (event_type == "depthUpdate") {
                process_depth(j);
            } else if (event_type == "trade") {
                process_trade(j);
            }
        } catch (const json::exception& e) {
            std::fprintf(stderr, "BinanceFeed: json parse error: %s\n", e.what());
        }
    }

    void process_ticker(const json& j) {
        uint64_t timestamp = j.value("E", 0ULL);
        std::string bid_str = j.value("b", "0");
        std::string ask_str = j.value("a", "0");
        std::string bid_qty = j.value("B", "0");
        std::string ask_qty = j.value("A", "0");

        int64_t bid_price = parse_price(bid_str);
        int64_t ask_price = parse_price(ask_str);
        uint64_t bid_size = parse_qty(bid_qty);
        uint64_t ask_size = parse_qty(ask_qty);

        uint64_t seq = ++sequence_;

        MarketTick tick(timestamp, seq, bid_price, ask_price, bid_size, ask_size);
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            tick_queue_.push(tick);
        }
        cv_.notify_one();
    }

    void process_depth(const json& j) {
        uint64_t timestamp = j.value("E", 0ULL);
        uint64_t seq = ++sequence_;

        int64_t best_bid = 0, best_ask = std::numeric_limits<int64_t>::max();
        uint64_t best_bid_qty = 0, best_ask_qty = 0;

        if (j.contains("b") && j["b"].is_array()) {
            for (const auto& level : j["b"]) {
                if (level.is_array() && level.size() >= 2) {
                    int64_t price = parse_price(level[0].get<std::string>());
                    uint64_t qty = parse_qty(level[1].get<std::string>());
                    if (qty > 0 && price > best_bid) {
                        best_bid = price;
                        best_bid_qty = qty;
                    }
                }
            }
        }

        if (j.contains("a") && j["a"].is_array()) {
            for (const auto& level : j["a"]) {
                if (level.is_array() && level.size() >= 2) {
                    int64_t price = parse_price(level[0].get<std::string>());
                    uint64_t qty = parse_qty(level[1].get<std::string>());
                    if (qty > 0 && price < best_ask) {
                        best_ask = price;
                        best_ask_qty = qty;
                    }
                }
            }
        }

        MarketTick tick(timestamp, seq, best_bid, best_ask, best_bid_qty, best_ask_qty);
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            tick_queue_.push(tick);
        }
        cv_.notify_one();
    }

    void process_trade(const json& j) {
        uint64_t timestamp = j.value("T", 0ULL);
        uint64_t seq = ++sequence_;

        std::string price_str = j.value("p", "0");
        std::string qty_str = j.value("q", "0");
        std::string side_str = j.value("m", true) ? "sell" : "buy";

        int64_t price = parse_price(price_str);
        uint64_t qty = parse_qty(qty_str);

        MarketTick tick(timestamp, seq, price, price, qty, qty);
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            tick_queue_.push(tick);
        }
        cv_.notify_one();
    }

    static int64_t parse_price(const std::string& s) {
        size_t dot = s.find('.');
        if (dot == std::string::npos) {
            return static_cast<int64_t>(std::stoll(s)) * 100;
        }
        std::string integral = s.substr(0, dot);
        std::string fractional = s.substr(dot + 1);
        while (fractional.size() < 2) fractional += '0';
        if (fractional.size() > 2) fractional = fractional.substr(0, 2);
        return std::stoll(integral) * 100 + std::stoll(fractional);
    }

    static uint64_t parse_qty(const std::string& s) {
        size_t dot = s.find('.');
        if (dot == std::string::npos) {
            return static_cast<uint64_t>(std::stoull(s));
        }
        std::string integral = s.substr(0, dot);
        std::string fractional = s.substr(dot + 1);
        while (fractional.size() < 8) fractional += '0';
        if (fractional.size() > 8) fractional = fractional.substr(0, 8);
        return std::stoull(integral) * 100000000ULL + std::stoull(fractional);
    }

    BinanceConfig config_;
    CURL* curl_ = nullptr;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread feed_thread_;
    std::queue<MarketTick> tick_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::atomic<uint64_t> sequence_{0};
};

}
