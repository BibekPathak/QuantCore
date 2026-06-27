#pragma once

#include <atomic>
#include <cstdint>

namespace hft {

class Sequencer {
public:
    Sequencer() = default;

    uint64_t next() {
        return next_.fetch_add(1, std::memory_order_acq_rel);
    }

    uint64_t last() const {
        return next_.load(std::memory_order_acquire) - 1;
    }

    void reset(uint64_t start = 1) {
        next_.store(start, std::memory_order_release);
    }

private:
    std::atomic<uint64_t> next_{1};
};

}
