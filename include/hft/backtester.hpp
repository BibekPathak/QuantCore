#pragma once

#include "hft/matching_engine.hpp"
#include "hft/market_data_feed.hpp"
#include "hft/strategy.hpp"
#include "hft/metrics.hpp"
#include <memory>
#include <vector>

namespace hft {

class Backtester {
public:
    Backtester() 
        : engine_({0, 200000, 1}, 1 << 22)
        , order_id_(0)
        , client_id_(1)
    {}

    void set_strategy(std::unique_ptr<IStrategy> strategy) {
        strategy_ = std::move(strategy);
    }

    void set_market_data(MarketDataFeed feed) {
        market_data_ = std::move(feed);
    }

    void run(Metrics& out_metrics) {
        Metrics local_metrics;
        engine_.reset();
        strategy_->reset();
        order_id_ = 0;
        
        engine_.set_trade_callback([&local_metrics, this](const Trade& trade) {
            local_metrics.record_trade(trade);
            strategy_->on_trade(trade);
        });

        uint64_t start_time = 0;
        
        while (market_data_.has_next()) {
            auto tick = market_data_.next();
            
            if (start_time == 0) start_time = tick.timestamp;
            
            auto orders = strategy_->on_tick(tick, order_id_, client_id_);
            
            for (const auto& order : orders) {
                auto before = std::chrono::high_resolution_clock::now();
                auto trades = engine_.process_order(order);
                auto after = std::chrono::high_resolution_clock::now();
                
                local_metrics.record_order(order, trades.size());
                local_metrics.record_latency(std::chrono::duration_cast<std::chrono::nanoseconds>(after - before).count());
                
                order_id_ += 2;
            }
            
            client_id_++;
        }

        local_metrics.finalize(start_time, market_data_.size());
        out_metrics = std::move(local_metrics);
    }

    Metrics run() {
        Metrics result;
        run(result);
        return result;
    }

    MatchingEngine& engine() { return engine_; }
    const MatchingEngine& engine() const { return engine_; }

private:
    MatchingEngine engine_;
    MarketDataFeed market_data_;
    std::unique_ptr<IStrategy> strategy_;
    uint64_t order_id_;
    uint64_t client_id_;
};

}
