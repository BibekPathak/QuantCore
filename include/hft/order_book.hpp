#pragma once

#include "hft/order.hpp"
#include <vector>
#include <deque>
#include <unordered_map>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>

namespace hft {

struct PriceLevel {
    int64_t price;
    std::deque<Order> orders;
    uint64_t total_quantity() const {
        uint64_t sum = 0;
        for (const auto& o : orders) {
            sum += o.remaining_quantity();
        }
        return sum;
    }
};

class OrderBook {
public:
    static constexpr size_t MAX_PRICE_LEVELS = 100000;

    explicit OrderBook(int64_t min_price = 0, int64_t max_price = 100000000) 
        : min_price_(min_price), max_price_(max_price) {
        price_to_index_.reserve(4096);
    }

    void clear() {
        bids_.clear();
        asks_.clear();
        order_map_.clear();
        price_to_index_.clear();
    }

    size_t size() const {
        size_t count = 0;
        for (const auto& pl : bids_) count += pl.orders.size();
        for (const auto& pl : asks_) count += pl.orders.size();
        return count;
    }

    bool has_bids() const { return !bids_.empty(); }
    bool has_asks() const { return !asks_.empty(); }
    bool is_empty() const { return bids_.empty() && asks_.empty(); }

    int64_t best_bid() const {
        return bids_.empty() ? 0 : bids_.front().price;
    }

    int64_t best_ask() const {
        return asks_.empty() ? std::numeric_limits<int64_t>::max() : asks_.front().price;
    }

    uint64_t best_bid_size() const {
        return bids_.empty() ? 0 : bids_.front().total_quantity();
    }

    uint64_t best_ask_size() const {
        return asks_.empty() ? 0 : asks_.front().total_quantity();
    }

    bool can_match() const {
        if (bids_.empty() || asks_.empty()) return false;
        return bids_.front().price >= asks_.front().price;
    }

    bool add_order(const Order& order) {
        auto* existing = find_order(order.order_id);
        if (existing) return false;

        if (order.is_buy()) {
            return insert_order(bids_, order, true);
        } else {
            return insert_order(asks_, order, false);
        }
    }

    Order* find_order(uint64_t order_id) {
        auto it = order_map_.find(order_id);
        return it != order_map_.end() ? it->second : nullptr;
    }

    const Order* find_order(uint64_t order_id) const {
        auto it = order_map_.find(order_id);
        return it != order_map_.end() ? it->second : nullptr;
    }

    bool remove_order(uint64_t order_id) {
        auto it = order_map_.find(order_id);
        if (it == order_map_.end()) return false;

        Order* order = it->second;
        auto& levels = order->is_buy() ? bids_ : asks_;
        
        for (auto& pl : levels) {
            for (auto oi = pl.orders.begin(); oi != pl.orders.end(); ++oi) {
                if (oi->order_id == order_id) {
                    pl.orders.erase(oi);
                    order_map_.erase(it);
                    if (pl.orders.empty()) {
                        remove_empty_level(levels, pl.price);
                    }
                    return true;
                }
            }
        }
        return false;
    }

    bool modify_quantity(uint64_t order_id, uint64_t new_quantity) {
        Order* order = find_order(order_id);
        if (!order || new_quantity == 0) return false;
        
        if (new_quantity < order->filled_quantity) return false;
        order->quantity = new_quantity;
        return true;
    }

    std::optional<Order> pop_best_bid() {
        if (bids_.empty()) return std::nullopt;
        
        auto& level = bids_.front();
        Order order = level.orders.front();
        level.orders.pop_front();
        
        order_map_.erase(order.order_id);
        
        if (level.orders.empty()) {
            bids_.erase(bids_.begin());
        }
        
        return order;
    }

    std::optional<Order> pop_best_ask() {
        if (asks_.empty()) return std::nullopt;
        
        auto& level = asks_.front();
        Order order = level.orders.front();
        level.orders.pop_front();
        
        order_map_.erase(order.order_id);
        
        if (level.orders.empty()) {
            asks_.erase(asks_.begin());
        }
        
        return order;
    }

    Order* get_best_bid() {
        return bids_.empty() ? nullptr : &bids_.front().orders.front();
    }

    Order* get_best_ask() {
        return asks_.empty() ? nullptr : &asks_.front().orders.front();
    }

    const std::vector<PriceLevel>& get_bids() const { return bids_; }
    const std::vector<PriceLevel>& get_asks() const { return asks_; }

    size_t bid_levels() const { return bids_.size(); }
    size_t ask_levels() const { return asks_.size(); }

    uint64_t total_bid_volume() const {
        uint64_t vol = 0;
        for (const auto& pl : bids_) vol += pl.total_quantity();
        return vol;
    }

    uint64_t total_ask_volume() const {
        uint64_t vol = 0;
        for (const auto& pl : asks_) vol += pl.total_quantity();
        return vol;
    }

private:
    std::vector<PriceLevel> bids_;
    std::vector<PriceLevel> asks_;
    std::unordered_map<uint64_t, Order*> order_map_;
    std::unordered_map<int64_t, size_t> price_to_index_;
    int64_t min_price_;
    int64_t max_price_;

    bool insert_order(std::vector<PriceLevel>& levels, const Order& order, bool is_bid) {
        auto it = std::lower_bound(levels.begin(), levels.end(), order.price,
            [is_bid](const PriceLevel& pl, int64_t p) {
                return is_bid ? pl.price > p : pl.price < p;
            });

        Order* order_ptr = nullptr;
        
        if (it == levels.end() || it->price != order.price) {
            PriceLevel new_level;
            new_level.price = order.price;
            new_level.orders.push_back(order);
            it = levels.insert(it, std::move(new_level));
            order_ptr = &it->orders.back();
        } else {
            it->orders.push_back(order);
            order_ptr = &it->orders.back();
        }

        order_map_[order.order_id] = order_ptr;
        return true;
    }

    void remove_empty_level(std::vector<PriceLevel>& levels, int64_t price) {
        auto it = std::find_if(levels.begin(), levels.end(),
            [price](const PriceLevel& pl) { return pl.price == price; });
        if (it != levels.end()) {
            levels.erase(it);
        }
    }
};

}