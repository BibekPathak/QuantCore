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
- **Throughput**: ~6.2M orders/sec (full backtest), ~2.6M with EventStore
- **P50 Latency**: ~154ns (full backtest)
- **P99 Latency**: ~2.9μs (full backtest)

## Project Structure
```
hft/
├── include/hft/
│   ├── order.hpp            # Order, Trade, MarketTick, LadderConfig structs
│   ├── arena.hpp            # Arena allocator (bump + intrusive free list)
│   ├── order_book.hpp       # Dense price ladder + intrusive linked lists
│   ├── matching_engine.hpp  # Price-time priority matching + EventStore
│   ├── publisher.hpp        # EventStore - sequenced event log + replay
│   ├── market_data_feed.hpp # CSV/synthetic data loader
│   ├── strategy.hpp         # Strategy interface & implementations
│   ├── backtester.hpp       # Simulation orchestrator
│   ├── metrics.hpp          # Latency/throughput tracking
│   ├── lock_free_queue.hpp  # Lock-free MPMC queue
│   ├── thread_safe_matching_engine.hpp
│   ├── thread_pool.hpp
│   ├── object_pool.hpp      # Generic & aligned object pools
│   └── risk_manager.hpp     # Risk-constrained matching engine
├── src/
│   └── main.cpp             # Demo program (backtest + direct throughput)
└── tests/
    └── test_order_book.cpp  # 69 unit tests
```

## Key Design Decisions
1. **Dense price ladder** - O(1) price level access by index, configurable via LadderConfig
2. **Intrusive linked lists** - Order prev/next pointers, O(1) insert/remove per level
3. **Arena allocator** - Contiguous block + inline free list, zero heap fragmentation
4. **std::set tracking** - O(log N) best-bid/ask via non-empty level sets
5. **EventStore** - Sequenced event log with multi-subscriber support + range replay
6. **Separate bid/ask ladders** - Each price level has independent bid and ask lists
7. **C++20** - Variants, structured bindings, if constexpr
8. **Compiler flags**: -O3 -march=native

## Next Steps
1. Phase 5: Binance WebSocket feed integration
2. Phase 6: PostOnly order type, IOC/FOK support
3. Phase 7: CPU optimizations (prefetching, alignment, branch hints)
4. Phase 8: Google Benchmark suite + perf + flamegraphs
5. Phase 9: Architecture documentation with Mermaid diagrams
