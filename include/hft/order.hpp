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
    Order* next;
    Order* prev;

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
        , next(nullptr)
        , prev(nullptr)
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

struct LadderConfig {
    int64_t min_price;
    int64_t max_price;
    int64_t tick_size;

    size_t ladder_size() const {
        return static_cast<size_t>((max_price - min_price) / tick_size) + 1;
    }

    size_t price_to_index(int64_t price) const {
        return static_cast<size_t>((price - min_price) / tick_size);
    }

    bool in_range(int64_t price) const {
        return price >= min_price && price <= max_price;
    }
};

struct Trade {
    uint64_t trade_id;
    uint64_t sequence;
    uint64_t order_id;
    uint64_t counter_order_id;
    uint64_t timestamp;
    uint64_t quantity;
    int64_t price;
    Side side;

    Trade() = default;

    Trade(uint64_t tid, uint64_t seq, uint64_t oid, uint64_t coid, uint64_t ts, int64_t p, uint64_t q, Side s)
        : trade_id(tid)
        , sequence(seq)
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

struct BookUpdate {
    uint64_t sequence;
    int64_t price;
    uint64_t size;
    Side side;
    uint64_t order_count;
    uint64_t volume;

    BookUpdate() = default;

    BookUpdate(uint64_t seq, int64_t p, uint64_t s, Side sd, uint64_t oc, uint64_t v)
        : sequence(seq), price(p), size(s), side(sd), order_count(oc), volume(v)
    {}
};

struct OrderEvent {
    uint64_t sequence;
    uint64_t order_id;
    OrderStatus status;
    uint64_t filled_quantity;
    int64_t fill_price;
    uint64_t fill_quantity;

    OrderEvent() = default;

    OrderEvent(uint64_t seq, uint64_t oid, OrderStatus st)
        : sequence(seq), order_id(oid), status(st), filled_quantity(0), fill_price(0), fill_quantity(0)
    {}
};

enum class EventType : uint8_t {
    OrderAccepted = 0,
    OrderFilled = 1,
    OrderPartiallyFilled = 2,
    OrderCancelled = 3,
    OrderRejected = 4,
    OrderModified = 5,
    StopTriggered = 6,
    BookUpdated = 7
};

struct ExchangeEvent {
    uint64_t sequence;
    EventType type;
    uint64_t order_id;
    int64_t price;
    uint64_t quantity;
    uint64_t remaining;
    Side side;

    ExchangeEvent() = default;

    ExchangeEvent(uint64_t seq, EventType t, uint64_t oid, int64_t p, uint64_t q, uint64_t rem, Side s)
        : sequence(seq), type(t), order_id(oid), price(p), quantity(q), remaining(rem), side(s)
    {}
};

struct MarketTick {
    uint64_t timestamp;
    uint64_t sequence;
    int64_t bid_price;
    int64_t ask_price;
    uint64_t bid_size;
    uint64_t ask_size;

    MarketTick() = default;

    MarketTick(uint64_t ts, uint64_t seq, int64_t bp, int64_t ap, uint64_t bs, uint64_t as)
        : timestamp(ts)
        , sequence(seq)
        , bid_price(bp)
        , ask_price(ap)
        , bid_size(bs)
        , ask_size(as)
    {}

    int64_t mid_price() const { return (bid_price + ask_price) / 2; }
    int64_t spread() const { return ask_price - bid_price; }
};

}
