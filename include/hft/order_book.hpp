#pragma once

#include "hft/order.hpp"
#include "hft/arena.hpp"
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>

namespace hft {

class OrderBook {
public:
    OrderBook()
        : ladder_({0, 200000, 1})
        , arena_()
    {
        arena_.init(4 * 1024 * 1024);
        init_ladder(ladder_);
    }

    explicit OrderBook(LadderConfig cfg, size_t arena_capacity = 1 << 22)
        : ladder_(cfg)
        , arena_()
    {
        arena_.init(arena_capacity);
        init_ladder(ladder_);
    }

    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    OrderBook(OrderBook&& other) noexcept = default;
    OrderBook& operator=(OrderBook&& other) noexcept = default;

    void clear() {
        order_map_.clear();
        pending_stop_orders_.clear();
        oco_links_.clear();
        bid_levels_set_.clear();
        ask_levels_set_.clear();
        arena_.reset();
        init_ladder(ladder_);
    }

    size_t size() const {
        return order_map_.size();
    }

    bool has_bids() const { return !bid_levels_set_.empty(); }
    bool has_asks() const { return !ask_levels_set_.empty(); }
    bool is_empty() const { return bid_levels_set_.empty() && ask_levels_set_.empty(); }

    int64_t best_bid() const {
        return has_bids() ? *bid_levels_set_.rbegin() : 0;
    }

    int64_t best_ask() const {
        return has_asks() ? *ask_levels_set_.begin() : std::numeric_limits<int64_t>::max();
    }

    uint64_t best_bid_size() const {
        return has_bids() ? total_quantity_at(bid_ladder_[ladder_.price_to_index(best_bid())]) : 0;
    }

    uint64_t best_ask_size() const {
        return has_asks() ? total_quantity_at(ask_ladder_[ladder_.price_to_index(best_ask())]) : 0;
    }

    bool can_match() const {
        return has_bids() && has_asks() && *bid_levels_set_.rbegin() >= *ask_levels_set_.begin();
    }

    bool add_order(const Order& order) {
        if (order_map_.count(order.order_id)) return false;
        if (!ladder_.in_range(order.price)) return false;

        Order* new_order = arena_.allocate();
        if (!new_order) return false;

        static_cast<Order&>(*new_order) = order;
        new_order->next = nullptr;
        new_order->prev = nullptr;

        if (order.is_buy()) {
            auto& pl = bid_ladder_[ladder_.price_to_index(order.price)];
            bool was_empty = pl.empty();
            pl.append(new_order);
            if (was_empty) bid_levels_set_.insert(order.price);
        } else {
            auto& pl = ask_ladder_[ladder_.price_to_index(order.price)];
            bool was_empty = pl.empty();
            pl.append(new_order);
            if (was_empty) ask_levels_set_.insert(order.price);
        }

        order_map_[order.order_id] = new_order;
        return true;
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
        bool is_buy = order->is_buy();
        int64_t price = order->price;
        size_t idx = ladder_.price_to_index(price);

        if (is_buy) {
            auto& pl = bid_ladder_[idx];
            pl.remove(order);
            if (pl.empty()) bid_levels_set_.erase(price);
        } else {
            auto& pl = ask_ladder_[idx];
            pl.remove(order);
            if (pl.empty()) ask_levels_set_.erase(price);
        }
        order_map_.erase(it);
        arena_.deallocate(order);

        return true;
    }

    bool modify_quantity(uint64_t order_id, uint64_t new_quantity) {
        Order* order = find_order(order_id);
        if (!order || new_quantity == 0) return false;
        if (new_quantity < order->filled_quantity) return false;
        order->quantity = new_quantity;
        return true;
    }

    std::optional<Order> pop_best_bid() {
        if (!has_bids()) return std::nullopt;
        int64_t price = *bid_levels_set_.rbegin();
        auto& pl = bid_ladder_[ladder_.price_to_index(price)];
        Order* o = pl.pop_front();
        if (!o) return std::nullopt;

        Order copy = *o;
        order_map_.erase(o->order_id);
        arena_.deallocate(o);

        if (pl.empty()) {
            bid_levels_set_.erase(price);
        }

        return copy;
    }

    std::optional<Order> pop_best_ask() {
        if (!has_asks()) return std::nullopt;
        int64_t price = *ask_levels_set_.begin();
        auto& pl = ask_ladder_[ladder_.price_to_index(price)];
        Order* o = pl.pop_front();
        if (!o) return std::nullopt;

        Order copy = *o;
        order_map_.erase(o->order_id);
        arena_.deallocate(o);

        if (pl.empty()) {
            ask_levels_set_.erase(price);
        }

        return copy;
    }

    Order* get_best_bid() {
        if (!has_bids()) return nullptr;
        auto& pl = bid_ladder_[ladder_.price_to_index(*bid_levels_set_.rbegin())];
        return pl.front();
    }

    Order* get_best_ask() {
        if (!has_asks()) return nullptr;
        auto& pl = ask_ladder_[ladder_.price_to_index(*ask_levels_set_.begin())];
        return pl.front();
    }

    size_t bid_levels() const { return bid_levels_set_.size(); }
    size_t ask_levels() const { return ask_levels_set_.size(); }

    uint64_t total_bid_volume() const {
        uint64_t vol = 0;
        for (int64_t p : bid_levels_set_) {
            vol += total_quantity_at(bid_ladder_[ladder_.price_to_index(p)]);
        }
        return vol;
    }

    uint64_t total_ask_volume() const {
        uint64_t vol = 0;
        for (int64_t p : ask_levels_set_) {
            vol += total_quantity_at(ask_ladder_[ladder_.price_to_index(p)]);
        }
        return vol;
    }

    uint64_t total_ask_volume_up_to(int64_t price) const {
        uint64_t vol = 0;
        for (int64_t p : ask_levels_set_) {
            if (p > price) break;
            vol += total_quantity_at(ask_ladder_[ladder_.price_to_index(p)]);
        }
        return vol;
    }

    uint64_t total_bid_volume_down_to(int64_t price) const {
        uint64_t vol = 0;
        for (auto it = bid_levels_set_.rbegin(); it != bid_levels_set_.rend(); ++it) {
            if (*it < price) break;
            vol += total_quantity_at(bid_ladder_[ladder_.price_to_index(*it)]);
        }
        return vol;
    }

    void add_pending_stop(const Order& order) {
        pending_stop_orders_.push_back(order);
    }

    bool remove_pending_stop(uint64_t order_id) {
        for (auto it = pending_stop_orders_.begin(); it != pending_stop_orders_.end(); ++it) {
            if (it->order_id == order_id) {
                pending_stop_orders_.erase(it);
                return true;
            }
        }
        return false;
    }

    const std::vector<Order>& get_pending_stops() const {
        return pending_stop_orders_;
    }

    std::vector<Order> check_and_trigger_all_stops(int64_t market_price) {
        std::vector<Order> triggered;
        for (auto it = pending_stop_orders_.begin(); it != pending_stop_orders_.end(); ) {
            bool triggered_flag = false;
            if (it->is_buy() && market_price >= it->stop_price) {
                triggered_flag = true;
            } else if (it->is_sell() && market_price <= it->stop_price) {
                triggered_flag = true;
            }
            if (triggered_flag) {
                Order triggered_order = *it;
                triggered_order.status = OrderStatus::Triggered;
                triggered.push_back(triggered_order);
                it = pending_stop_orders_.erase(it);
            } else {
                ++it;
            }
        }
        return triggered;
    }

    void link_oco_orders(uint64_t order_id_1, uint64_t order_id_2) {
        oco_links_[order_id_1] = order_id_2;
        oco_links_[order_id_2] = order_id_1;
    }

    uint64_t get_linked_order_id(uint64_t order_id) const {
        auto it = oco_links_.find(order_id);
        return it != oco_links_.end() ? it->second : 0;
    }

    void unlink_oco_order(uint64_t order_id) {
        auto it = oco_links_.find(order_id);
        if (it != oco_links_.end()) {
            uint64_t linked_id = it->second;
            oco_links_.erase(it);
            oco_links_.erase(linked_id);
        }
    }

    void clear_oco_links() {
        oco_links_.clear();
    }

    std::optional<uint64_t> check_stop_triggered(uint64_t order_id, int64_t market_price) {
        for (auto it = pending_stop_orders_.begin(); it != pending_stop_orders_.end(); ++it) {
            if (it->order_id == order_id) {
                bool triggered = false;
                if (it->is_buy() && market_price >= it->stop_price) {
                    triggered = true;
                } else if (it->is_sell() && market_price <= it->stop_price) {
                    triggered = true;
                }
                if (triggered) {
                    Order triggered_order = *it;
                    triggered_order.status = OrderStatus::Triggered;
                    pending_stop_orders_.erase(it);
                    return triggered_order.order_id;
                }
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    size_t pending_stops_count() const {
        return pending_stop_orders_.size();
    }

    size_t oco_links_count() const {
        return oco_links_.size() / 2;
    }

    const LadderConfig& ladder_config() const { return ladder_; }

    uint64_t order_count_at_price(int64_t price, Side side) const {
        size_t idx = ladder_.price_to_index(price);
        if (side == Side::Buy) return bid_ladder_[idx].count;
        return ask_ladder_[idx].count;
    }

private:
    struct PriceLevel {
        Order* head = nullptr;
        Order* tail = nullptr;
        uint64_t count = 0;

        void append(Order* order) {
            order->prev = tail;
            order->next = nullptr;
            if (tail) tail->next = order;
            else head = order;
            tail = order;
            count++;
        }

        void remove(Order* order) {
            if (order->prev) order->prev->next = order->next;
            else head = order->next;
            if (order->next) order->next->prev = order->prev;
            else tail = order->prev;
            order->prev = nullptr;
            order->next = nullptr;
            count--;
        }

        Order* pop_front() {
            if (!head) return nullptr;
            Order* o = head;
            remove(o);
            return o;
        }

        bool empty() const { return head == nullptr; }
        Order* front() const { return head; }
    };

    LadderConfig ladder_;
    Arena arena_;
    std::vector<PriceLevel> bid_ladder_;
    std::vector<PriceLevel> ask_ladder_;
    std::set<int64_t> bid_levels_set_;
    std::set<int64_t> ask_levels_set_;
    std::unordered_map<uint64_t, Order*> order_map_;
    std::vector<Order> pending_stop_orders_;
    std::unordered_map<uint64_t, uint64_t> oco_links_;

    void init_ladder(const LadderConfig& cfg) {
        bid_ladder_.assign(cfg.ladder_size(), PriceLevel{});
        ask_ladder_.assign(cfg.ladder_size(), PriceLevel{});
    }

    static uint64_t total_quantity_at(const PriceLevel& pl) {
        uint64_t sum = 0;
        Order* cur = pl.head;
        while (cur) {
            sum += cur->remaining_quantity();
            cur = cur->next;
        }
        return sum;
    }
};

}
