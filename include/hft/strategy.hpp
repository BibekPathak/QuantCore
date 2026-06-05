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

}
