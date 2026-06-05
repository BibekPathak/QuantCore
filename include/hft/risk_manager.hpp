#pragma once

#include "hft/order.hpp"
#include <atomic>
#include <cstdint>
#include <optional>

namespace hft {

struct RiskLimits {
    uint64_t max_order_size = 1000000;
    int64_t max_position = 10000000;
    int64_t max_daily_loss = -100000000;
    int64_t max_order_value = 10000000000;
    uint64_t max_orders_per_second = 100000;
    uint64_t min_order_size = 1;
};

struct RiskCheckResult {
    bool approved;
    const char* reason;
    
    static RiskCheckResult approved_result() {
        return {true, nullptr};
    }
    
    static RiskCheckResult rejected(const char* reason) {
        return {false, reason};
    }
};

class RiskManager {
public:
    explicit RiskManager(RiskLimits limits = {})
        : limits_(limits)
        , net_position_(0)
        , daily_pnl_(0)
        , order_count_(0)
        , rejected_count_(0)
    {}

    RiskCheckResult check_order(const Order& order) {
        if (order.quantity < limits_.min_order_size) {
            rejected_count_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult::rejected("Order too small");
        }
        
        if (order.quantity > limits_.max_order_size) {
            rejected_count_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult::rejected("Order exceeds max size");
        }
        
        if (order.price > 0) {
            int64_t order_value = static_cast<int64_t>(order.quantity) * order.price;
            if (order_value > limits_.max_order_value) {
                rejected_count_.fetch_add(1, std::memory_order_relaxed);
                return RiskCheckResult::rejected("Order value exceeds limit");
            }
        }
        
        if (limits_.max_orders_per_second > 0) {
            uint64_t rate = order_rate_.load(std::memory_order_relaxed);
            if (rate >= limits_.max_orders_per_second) {
                rejected_count_.fetch_add(1, std::memory_order_relaxed);
                return RiskCheckResult::rejected("Order rate limit exceeded");
            }
        }
        
        return RiskCheckResult::approved_result();
    }

    RiskCheckResult check_position(const Order& order, int64_t current_position) {
        int64_t new_position = current_position;
        if (order.is_buy()) {
            new_position += static_cast<int64_t>(order.quantity);
        } else {
            new_position -= static_cast<int64_t>(order.quantity);
        }
        
        if (new_position > limits_.max_position || new_position < -limits_.max_position) {
            rejected_count_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult::rejected("Position limit exceeded");
        }
        
        return RiskCheckResult::approved_result();
    }

    RiskCheckResult check_daily_loss() {
        if (limits_.max_daily_loss < 0 && daily_pnl_.load(std::memory_order_acquire) < limits_.max_daily_loss) {
            return RiskCheckResult::rejected("Daily loss limit exceeded");
        }
        return RiskCheckResult::approved_result();
    }

    void record_trade(const Trade& trade) {
        if (trade.is_buy()) {
            net_position_.fetch_add(static_cast<int64_t>(trade.quantity), std::memory_order_relaxed);
        } else {
            net_position_.fetch_sub(static_cast<int64_t>(trade.quantity), std::memory_order_relaxed);
        }
        
        int64_t pnl_change = static_cast<int64_t>(trade.quantity) * trade.price;
        if (trade.is_sell()) {
            daily_pnl_.fetch_add(pnl_change, std::memory_order_relaxed);
        } else {
            daily_pnl_.fetch_sub(pnl_change, std::memory_order_relaxed);
        }
        
        order_count_.fetch_add(1, std::memory_order_relaxed);
        order_rate_.fetch_add(1, std::memory_order_relaxed);
    }

    void increment_order_count() {
        order_count_.fetch_add(1, std::memory_order_relaxed);
        order_rate_.fetch_add(1, std::memory_order_relaxed);
    }

    void reset_daily() {
        daily_pnl_.store(0, std::memory_order_release);
        order_count_.store(0, std::memory_order_release);
        order_rate_.store(0, std::memory_order_release);
    }

    void reset_rate_counter() {
        order_rate_.store(0, std::memory_order_release);
    }

    int64_t net_position() const { return net_position_.load(); }
    int64_t daily_pnl() const { return daily_pnl_.load(); }
    uint64_t order_count() const { return order_count_.load(); }
    uint64_t rejected_count() const { return rejected_count_.load(); }
    uint64_t order_rate() const { return order_rate_.load(); }

    void set_limits(RiskLimits limits) { limits_ = limits; }
    RiskLimits get_limits() const { return limits_; }

    void set_max_order_size(uint64_t size) { limits_.max_order_size = size; }
    void set_max_position(int64_t pos) { limits_.max_position = pos; }
    void set_max_daily_loss(int64_t loss) { limits_.max_daily_loss = loss; }

private:
    RiskLimits limits_;
    std::atomic<int64_t> net_position_;
    std::atomic<int64_t> daily_pnl_;
    std::atomic<uint64_t> order_count_;
    std::atomic<uint64_t> rejected_count_;
    std::atomic<uint64_t> order_rate_{0};
};

class RiskCheckedMatchingEngine {
public:
    explicit RiskCheckedMatchingEngine(RiskLimits limits = {})
        : risk_manager_(limits)
    {}

    bool process_order(const Order& order) {
        auto check = risk_manager_.check_order(order);
        if (!check.approved) {
            return false;
        }
        
        auto trades = engine_.process_order(order);
        for (const auto& trade : trades) {
            risk_manager_.record_trade(trade);
            if (callback_) {
                callback_(trade);
            }
        }
        
        risk_manager_.increment_order_count();
        return true;
    }

    void set_trade_callback(typename MatchingEngine::TradeCallback callback) {
        callback_ = std::move(callback);
    }

    auto check_order(const Order& order) -> RiskCheckResult {
        return risk_manager_.check_order(order);
    }

    auto check_position(const Order& order) -> RiskCheckResult {
        return risk_manager_.check_position(order, risk_manager_.net_position());
    }

    OrderBook& order_book() { return engine_.order_book(); }
    const OrderBook& order_book() const { return engine_.order_book(); }
    RiskManager& risk_manager() { return risk_manager_; }
    const RiskManager& risk_manager() const { return risk_manager_; }

    void reset() {
        engine_.reset();
        risk_manager_.reset_daily();
    }

private:
    MatchingEngine engine_;
    RiskManager risk_manager_;
    typename MatchingEngine::TradeCallback callback_;
};

}
