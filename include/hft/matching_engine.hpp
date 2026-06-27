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
    using EventCallback = std::function<void(const ExchangeEvent&)>;

    explicit MatchingEngine(LadderConfig cfg = {0, 200000, 1}, size_t arena_capacity = 1 << 22)
        : order_book_(cfg, arena_capacity)
        , trade_id_counter_(0)
        , sequence_counter_(1)
    {}

    void set_trade_callback(TradeCallback callback) {
        callback_ = std::move(callback);
    }

    void set_event_callback(EventCallback callback) {
        event_callback_ = std::move(callback);
    }

    uint64_t next_sequence() {
        return sequence_counter_.fetch_add(1, std::memory_order_acq_rel);
    }

    uint64_t last_sequence() const {
        return sequence_counter_.load(std::memory_order_acquire) - 1;
    }

    void set_sequence_counter(uint64_t seq) {
        sequence_counter_.store(seq, std::memory_order_release);
    }

    std::vector<Trade> process_order(const Order& order) {
        std::vector<Trade> trades;
        trades.reserve(8);

        if (event_callback_) {
            ExchangeEvent ev(next_sequence(), EventType::OrderAccepted, order.order_id,
                           order.price, order.quantity, order.quantity, order.side);
            event_callback_(ev);
        }

        if (order.type == OrderType::StopLoss) {
            return handle_stop_order(order, true);
        }
        if (order.type == OrderType::StopLimit) {
            return handle_stop_order(order, false);
        }
        if (order.type == OrderType::Iceberg) {
            return handle_iceberg_order(order);
        }
        if (order.type == OrderType::Market) {
            return handle_market_order(order);
        }

        return handle_limit_order(order);
    }

    std::vector<Trade> on_market_tick(int64_t market_price) {
        std::vector<Trade> all_trades;
        auto triggered = order_book_.check_and_trigger_all_stops(market_price);
        for (auto& order : triggered) {
            OrderType new_type = order.type == OrderType::StopLoss ? OrderType::Market : OrderType::Limit;
            order.type = new_type;
            order.status = OrderStatus::New;
            auto trades = process_order(order);
            all_trades.insert(all_trades.end(), trades.begin(), trades.end());
        }
        return all_trades;
    }

    OrderBook& order_book() { return order_book_; }
    const OrderBook& order_book() const { return order_book_; }

    uint64_t total_trades() const { return trade_id_counter_.load(); }

    bool cancel_order(uint64_t order_id) {
        uint64_t linked_id = order_book_.get_linked_order_id(order_id);
        bool removed = order_book_.remove_order(order_id);
        if (removed) {
            if (event_callback_) {
                ExchangeEvent ev(next_sequence(), EventType::OrderCancelled, order_id, 0, 0, 0, Side::Buy);
                event_callback_(ev);
            }
            if (linked_id != 0) {
                order_book_.remove_order(linked_id);
                order_book_.unlink_oco_order(order_id);
                if (event_callback_) {
                    ExchangeEvent ev2(next_sequence(), EventType::OrderCancelled, linked_id, 0, 0, 0, Side::Buy);
                    event_callback_(ev2);
                }
            }
        }
        return removed;
    }

    void link_oco(uint64_t order_id_1, uint64_t order_id_2) {
        order_book_.link_oco_orders(order_id_1, order_id_2);
    }

    void handle_post_order(Order& order) {
        if (order.linked_order_id != 0) {
            order_book_.link_oco_orders(order.order_id, order.linked_order_id);
        }
        order_book_.add_order(order);
    }

    void reset() {
        order_book_.clear();
        trade_id_counter_.store(0);
        sequence_counter_.store(1);
    }

    bool amend_order(uint64_t order_id, uint64_t new_quantity) {
        bool changed = order_book_.modify_quantity(order_id, new_quantity);
        if (changed && event_callback_) {
            ExchangeEvent ev(next_sequence(), EventType::OrderModified, order_id, 0, new_quantity, 0, Side::Buy);
            event_callback_(ev);
        }
        return changed;
    }

    template<typename Func>
    void replay(uint64_t seq_start, uint64_t seq_end, Func&& event_handler) {
        EventCallback saved_cb = event_callback_;
        event_callback_ = std::forward<Func>(event_handler);
        event_callback_ = saved_cb;
    }

private:
    uint64_t next_trade_id() {
        return ++trade_id_counter_;
    }

    void emit_trade(Trade& trade) {
        trade.sequence = next_sequence();
        if (callback_) {
            callback_(trade);
        }
        if (event_callback_) {
            ExchangeEvent ev(trade.sequence, EventType::OrderFilled, trade.order_id,
                           trade.price, trade.quantity, 0, trade.side);
            event_callback_(ev);
        }
    }

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
                    next_trade_id(), 0, mutable_order.order_id,
                    best_ask->order_id, mutable_order.timestamp,
                    match_price, match_qty, mutable_order.side
                };
                emit_trade(trade);
                trades.push_back(trade);

                best_ask->filled_quantity += match_qty;
                mutable_order.filled_quantity += match_qty;
                if (best_ask->is_filled()) {
                    order_book_.pop_best_ask();
                }
            }

            if (mutable_order.remaining_quantity() > 0 && mutable_order.is_active()) {
                handle_post_order(mutable_order);
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
                    next_trade_id(), 0, mutable_order.order_id,
                    best_bid->order_id, mutable_order.timestamp,
                    match_price, match_qty, mutable_order.side
                };
                emit_trade(trade);
                trades.push_back(trade);

                best_bid->filled_quantity += match_qty;
                mutable_order.filled_quantity += match_qty;
                if (best_bid->is_filled()) {
                    order_book_.pop_best_bid();
                }
            }

            if (mutable_order.remaining_quantity() > 0 && mutable_order.is_active()) {
                handle_post_order(mutable_order);
            }
        }

        return trades;
    }

    std::vector<Trade> handle_stop_order(const Order& order, bool as_market) {
        order_book_.add_pending_stop(order);
        if (event_callback_) {
            ExchangeEvent ev(next_sequence(), EventType::StopTriggered, order.order_id,
                           order.stop_price, order.quantity, order.quantity, order.side);
            event_callback_(ev);
        }
        return {};
    }

    std::vector<Trade> handle_iceberg_order(const Order& order) {
        std::vector<Trade> trades;
        Order mutable_order = order;

        uint64_t visible_qty = order.visible_quantity > 0 ?
            std::min(order.visible_quantity, order.quantity) : order.quantity;

        mutable_order.quantity = visible_qty;
        mutable_order.status = OrderStatus::New;

        if (mutable_order.is_buy()) {
            while (order_book_.has_asks() &&
                   mutable_order.remaining_quantity() > 0 &&
                   mutable_order.price >= order_book_.best_ask()) {
                auto* best_ask = order_book_.get_best_ask();
                if (!best_ask) break;

                int64_t match_price = best_ask->price;
                uint64_t match_qty = std::min(mutable_order.remaining_quantity(), best_ask->remaining_quantity());

                Trade trade{
                    next_trade_id(), 0, mutable_order.order_id,
                    best_ask->order_id, mutable_order.timestamp,
                    match_price, match_qty, mutable_order.side
                };
                emit_trade(trade);
                trades.push_back(trade);

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
                    next_trade_id(), 0, mutable_order.order_id,
                    best_bid->order_id, mutable_order.timestamp,
                    match_price, match_qty, mutable_order.side
                };
                emit_trade(trade);
                trades.push_back(trade);

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
                    next_trade_id(), 0, mutable_order.order_id,
                    best_ask->order_id, mutable_order.timestamp,
                    best_ask->price, match_qty, mutable_order.side
                };
                emit_trade(trade);
                trades.push_back(trade);

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
                    next_trade_id(), 0, mutable_order.order_id,
                    best_bid->order_id, mutable_order.timestamp,
                    best_bid->price, match_qty, mutable_order.side
                };
                emit_trade(trade);
                trades.push_back(trade);

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
    EventCallback event_callback_;
    std::atomic<uint64_t> trade_id_counter_{0};
    std::atomic<uint64_t> sequence_counter_{1};
};

}
