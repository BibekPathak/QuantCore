#pragma once

#include "hft/order.hpp"
#include "hft/order_book.hpp"
#include <vector>
#include <functional>
#include <atomic>

namespace hft {

class MatchingEngine {
public:
    using TradeCallback = std::function<void(const Trade&)>;

    explicit MatchingEngine(size_t reserve_capacity = 1024)
        : order_book_(reserve_capacity)
        , trade_id_counter_(0)
    {}

    void set_trade_callback(TradeCallback callback) {
        callback_ = std::move(callback);
    }

    std::vector<Trade> process_order(const Order& order) {
        std::vector<Trade> trades;
        trades.reserve(8);

        if (order.type == OrderType::Market) {
            return handle_market_order(order);
        }

        return handle_limit_order(order);
    }

    OrderBook& order_book() { return order_book_; }
    const OrderBook& order_book() const { return order_book_; }

    uint64_t total_trades() const { return trade_id_counter_.load(); }

    bool cancel_order(uint64_t order_id) {
        return order_book_.remove_order(order_id);
    }

    bool amend_order(uint64_t order_id, uint64_t new_quantity) {
        return order_book_.modify_quantity(order_id, new_quantity);
    }

    bool amend_order(uint64_t order_id, int64_t new_price, uint64_t new_quantity) {
        order_book_.remove_order(order_id);
        Order* existing = nullptr;
        if (existing) {
            existing->price = new_price;
            existing->quantity = new_quantity;
            order_book_.add_order(*existing);
            return true;
        }
        return false;
    }

    void reset() {
        order_book_.clear();
        trade_id_counter_.store(0);
    }

private:
    std::vector<Trade> handle_limit_order(const Order& order) {
        std::vector<Trade> trades;
        Order mutable_order = order;

        if (mutable_order.is_buy()) {
            while (order_book_.has_asks() && 
                   mutable_order.remaining_quantity() > 0 &&
                   mutable_order.price >= order_book_.best_ask()) {
                auto* best_ask = order_book_.get_best_ask();
                if (!best_ask) break;

                int64_t match_price = best_ask->price;
                uint64_t match_qty = std::min(mutable_order.remaining_quantity(), best_ask->remaining_quantity());

                Trade trade{
                    ++trade_id_counter_,
                    mutable_order.order_id,
                    best_ask->order_id,
                    mutable_order.timestamp,
                    match_price,
                    match_qty,
                    mutable_order.side
                };
                trades.push_back(trade);

                if (callback_) {
                    callback_(trade);
                }

                best_ask->filled_quantity += match_qty;
                mutable_order.filled_quantity += match_qty;
                if (best_ask->is_filled()) {
                    order_book_.pop_best_ask();
                }
            }

            if (mutable_order.remaining_quantity() > 0 && mutable_order.is_active()) {
                order_book_.add_order(mutable_order);
            }
        } else {
            while (order_book_.has_bids() && 
                   mutable_order.remaining_quantity() > 0 &&
                   mutable_order.price <= order_book_.best_bid()) {
                auto* best_bid = order_book_.get_best_bid();
                if (!best_bid) break;

                int64_t match_price = best_bid->price;
                uint64_t match_qty = std::min(mutable_order.remaining_quantity(), best_bid->remaining_quantity());

                Trade trade{
                    ++trade_id_counter_,
                    mutable_order.order_id,
                    best_bid->order_id,
                    mutable_order.timestamp,
                    match_price,
                    match_qty,
                    mutable_order.side
                };
                trades.push_back(trade);

                if (callback_) {
                    callback_(trade);
                }

                best_bid->filled_quantity += match_qty;
                mutable_order.filled_quantity += match_qty;
                if (best_bid->is_filled()) {
                    order_book_.pop_best_bid();
                }
            }

            if (mutable_order.remaining_quantity() > 0 && mutable_order.is_active()) {
                order_book_.add_order(mutable_order);
            }
        }

        return trades;
    }

    std::vector<Trade> handle_market_order(const Order& order) {
        std::vector<Trade> trades;
        Order mutable_order = order;

        if (mutable_order.is_buy()) {
            while (order_book_.has_asks() && mutable_order.remaining_quantity() > 0) {
                auto* best_ask = order_book_.get_best_ask();
                if (!best_ask) break;

                uint64_t match_qty = std::min(mutable_order.remaining_quantity(), best_ask->remaining_quantity());

                Trade trade{
                    ++trade_id_counter_,
                    mutable_order.order_id,
                    best_ask->order_id,
                    mutable_order.timestamp,
                    best_ask->price,
                    match_qty,
                    mutable_order.side
                };
                trades.push_back(trade);

                if (callback_) {
                    callback_(trade);
                }

                best_ask->filled_quantity += match_qty;
                mutable_order.filled_quantity += match_qty;
                if (best_ask->is_filled()) {
                    order_book_.pop_best_ask();
                }
            }
        } else {
            while (order_book_.has_bids() && mutable_order.remaining_quantity() > 0) {
                auto* best_bid = order_book_.get_best_bid();
                if (!best_bid) break;

                uint64_t match_qty = std::min(mutable_order.remaining_quantity(), best_bid->remaining_quantity());

                Trade trade{
                    ++trade_id_counter_,
                    mutable_order.order_id,
                    best_bid->order_id,
                    mutable_order.timestamp,
                    best_bid->price,
                    match_qty,
                    mutable_order.side
                };
                trades.push_back(trade);

                if (callback_) {
                    callback_(trade);
                }

                best_bid->filled_quantity += match_qty;
                mutable_order.filled_quantity += match_qty;
                if (best_bid->is_filled()) {
                    order_book_.pop_best_bid();
                }
            }
        }

        return trades;
    }

    OrderBook order_book_;
    TradeCallback callback_;
    std::atomic<uint64_t> trade_id_counter_{0};
};

}
