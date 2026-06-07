#pragma once

#include "hft/order.hpp"
#include "hft/order_book.hpp"
#include <vector>
#include <functional>
#include <memory>
#include <random>

namespace hft {

class IStrategy {
public:
    virtual ~IStrategy() = default;
    virtual std::vector<Order> on_tick(const MarketTick& tick, uint64_t order_id, uint64_t client_id) = 0;
    virtual void on_trade(const Trade& trade) = 0;
    virtual void reset() = 0;
};

class RandomStrategy : public IStrategy {
public:
    RandomStrategy(double probability = 0.5, int64_t price_offset = 100)
        : probability_(probability)
        , price_offset_(price_offset)
        , gen_(std::random_device{}())
        , dist_(0.0, 1.0)
    {}

    std::vector<Order> on_tick(const MarketTick& tick, uint64_t order_id, uint64_t client_id) override {
        std::vector<Order> orders;
        
        if (dist_(gen_) < probability_) {
            Side side = dist_(gen_) < 0.5 ? Side::Buy : Side::Sell;
            int64_t price_offset = (rand() % 2 == 0 ? 1 : -1) * (rand() % 10 + 1) * (price_offset_ / 10);
            int64_t price = side == Side::Buy ? tick.bid_price + price_offset : tick.ask_price + price_offset;
            uint64_t quantity = (rand() % 10 + 1) * 100;

            orders.emplace_back(order_id, client_id, tick.timestamp, price, quantity, side, OrderType::Limit);
        }

        return orders;
    }

    void on_trade(const Trade& trade) override {
    }

    void reset() override {}

private:
    double probability_;
    int64_t price_offset_;
    std::mt19937 gen_;
    std::uniform_real_distribution<double> dist_;
};

class MarketMakerStrategy : public IStrategy {
public:
    MarketMakerStrategy(int64_t spread = 200, uint64_t size = 1000)
        : spread_(spread)
        , size_(size)
        , gen_(std::random_device{}())
        , dist_(0.0, 1.0)
    {}

    std::vector<Order> on_tick(const MarketTick& tick, uint64_t order_id, uint64_t client_id) override {
        std::vector<Order> orders;
        
        int64_t mid = (tick.bid_price + tick.ask_price) / 2;
        int64_t half_spread = spread_ / 2;
        
        int64_t bid_price = mid - half_spread;
        int64_t ask_price = mid + half_spread;

        orders.emplace_back(order_id, client_id, tick.timestamp, bid_price, size_, Side::Buy, OrderType::Limit);
        orders.emplace_back(order_id + 1, client_id, tick.timestamp, ask_price, size_, Side::Sell, OrderType::Limit);

        return orders;
    }

    void on_trade(const Trade& trade) override {}

    void reset() override {}

private:
    int64_t spread_;
    uint64_t size_;
    std::mt19937 gen_;
    std::uniform_real_distribution<double> dist_;
};

class MomentumStrategy : public IStrategy {
public:
    MomentumStrategy(int64_t threshold = 500)
        : threshold_(threshold)
        , last_mid_(0)
        , position_(0)
    {}

    std::vector<Order> on_tick(const MarketTick& tick, uint64_t order_id, uint64_t client_id) override {
        std::vector<Order> orders;
        
        int64_t mid = (tick.bid_price + tick.ask_price) / 2;
        
        if (last_mid_ != 0) {
            int64_t diff = mid - last_mid_;
            
            if (diff > threshold_ && position_ <= 0) {
                orders.emplace_back(order_id, client_id, tick.timestamp, tick.ask_price, 1000, Side::Buy, OrderType::Market);
                position_ += 1000;
            } else if (diff < -threshold_ && position_ >= 0) {
                orders.emplace_back(order_id, client_id, tick.timestamp, tick.bid_price, 1000, Side::Sell, OrderType::Market);
                position_ -= 1000;
            }
        }
        
        last_mid_ = mid;
        return orders;
    }

    void on_trade(const Trade& trade) override {
        if (trade.side == Side::Buy) {
            position_ += trade.quantity;
        } else {
            position_ -= trade.quantity;
        }
    }

    void reset() override {
        last_mid_ = 0;
        position_ = 0;
    }

    int64_t position() const { return position_; }

private:
    int64_t threshold_;
    int64_t last_mid_;
    int64_t position_;
};

class VWAPStrategy : public IStrategy {
public:
    VWAPStrategy(uint64_t target_quantity = 10000, uint64_t slice_count = 10)
        : target_quantity_(target_quantity)
        , slice_count_(slice_count)
        , slice_size_(target_quantity / slice_count)
        , current_slice_(0)
        , position_(0)
        , vwap_price_(0)
        , total_volume_(0)
        , last_time_bucket_(0)
    {}

    std::vector<Order> on_tick(const MarketTick& tick, uint64_t order_id, uint64_t client_id) override {
        std::vector<Order> orders;
        
        int64_t mid = (tick.bid_price + tick.ask_price) / 2;
        
        size_t time_bucket = tick.timestamp % slice_count_;
        
        if (time_bucket != last_time_bucket_ && current_slice_ < slice_count_) {
            if (last_time_bucket_ != 0 || current_slice_ == 0) {
                int64_t price_offset = (rand() % 5 - 2) * 10;
                
                uint64_t qty = slice_size_;
                
                Side side = Side::Buy;
                int64_t price = tick.bid_price + price_offset;
                
                orders.emplace_back(order_id + current_slice_, client_id, tick.timestamp, 
                                   price, qty, side, OrderType::Limit);
                
                current_slice_++;
            }
            last_time_bucket_ = time_bucket;
        }
        
        return orders;
    }

    void on_trade(const Trade& trade) override {
        int64_t value = static_cast<int64_t>(trade.quantity) * trade.price;
        total_volume_ += trade.quantity;
        vwap_price_ = total_volume_ > 0 ? 
            (vwap_price_ * (total_volume_ - trade.quantity) + value) / total_volume_ : trade.price;
        
        if (trade.side == Side::Buy) {
            position_ += trade.quantity;
        } else {
            position_ -= trade.quantity;
        }
    }

    void reset() override {
        current_slice_ = 0;
        position_ = 0;
        vwap_price_ = 0;
        total_volume_ = 0;
        last_time_bucket_ = 0;
    }

    int64_t position() const { return position_; }
    int64_t vwap_price() const { return vwap_price_; }
    uint64_t remaining_slices() const { return slice_count_ - current_slice_; }

private:
    uint64_t target_quantity_;
    uint64_t slice_count_;
    uint64_t slice_size_;
    uint64_t current_slice_;
    int64_t position_;
    int64_t vwap_price_;
    uint64_t total_volume_;
    size_t last_time_bucket_;
};

class VWAPMarketMaker : public IStrategy {
public:
    VWAPMarketMaker(int64_t base_spread = 100, uint64_t order_size = 500)
        : base_spread_(base_spread)
        , order_size_(order_size)
        , vwap_accumulator_(0)
        , volume_accumulator_(0)
    {}

    std::vector<Order> on_tick(const MarketTick& tick, uint64_t order_id, uint64_t client_id) override {
        std::vector<Order> orders;
        
        int64_t mid = (tick.bid_price + tick.ask_price) / 2;
        
        vwap_accumulator_ += mid;
        volume_accumulator_++;
        int64_t vwap = volume_accumulator_ > 0 ? vwap_accumulator_ / volume_accumulator_ : mid;
        
        int64_t spread = base_spread_;
        int64_t half_spread = spread / 2;
        
        int64_t bid_price = mid - half_spread;
        int64_t ask_price = mid + half_spread;
        
        orders.emplace_back(order_id, client_id, tick.timestamp, bid_price, order_size_, Side::Buy, OrderType::Limit);
        orders.emplace_back(order_id + 1, client_id, tick.timestamp, ask_price, order_size_, Side::Sell, OrderType::Limit);

        return orders;
    }

    void on_trade(const Trade& trade) override {}

    void reset() override {
        vwap_accumulator_ = 0;
        volume_accumulator_ = 0;
    }

    int64_t current_vwap() const {
        return volume_accumulator_ > 0 ? vwap_accumulator_ / volume_accumulator_ : 0;
    }

private:
    int64_t base_spread_;
    uint64_t order_size_;
    int64_t vwap_accumulator_;
    uint64_t volume_accumulator_;
};

}
