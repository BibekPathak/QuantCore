#pragma once

#include "hft/order.hpp"
#include <vector>
#include <algorithm>
#include <cstddef>

namespace hft {

class OrderBook {
public:
    std::vector<Order> bids;
    std::vector<Order> asks;

    OrderBook() {
        bids.reserve(1024);
        asks.reserve(1024);
    }

    explicit OrderBook(size_t capacity) {
        bids.reserve(capacity);
        asks.reserve(capacity);
    }

    void clear() {
        bids.clear();
        asks.clear();
    }

    void reserve(size_t capacity) {
        bids.reserve(capacity);
        asks.reserve(capacity);
    }

    size_t size() const {
        return bids.size() + asks.size();
    }

    bool has_bids() const { return !bids.empty(); }
    bool has_asks() const { return !asks.empty(); }
    bool is_empty() const { return bids.empty() && asks.empty(); }

    int64_t best_bid() const {
        return bids.empty() ? 0 : bids.front().price;
    }

    int64_t best_ask() const {
        return asks.empty() ? INT64_MAX : asks.front().price;
    }

    uint64_t best_bid_size() const {
        return bids.empty() ? 0 : bids.front().remaining_quantity();
    }

    uint64_t best_ask_size() const {
        return asks.empty() ? 0 : asks.front().remaining_quantity();
    }

    bool can_match() const {
        if (bids.empty() || asks.empty()) return false;
        return bids.front().price >= asks.front().price;
    }

    void add_order(const Order& order) {
        if (order.is_buy()) {
            insert_sorted_bids(bids, order);
        } else {
            insert_sorted_asks(asks, order);
        }
    }

    Order* find_order(uint64_t order_id) {
        for (auto& o : bids) {
            if (o.order_id == order_id) return &o;
        }
        for (auto& o : asks) {
            if (o.order_id == order_id) return &o;
        }
        return nullptr;
    }

    bool remove_order(uint64_t order_id) {
        auto it = std::find_if(bids.begin(), bids.end(),
            [order_id](const Order& o) { return o.order_id == order_id; });
        if (it != bids.end()) {
            bids.erase(it);
            return true;
        }
        it = std::find_if(asks.begin(), asks.end(),
            [order_id](const Order& o) { return o.order_id == order_id; });
        if (it != asks.end()) {
            asks.erase(it);
            return true;
        }
        return false;
    }

    Order* get_best_bid() {
        return bids.empty() ? nullptr : &bids.front();
    }

    Order* get_best_ask() {
        return asks.empty() ? nullptr : &asks.front();
    }

    void pop_best_bid() {
        if (!bids.empty()) {
            bids.erase(bids.begin());
        }
    }

    void pop_best_ask() {
        if (!asks.empty()) {
            asks.erase(asks.begin());
        }
    }

    void modify_bid_quantity(size_t index, uint64_t new_qty) {
        if (index < bids.size()) {
            bids[index].quantity = new_qty;
        }
    }

    void modify_ask_quantity(size_t index, uint64_t new_qty) {
        if (index < asks.size()) {
            asks[index].quantity = new_qty;
        }
    }

private:
    static void insert_sorted_bids(std::vector<Order>& vec, const Order& order) {
        auto it = std::lower_bound(vec.begin(), vec.end(), order,
            [](const Order& a, const Order& b) {
                return a.price > b.price;
            });
        vec.insert(it, order);
    }

    static void insert_sorted_asks(std::vector<Order>& vec, const Order& order) {
        auto it = std::lower_bound(vec.begin(), vec.end(), order,
            [](const Order& a, const Order& b) {
                return a.price < b.price;
            });
        vec.insert(it, order);
    }
};

}
