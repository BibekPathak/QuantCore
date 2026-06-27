#pragma once

#include "hft/order.hpp"
#include <vector>
#include <functional>
#include <variant>
#include <cstdint>

namespace hft {

using Event = std::variant<Trade, ExchangeEvent>;

class EventStore {
public:
    EventStore(size_t initial_capacity = 1 << 20) {
        events_.reserve(initial_capacity);
    }

    void publish(const Trade& trade) {
        events_.push_back(trade);
        for (auto& cb : trade_subs_) {
            cb(trade);
        }
        for (auto& cb : all_subs_) {
            cb(Event(trade));
        }
    }

    void publish(const ExchangeEvent& ev) {
        events_.push_back(ev);
        for (auto& cb : event_subs_) {
            cb(ev);
        }
        for (auto& cb : all_subs_) {
            cb(Event(ev));
        }
    }

    void subscribe_trades(std::function<void(const Trade&)> callback) {
        trade_subs_.push_back(std::move(callback));
    }

    void subscribe_events(std::function<void(const ExchangeEvent&)> callback) {
        event_subs_.push_back(std::move(callback));
    }

    void subscribe_all(std::function<void(const Event&)> callback) {
        all_subs_.push_back(std::move(callback));
    }

    const Event& at(uint64_t sequence) const {
        return events_[sequence - 1];
    }

    size_t size() const { return events_.size(); }
    bool empty() const { return events_.empty(); }

    uint64_t last_sequence() const {
        return events_.size();
    }

    template<typename Func>
    void replay(uint64_t from_seq, uint64_t to_seq, Func&& handler) const {
        if (from_seq < 1) from_seq = 1;
        if (to_seq > events_.size()) to_seq = events_.size();
        for (uint64_t s = from_seq; s <= to_seq; s++) {
            handler(events_[s - 1]);
        }
    }

    template<typename Func>
    void replay_all(Func&& handler) const {
        replay(1, events_.size(), std::forward<Func>(handler));
    }

    void clear() {
        events_.clear();
    }

    void reset() {
        events_.clear();
        trade_subs_.clear();
        event_subs_.clear();
        all_subs_.clear();
    }

    const std::vector<Event>& events() const { return events_; }

private:
    std::vector<Event> events_;
    std::vector<std::function<void(const Trade&)>> trade_subs_;
    std::vector<std::function<void(const ExchangeEvent&)>> event_subs_;
    std::vector<std::function<void(const Event&)>> all_subs_;
};

}
