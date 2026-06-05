#pragma once

#include "hft/order.hpp"
#include "hft/order_book.hpp"
#include "hft/lock_free_queue.hpp"
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <optional>
#include <atomic>

namespace hft {

struct OrderResult {
    std::vector<Trade> trades;
    bool success;
    const char* error_message;
};

class ThreadSafeMatchingEngine {
public:
    using TradeCallback = std::function<void(const Trade&)>;

    explicit ThreadSafeMatchingEngine(size_t queue_capacity = 65536)
        : order_queue_(queue_capacity)
        , running_(false)
        , orders_processed_(0)
        , orders_failed_(0)
    {}

    void set_trade_callback(TradeCallback callback) {
        callback_ = std::move(callback);
    }

    bool enqueue_order(const Order& order) {
        if (!order_queue_.enqueue(order)) {
            orders_failed_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        return true;
    }

    bool enqueue_order(Order&& order) {
        if (!order_queue_.enqueue(order)) {
            orders_failed_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        return true;
    }

    size_t process_all_pending() {
        Order order;
        size_t count = 0;
        while (order_queue_.dequeue(order)) {
            auto trades = engine_.process_order(order);
            for (const auto& trade : trades) {
                if (callback_) {
                    callback_(trade);
                }
            }
            count++;
        }
        orders_processed_.fetch_add(count, std::memory_order_relaxed);
        return count;
    }

    size_t drain_pending() {
        return process_all_pending();
    }

    void start_processing_loop() {
        running_.store(true, std::memory_order_release);
        processor_thread_ = std::thread([this] {
            Order order;
            while (running_.load(std::memory_order_acquire)) {
                if (order_queue_.dequeue(order)) {
                    auto trades = engine_.process_order(order);
                    for (const auto& trade : trades) {
                        if (callback_) {
                            callback_(trade);
                        }
                    }
                    orders_processed_.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    void stop_processing_loop() {
        running_.store(false, std::memory_order_release);
        if (processor_thread_.joinable()) {
            processor_thread_.join();
        }
        process_all_pending();
    }

    OrderBook& order_book() { return engine_.order_book(); }
    const OrderBook& order_book() const { return engine_.order_book(); }

    uint64_t total_trades() const { return engine_.total_trades(); }
    uint64_t orders_processed() const { return orders_processed_.load(); }
    uint64_t orders_failed() const { return orders_failed_.load(); }

    void reset() {
        order_queue_.clear();
        engine_.reset();
        orders_processed_.store(0);
        orders_failed_.store(0);
    }

    size_t pending_orders() const {
        return order_queue_.size();
    }

private:
    ThreadSafeOrderQueue<Order> order_queue_;
    MatchingEngine engine_;
    TradeCallback callback_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> orders_processed_;
    std::atomic<uint64_t> orders_failed_;
    std::thread processor_thread_;
};

}
