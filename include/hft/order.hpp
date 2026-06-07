#pragma once

#include <cstdint>
#include <cstring>

namespace hft {

enum class Side : uint8_t {
    Buy = 0,
    Sell = 1
};

enum class OrderType : uint8_t {
    Market = 0,
    Limit = 1,
    StopLoss = 2,
    StopLimit = 3,
    Iceberg = 4
};

enum class OrderStatus : uint8_t {
    New = 0,
    PartiallyFilled = 1,
    Filled = 2,
    Cancelled = 3,
    Rejected = 4,
    StopPending = 5,
    Triggered = 6
};

enum class TimeInForce : uint8_t {
    Day = 0,
    GTC = 1,
    IOC = 2,
    FOK = 3,
    GTD = 4
};

struct Order {
    uint64_t order_id;
    uint64_t client_id;
    uint64_t timestamp;
    uint64_t quantity;
    uint64_t filled_quantity;
    int64_t price;
    Side side;
    OrderType type;
    OrderStatus status;
    TimeInForce time_in_force;
    int64_t stop_price;
    uint64_t visible_quantity;
    uint64_t revealed_quantity;
    uint64_t linked_order_id;
    uint64_t expire_time;

    Order() = default;

    Order(uint64_t id, uint64_t cid, uint64_t ts, int64_t p, uint64_t q, Side s, OrderType t = OrderType::Limit)
        : order_id(id)
        , client_id(cid)
        , timestamp(ts)
        , quantity(q)
        , filled_quantity(0)
        , price(p)
        , side(s)
        , type(t)
        , status(OrderStatus::New)
        , time_in_force(TimeInForce::Day)
        , stop_price(0)
        , visible_quantity(q)
        , revealed_quantity(0)
        , linked_order_id(0)
        , expire_time(0)
    {}

    bool is_buy() const { return side == Side::Buy; }
    bool is_sell() const { return side == Side::Sell; }
    bool is_filled() const { return filled_quantity >= quantity; }
    bool is_active() const { return status == OrderStatus::New || status == OrderStatus::PartiallyFilled; }
    bool is_stop_pending() const { return status == OrderStatus::StopPending; }
    bool is_triggered() const { return status == OrderStatus::Triggered; }

    uint64_t remaining_quantity() const { return quantity - filled_quantity; }

    uint64_t remaining_visible() const {
        return visible_quantity > revealed_quantity ? visible_quantity - revealed_quantity : 0;
    }

    uint64_t hidden_quantity() const {
        return quantity > visible_quantity ? quantity - visible_quantity : 0;
    }

    bool should_reveal_more() const {
        return hidden_quantity() > 0 && remaining_visible() < visible_quantity / 5;
    }
};

struct Trade {
    uint64_t trade_id;
    uint64_t order_id;
    uint64_t counter_order_id;
    uint64_t timestamp;
    uint64_t quantity;
    int64_t price;
    Side side;

    Trade() = default;

    Trade(uint64_t tid, uint64_t oid, uint64_t coid, uint64_t ts, int64_t p, uint64_t q, Side s)
        : trade_id(tid)
        , order_id(oid)
        , counter_order_id(coid)
        , timestamp(ts)
        , price(p)
        , quantity(q)
        , side(s)
    {}

    bool is_buy() const { return side == Side::Buy; }
    bool is_sell() const { return side == Side::Sell; }
};

struct MarketTick {
    uint64_t timestamp;
    int64_t bid_price;
    int64_t ask_price;
    uint64_t bid_size;
    uint64_t ask_size;

    MarketTick() = default;

    MarketTick(uint64_t ts, int64_t bp, int64_t ap, uint64_t bs, uint64_t as)
        : timestamp(ts)
        , bid_price(bp)
        , ask_price(ap)
        , bid_size(bs)
        , ask_size(as)
    {}

    int64_t mid_price() const { return (bid_price + ask_price) / 2; }
    int64_t spread() const { return ask_price - bid_price; }
};

}
