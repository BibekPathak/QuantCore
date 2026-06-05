#include "hft/order_book.hpp"
#include "hft/matching_engine.hpp"
#include "hft/lock_free_queue.hpp"
#include "hft/object_pool.hpp"
#include "hft/risk_manager.hpp"
#include "hft/benchmark.hpp"
#include <iostream>
#include <iomanip>
#include <atomic>
#include <vector>

using namespace hft;

int main() {
    std::cout << "HFT Trading System - Benchmarks\n";
    std::cout << "================================\n\n";

    std::cout << "=== Order Book Benchmarks ===\n";
    std::cout << std::string(60, '-') << "\n";

    auto ob_result = Benchmark::run("OrderBook: 1000 add orders", [&]() {
        OrderBook local_ob;
        for (int i = 0; i < 1000; i++) {
            local_ob.add_order(Order(i, 1, 1000, 100000 + i, 100, Side::Buy));
        }
    }, 1000);
    ob_result.print("OrderBook: 1000 add orders");

    auto ob_find = Benchmark::run("OrderBook: 1000 find orders", [&]() {
        OrderBook local_ob;
        for (int i = 0; i < 1000; i++) {
            local_ob.add_order(Order(i, 1, 1000, 100000 + i, 100, Side::Buy));
        }
        for (int i = 0; i < 1000; i++) {
            local_ob.find_order(i);
        }
    }, 1000);
    ob_find.print("OrderBook: 1000 find orders");

    std::cout << "\n=== Matching Engine Benchmarks ===\n";
    std::cout << std::string(60, '-') << "\n";

    auto me_result = Benchmark::run("MatchingEngine: 1000 limit orders", [&]() {
        MatchingEngine engine;
        for (int i = 0; i < 1000; i++) {
            engine.process_order(Order(i, 1, 1000, 100000 + i, 100, Side::Buy));
        }
    }, 1000);
    me_result.print("MatchingEngine: 1000 limit orders");

    auto me_match = Benchmark::run("MatchingEngine: 1000 orders with matches", [&]() {
        MatchingEngine engine;
        for (int i = 0; i < 500; i++) {
            engine.process_order(Order(i, 1, 1000, 100000, 100, Side::Buy));
        }
        for (int i = 500; i < 1000; i++) {
            engine.process_order(Order(i, 1, 1001, 100000, 100, Side::Sell));
        }
    }, 1000);
    me_match.print("MatchingEngine: 1000 orders with matches");

    std::cout << "\n=== Lock-Free Queue Benchmarks ===\n";
    std::cout << std::string(60, '-') << "\n";

    auto lf_result = Benchmark::run("LockFreeQueue: 10000 enqueue", [&]() {
        LockFreeQueue<int> q;
        for (int i = 0; i < 10000; i++) {
            q.enqueue(i);
        }
    }, 100);
    lf_result.print("LockFreeQueue: 10000 enqueue");

    auto lf_deq = Benchmark::run("LockFreeQueue: 10000 enqueue+dequeue", [&]() {
        LockFreeQueue<int> q;
        int val;
        for (int i = 0; i < 5000; i++) {
            q.enqueue(i);
        }
        for (int i = 0; i < 5000; i++) {
            q.dequeue(val);
        }
    }, 100);
    lf_deq.print("LockFreeQueue: 10000 enqueue+dequeue");

    std::cout << "\n=== Object Pool Benchmarks ===\n";
    std::cout << std::string(60, '-') << "\n";

    auto pool_result = Benchmark::run("OrderPool: 1000 create/destroy", [&]() {
        OrderPool pool(10000);
        for (int i = 0; i < 1000; i++) {
            Order* o = pool.create(i, 1, 1000, 100000, 100, Side::Buy);
            pool.destroy(o);
        }
    }, 100);
    pool_result.print("OrderPool: 1000 create/destroy");

    std::cout << "\n=== Risk Manager Benchmarks ===\n";
    std::cout << std::string(60, '-') << "\n";

    auto risk_result = Benchmark::run("RiskManager: 10000 checks", [&]() {
        RiskLimits limits;
        limits.max_order_size = 100000;
        RiskManager rm(limits);
        for (int i = 0; i < 10000; i++) {
            Order order(i, 1, 1000, 100000, 100, Side::Buy);
            rm.check_order(order);
        }
    }, 100);
    risk_result.print("RiskManager: 10000 checks");

    std::cout << "\n=== Single-Operation Latency ===\n";
    std::cout << std::string(60, '-') << "\n";

    auto lat_add = Benchmark::run_no_overhead("OrderBook add (ns)", [&]() {
        OrderBook ob;
        ob.add_order(Order(1, 1, 1000, 100000, 100, Side::Buy));
    }, 100000);
    lat_add.print("OrderBook add");

    auto lat_find = Benchmark::run_no_overhead("OrderBook find (ns)", [&]() {
        OrderBook ob;
        ob.add_order(Order(1, 1, 1000, 100000, 100, Side::Buy));
        ob.find_order(1);
    }, 100000);
    lat_find.print("OrderBook find");

    auto lat_me = Benchmark::run_no_overhead("MatchingEngine process (ns)", [&]() {
        MatchingEngine engine;
        engine.process_order(Order(1, 1, 1000, 100000, 100, Side::Buy));
    }, 100000);
    lat_me.print("MatchingEngine process");

    std::cout << "\nBenchmarks complete!\n";
    return 0;
}
