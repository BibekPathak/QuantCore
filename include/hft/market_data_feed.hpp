#pragma once

#include "hft/order.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <memory>

namespace hft {

class MarketDataFeed {
public:
    MarketDataFeed() = default;

    bool load_csv(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        std::string line;
        std::getline(file, line);

        ticks_.clear();
        ticks_.reserve(100000);

        uint64_t seq = 1;
        while (std::getline(file, line)) {
            size_t pos = 0;
            auto next_token = [&]() {
                size_t start = pos;
                pos = line.find(',', start);
                std::string token = line.substr(start, pos - start);
                if (pos != std::string::npos) pos++;
                return token;
            };

            uint64_t timestamp = std::stoull(next_token());
            int64_t bid_price = std::stoll(next_token());
            int64_t ask_price = std::stoll(next_token());
            uint64_t bid_size = std::stoull(next_token());
            uint64_t ask_size = std::stoull(next_token());

            ticks_.emplace_back(timestamp, seq++, bid_price, ask_price, bid_size, ask_size);
        }

        index_ = 0;
        return true;
    }

    void generate_synthetic(size_t num_ticks, int64_t base_price = 100000, int64_t tick_size = 100) {
        ticks_.clear();
        ticks_.reserve(num_ticks);

        int64_t bid = base_price;
        int64_t ask = base_price + tick_size;
        uint64_t ts = 1000000;

        for (size_t i = 0; i < num_ticks; ++i) {
            int64_t change = (rand() % 5 - 2) * tick_size;
            bid = std::max<int64_t>(100, bid + change);
            ask = bid + tick_size;

            uint64_t bid_size = (rand() % 100 + 1) * 100;
            uint64_t ask_size = (rand() % 100 + 1) * 100;

            ticks_.emplace_back(ts + i * 1000, i + 1, bid, ask, bid_size, ask_size);
        }

        index_ = 0;
    }

    bool has_next() const {
        return index_ < ticks_.size();
    }

    MarketTick next() {
        return ticks_[index_++];
    }

    const MarketTick& peek() const {
        return ticks_[index_];
    }

    void reset() {
        index_ = 0;
    }

    size_t size() const { return ticks_.size(); }
    size_t remaining() const { return ticks_.size() - index_; }
    size_t position() const { return index_; }

    const std::vector<MarketTick>& ticks() const { return ticks_; }

private:
    std::vector<MarketTick> ticks_;
    size_t index_ = 0;
};

}
