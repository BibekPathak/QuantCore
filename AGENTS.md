# HFT Trading System - Quick Reference

## Build Commands
```bash
# Compile main program (with Binance WebSocket feed)
g++ -std=c++20 -O3 -march=native -I include -o hft_test src/main.cpp -lcurl -lssl -lcrypto

# Compile tests (standalone, no external deps)
g++ -std=c++20 -O3 -march=native -I include -o test_order_book tests/test_order_book.cpp

# Run main program
./hft_test

# Run tests
./test_order_book

# Quick compile check for Binance feed
g++ -std=c++20 -O2 -I include -c include/hft/binance_feed.hpp -o /dev/null -lcurl -lssl -lcrypto
```

## Performance Results (Current)
- **Throughput**: ~6.2M orders/sec (full backtest), ~2.6M with EventStore
- **P50 Latency**: ~152ns (full backtest)
- **P99 Latency**: ~2.8μs (full backtest)
- **Order struct size**: 128 bytes (alignas(64))
- **73 unit tests** all passing

## Project Structure
```
hft/
├── include/
│   ├── json.hpp               # nlohmann/json (single header, v3.11.3)
│   └── hft/
│       ├── order.hpp            # Order, Trade, MarketTick, LadderConfig structs
│       ├── arena.hpp            # Arena allocator (bump + intrusive free list)
│       ├── order_book.hpp       # Dense price ladder + intrusive linked lists
│       ├── matching_engine.hpp  # Price-time priority matching + EventStore
│       ├── publisher.hpp        # EventStore - sequenced event log + replay
│       ├── market_data_feed.hpp # CSV/synthetic data loader
│       ├── binance_feed.hpp     # Binance WebSocket feed (libcurl + nlohmann/json)
│       ├── strategy.hpp         # Strategy interface & implementations
│       ├── backtester.hpp       # Simulation orchestrator
│       ├── metrics.hpp          # Latency/throughput tracking
│       ├── lock_free_queue.hpp  # Lock-free MPMC queue
│       ├── thread_safe_matching_engine.hpp
│       ├── thread_pool.hpp
│       ├── object_pool.hpp      # Generic & aligned object pools
│       └── risk_manager.hpp     # Risk-constrained matching engine
├── src/
│   └── main.cpp             # Demo program (backtest + direct throughput)
└── tests/
    └── test_order_book.cpp  # 73 unit tests
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
9. **alignas(64) hot structs** - Order + PriceLevel aligned to cache line, false sharing prevention
10. **Hot/cold field splitting** - Frequently accessed fields in first cache line
11. **__builtin_prefetch** - Software prefetching on intrusive list traversal in matching loops
12. **[[likely]]/[[unlikely]]** - Branch hints for better instruction cache layout

## Completed Phases
- ✅ Phase 1: Arena allocator + Dense ladder + Intrusive list + Matching engine
- ✅ Phase 2: EventStore/Publisher with sequence numbers
- ✅ Phase 3: Multi-threaded matching engine + ThreadPool
- ✅ Phase 4: Risk manager + Stop/Iceberg/OCO orders + Strategy framework
- ✅ Phase 5: Binance WebSocket feed integration
- ✅ Phase 6: PostOnly, IOC, FOK order types
- ✅ Phase 7: CPU optimizations (alignas(64), prefetch, branch hints, field reordering)

## Next Steps
1. Phase 8: Google Benchmark suite + perf + flamegraphs
2. Phase 9: Architecture documentation with Mermaid diagrams
