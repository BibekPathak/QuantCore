#include "hft/order_book.hpp"
#include "hft/matching_engine.hpp"
#include "hft/lock_free_queue.hpp"
#include "hft/object_pool.hpp"
#include "hft/risk_manager.hpp"
#include "hft/thread_pool.hpp"
#include "hft/publisher.hpp"
#include "hft/benchmark.hpp"
#include <iostream>
#include <iomanip>
#include <atomic>
#include <vector>
#include <thread>

using namespace hft;

struct EmptyCallback {
    void operator()(const Trade&) {}
    void operator()(const ExchangeEvent&) {}
};

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cout << "HFT Trading System - Benchmarks\n" << std::flush;
    std::cout << "================================\n\n";

    // ===== Order Book =====
    std::cout << "=== Order Book Benchmarks ===\n";
    std::cout << std::string(60, '-') << "\n";

    {
        auto r = Benchmark::run("add 10000 orders", [&]() {
            OrderBook lb;
            for (int i = 0; i < 10000; i++)
                lb.add_order(Order(i, 1, 1000, 100000 + i, 100, Side::Buy));
        }, 30, false);
        r.print("OrderBook: add 10000 orders");
    }
    {
        OrderBook ob;
        for (int i = 0; i < 50000; i++)
            ob.add_order(Order(i, 1, 1000, 100000 + i, 100, Side::Buy));
        auto r = Benchmark::run("find (50000-level book)", [&]() {
            volatile auto* found = ob.find_order(25000);
            (void)found;
        }, 10000, false);
        r.print("OrderBook: find");
    }
    {
        OrderBook ob;
        for (int i = 0; i < 50000; i++)
            ob.add_order(Order(i, 1, 1000, 100000 + i, 100, Side::Buy));
        auto r = Benchmark::run("remove (50000-level book)", [&]() {
            ob.remove_order(0);
            ob.add_order(Order(0, 1, 1000, 100000, 100, Side::Buy));
        }, 10000, false);
        r.print("OrderBook: remove");
    }

    // ===== Matching Engine =====
    std::cout << "\n=== Matching Engine Benchmarks ===\n";
    std::cout << std::string(60, '-') << "\n";

    {
        MatchingEngine engine;
        auto r = Benchmark::run("single limit (no match)", [&]() {
            uint64_t id = engine.order_book().size() + 1;
            engine.process_order(Order(id, 1, 1000, 100000, 100, Side::Buy));
        }, 5000, false);
        r.print("MatchingEngine: single limit (no match)");
    }
    {
        MatchingEngine engine;
        for (int i = 0; i < 5000; i++)
            engine.process_order(Order(i, 1, 1000, 100000, 100, Side::Buy));
        auto r = Benchmark::run("market vs deep book", [&]() {
            uint64_t id = engine.order_book().size() + 1;
            engine.process_order(Order(id, 1, 1001, 100, 100, Side::Sell));
        }, 500, false);
        r.print("MatchingEngine: market vs deep book");
    }
    {
        MatchingEngine engine;
        auto r = Benchmark::run("10000 alternating orders", [&]() {
            for (int i = 0; i < 5000; i++) {
                engine.process_order(Order(i*2, 1, 1000, 100000 + (i % 100), 100, Side::Buy));
                engine.process_order(Order(i*2+1, 1, 1001, 100000 + (i % 100), 100, Side::Sell));
            }
        }, 20, false);
        r.print("MatchingEngine: 10000 alternating orders");
    }
    {
        MatchingEngine engine;
        for (int i = 0; i < 10000; i++)
            engine.process_order(Order(i, 1, 1000, 100000, 100, Side::Buy));
        auto r = Benchmark::run("1000 market orders", [&]() {
            for (int i = 0; i < 1000; i++)
                engine.process_order(Order(10000 + i, 1, 1001, 100000, 100, Side::Sell));
        }, 30, false);
        r.print("MatchingEngine: 1000 market orders (filled)");
    }

    // ===== Lock-Free Queue =====
    std::cout << "\n=== Lock-Free Queue Benchmarks ===\n";
    std::cout << std::string(60, '-') << "\n";

    {
        LockFreeQueue<int> q;
        auto r = Benchmark::run("10000 enqueue+dequeue", [&]() {
            int v;
            for (int i = 0; i < 10000; i++) q.enqueue(i);
            for (int i = 0; i < 10000; i++) q.dequeue(v);
        }, 100, false);
        r.print("LockFreeQueue: 10000 enqueue+dequeue");
    }
    {
        LockFreeQueue<int> q;
        auto r = Benchmark::run("single enq+deq latency", [&]() {
            q.enqueue(42);
            int v;
            q.dequeue(v);
        }, 50000, false);
        r.print("LockFreeQueue: single enqueue+dequeue");
    }

    // ===== Object Pool =====
    std::cout << "\n=== Object Pool Benchmarks ===\n";
    std::cout << std::string(60, '-') << "\n";

    {
        OrderPool pool(100000);
        auto r = Benchmark::run("10000 create+destroy", [&]() {
            for (int i = 0; i < 10000; i++) {
                Order* o = pool.create(i, 1, 1000, 100000, 100, Side::Buy);
                pool.destroy(o);
            }
        }, 100, false);
        r.print("OrderPool: 10000 create+destroy");
    }
    {
        AlignedOrderPool apool(100000);
        auto r = Benchmark::run("AlignedPool 10000 create+destroy", [&]() {
            for (int i = 0; i < 10000; i++) {
                Order* o = apool.create(i, 1, 1000, 100000, 100, Side::Buy);
                apool.destroy(o);
            }
        }, 100, false);
        r.print("AlignedOrderPool: 10000 create+destroy");
    }

    // ===== EventStore =====
    std::cout << "\n=== EventStore Benchmarks ===\n";
    std::cout << std::string(60, '-') << "\n";

    {
        EventStore store;
        EmptyCallback cb;
        store.subscribe_trades(cb);
        store.subscribe_events(cb);
        auto r = Benchmark::run("publish 10000 events", [&]() {
            for (int i = 0; i < 10000; i++) {
                ExchangeEvent e(store.last_sequence() + 1,
                    EventType::OrderAccepted, i, 100000, 100, 0, Side::Buy);
                store.publish(e);
            }
        }, 20, false);
        r.print("EventStore: publish 10000 events");
    }
    {
        EventStore store;
        for (uint64_t i = 0; i < 100000; i++)
            store.publish(ExchangeEvent(i, EventType::OrderAccepted, i, 100000, 100, 0, Side::Buy));
        uint64_t total = 0;
        auto r = Benchmark::run("replay 100000 events", [&]() {
            total = 0;
            store.replay(1, 100000, [&](const auto&) { total++; });
        }, 20, false);
        r.print("EventStore: replay 100000 events");
    }

    // ===== ThreadPool =====
    std::cout << "\n=== ThreadPool Benchmarks ===\n";
    std::cout << std::string(60, '-') << "\n";

    {
        ThreadPool pool(4);
        std::atomic<uint64_t> counter{0};
        // Manual warmup: 3 iterations
        for (int w = 0; w < 3; w++) {
            for (int i = 0; i < 10000; i++)
                pool.enqueue([&]() { counter.fetch_add(1, std::memory_order_relaxed); });
            pool.wait_all();
            counter.store(0);
        }
        auto r = Benchmark::run("submit 10000 tasks (pool 4)", [&]() {
            for (int i = 0; i < 10000; i++)
                pool.enqueue([&]() { counter.fetch_add(1, std::memory_order_relaxed); });
            pool.wait_all();
            counter.store(0);
        }, 10, false);
        r.print("ThreadPool: submit 10000 tasks");
    }

    // ===== Risk Manager =====
    std::cout << "\n=== Risk Manager Benchmarks ===\n";
    std::cout << std::string(60, '-') << "\n";

    {
        RiskLimits limits;
        limits.max_order_size = 100000;
        RiskManager rm(limits);
        Order order(1, 1, 1000, 100000, 100, Side::Buy);
        auto r = Benchmark::run("check_order latency", [&]() {
            rm.check_order(order);
        }, 100000, false);
        r.print("RiskManager: check_order latency");
    }

    // ===== Single-Operation Latency =====
    std::cout << "\n=== Single-Operation Latency ===\n";
    std::cout << std::string(60, '-') << "\n";

    {
        OrderBook ob;
        ob.add_order(Order(1, 1, 1000, 100000, 100, Side::Buy));
        auto r = Benchmark::run_no_overhead("find (1-level book)", [&]() {
            volatile auto* f = ob.find_order(1);
            (void)f;
        }, 10000);
        r.print("OrderBook: find (1-level)");
    }
    {
        OrderBook ob;
        for (int i = 0; i < 1000; i++)
            ob.add_order(Order(i, 1, 1000, 100000 + i, 100, Side::Buy));
        auto r = Benchmark::run_no_overhead("find (1000-level book)", [&]() {
            volatile auto* f = ob.find_order(500);
            (void)f;
        }, 10000);
        r.print("OrderBook: find (1000-level)");
    }
    {
        OrderBook ob;
        auto r = Benchmark::run_no_overhead("add+clear (empty ob)", [&]() {
            ob.add_order(Order(1, 1, 1000, 100000, 100, Side::Buy));
            ob.clear();
        }, 10000);
        r.print("OrderBook: add+clear (empty)");
    }
    {
        MatchingEngine engine;
        auto r = Benchmark::run_no_overhead("process limit (no match)", [&]() {
            uint64_t id = engine.order_book().size() + 1;
            engine.process_order(Order(id, 1, 1000, 100000, 100, Side::Buy));
        }, 10000);
        r.print("MatchingEngine: process (no match)");
    }

    std::cout << "\nBenchmarks complete!\n";
    return 0;
}
