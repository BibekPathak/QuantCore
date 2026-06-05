# HFT Trading System - Quick Reference

## Build Commands
```bash
# Compile main program
g++ -std=c++20 -O3 -march=native -I include -o hft_test src/main.cpp

# Compile tests
g++ -std=c++20 -O3 -march=native -I include -o test_order_book tests/test_order_book.cpp

# Run main program
./hft_test

# Run tests
./test_order_book
```

## Performance Results (Current)
- **Throughput**: ~23M orders/sec
- **P50 Latency**: ~10μs
- **P99 Latency**: ~25μs

## Project Structure
```
hft/
├── include/hft/
│   ├── order.hpp         # Order, Trade, MarketTick structs
│   ├── order_book.hpp    # Sorted vector-based order book
│   ├── matching_engine.hpp # Price-time priority matching
│   ├── market_data_feed.hpp # CSV/synthetic data loader
│   ├── strategy.hpp      # Strategy interface & implementations
│   ├── backtester.hpp    # Simulation orchestrator
│   └── metrics.hpp       # Latency/throughput tracking
├── src/
│   └── main.cpp          # Demo program
└── tests/
    └── test_order_book.cpp # Unit tests
```

## Key Design Decisions
1. **Vector-based OrderBook** - Cache-friendly, sorted inserts
2. **Price-Time Priority** - FIFO matching at best price
3. **Preallocation** - Reserve capacity to avoid heap allocations
4. **C++20** - Structured bindings, concepts, ranges
5. **Compiler flags**: -O3 -march=native

## Next Steps (Optimization Phases)
1. Phase 4: Add preallocation, object pooling
2. Phase 5: Multi-threading with thread pools
3. Phase 6: Lock-free queues, SIMD intrinsics
