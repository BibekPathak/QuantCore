#pragma once

#include "hft/sequencer.hpp"
#include "hft/matching_engine.hpp"
#include "hft/risk_manager.hpp"
#include <vector>

namespace hft {

class Exchange {
public:
    struct SubmitResult {
        std::vector<Trade> trades;
        bool accepted;
        const char* reason;
    };

    explicit Exchange(LadderConfig cfg = {0, 200000, 1}, size_t arena_capacity = 1 << 22)
        : me_(cfg, arena_capacity)
    {}

    MatchingEngine& engine() { return me_; }
    const MatchingEngine& engine() const { return me_; }

    EventStore& events() { return me_.event_store(); }
    const EventStore& events() const { return me_.event_store(); }

    RiskManager& risk() { return risk_; }
    const RiskManager& risk() const { return risk_; }

    Sequencer& sequencer() { return seq_; }
    const Sequencer& sequencer() const { return seq_; }

    SubmitResult submit(const Order& order) {
        auto check = risk_.check_order(order);
        if (!check.approved) {
            uint64_t seq = seq_.next();
            me_.event_store().publish(ExchangeEvent(
                seq, EventType::OrderRejected, order.order_id,
                order.price, order.quantity, order.quantity, order.side));
            return {{}, false, check.reason};
        }
        auto trades = me_.process_order(seq_, order);
        for (auto& t : trades) {
            risk_.record_trade(t);
        }
        risk_.increment_order_count();
        return {trades, true, nullptr};
    }

    bool cancel(uint64_t order_id) {
        return me_.cancel_order(seq_, order_id);
    }

    bool amend(uint64_t order_id, uint64_t new_quantity) {
        return me_.amend_order(seq_, order_id, new_quantity);
    }

    void reset() {
        seq_.reset();
        risk_.reset_daily();
        me_.reset();
    }

private:
    Sequencer seq_;
    RiskManager risk_;
    MatchingEngine me_;
};

}
