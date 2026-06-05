#pragma once

#include "hft/order.hpp"
#include <vector>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <numeric>
#include <algorithm>

namespace hft {

class Metrics {
public:
    Metrics() 
        : total_orders_(0)
        , total_trades_(0)
        , total_volume_(0)
        , total_pnl_(0)
        , total_matched_orders_(0)
        , start_time_(0)
        , num_ticks_(0)
    {}

    void record_order(const Order& order, size_t num_trades) {
        total_orders_++;
        if (num_trades > 0) {
            total_matched_orders_++;
        }
    }

    void record_trade(const Trade& trade) {
        total_trades_++;
        total_volume_ += trade.quantity;
        
        if (trade.side == Side::Buy) {
            total_pnl_ -= trade.price * static_cast<int64_t>(trade.quantity);
        } else {
            total_pnl_ += trade.price * static_cast<int64_t>(trade.quantity);
        }
    }

    void record_latency(int64_t latency_ns) {
        latencies_.push_back(latency_ns);
    }

    void finalize(uint64_t start, size_t ticks) {
        start_time_ = start;
        num_ticks_ = ticks;
        
        if (!latencies_.empty()) {
            std::sort(latencies_.begin(), latencies_.end());
        }
    }

    uint64_t total_orders() const { return total_orders_; }
    uint64_t total_trades() const { return total_trades_; }
    uint64_t total_volume() const { return total_volume_; }
    int64_t total_pnl() const { return total_pnl_; }
    uint64_t total_matched_orders() const { return total_matched_orders_; }

    double avg_latency_ns() const {
        if (latencies_.empty()) return 0;
        return std::accumulate(latencies_.begin(), latencies_.end(), 0.0) / latencies_.size();
    }

    int64_t p50_latency_ns() const {
        if (latencies_.empty()) return 0;
        return latencies_[latencies_.size() / 2];
    }

    int64_t p99_latency_ns() const {
        if (latencies_.empty()) return 0;
        return latencies_[latencies_.size() * 99 / 100];
    }

    int64_t min_latency_ns() const {
        if (latencies_.empty()) return 0;
        return latencies_.front();
    }

    int64_t max_latency_ns() const {
        if (latencies_.empty()) return 0;
        return latencies_.back();
    }

    double orders_per_second() const {
        if (start_time_ == 0 || num_ticks_ == 0) return 0;
        return static_cast<double>(total_orders_) / (num_ticks_ * 0.001);
    }

    void print() const {
        printf("\n========== PERFORMANCE METRICS ==========\n");
        printf("Total Orders:       %lu\n", (unsigned long)total_orders_);
        printf("Total Trades:       %lu\n", (unsigned long)total_trades_);
        printf("Total Volume:       %lu\n", (unsigned long)total_volume_);
        printf("Total PnL:          %ld\n", (long)total_pnl_);
        printf("----------------------------------------\n");
        printf("Avg Latency:        %.2f ns\n", avg_latency_ns());
        printf("P50 Latency:        %ld ns\n", (long)p50_latency_ns());
        printf("P99 Latency:        %ld ns\n", (long)p99_latency_ns());
        printf("Min Latency:        %ld ns\n", (long)min_latency_ns());
        printf("Max Latency:        %ld ns\n", (long)max_latency_ns());
        printf("==========================================\n\n");
    }

private:
    uint64_t total_orders_;
    uint64_t total_matched_orders_;
    uint64_t total_trades_;
    uint64_t total_volume_;
    int64_t total_pnl_;
    std::vector<int64_t> latencies_;
    uint64_t start_time_;
    size_t num_ticks_;
};

}
