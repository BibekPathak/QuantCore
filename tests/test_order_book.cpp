#include "hft/order_book.hpp"
#include "hft/matching_engine.hpp"
#include "hft/lock_free_queue.hpp"
#include "hft/thread_safe_matching_engine.hpp"
#include "hft/thread_pool.hpp"
#include "hft/object_pool.hpp"
#include "hft/risk_manager.hpp"
#include "hft/strategy.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

using namespace hft;

void test_order_book_basic() {
    std::cout << "Test: OrderBook Basic... ";
    
    OrderBook ob;
    
    Order buy1(1, 1, 1000, 100000, 100, Side::Buy);
    Order buy2(2, 1, 1001, 100100, 100, Side::Buy);
    Order sell1(3, 1, 1002, 100200, 100, Side::Sell);
    
    ob.add_order(buy1);
    ob.add_order(buy2);
    ob.add_order(sell1);
    
    assert(ob.best_bid() == 100100);
    assert(ob.best_ask() == 100200);
    assert(ob.has_bids() == true);
    assert(ob.has_asks() == true);
    
    std::cout << "PASSED\n";
}

void test_order_book_match() {
    std::cout << "Test: OrderBook Match... ";
    
    OrderBook ob;
    
    Order buy(1, 1, 1000, 100500, 100, Side::Buy);
    Order sell(2, 1, 1001, 100400, 100, Side::Sell);
    
    ob.add_order(buy);
    ob.add_order(sell);
    
    assert(ob.can_match() == true);
    
    std::cout << "PASSED\n";
}

void test_matching_engine_simple() {
    std::cout << "Test: MatchingEngine Simple... ";
    
    MatchingEngine engine;
    
    Order buy(1, 1, 1000, 100500, 100, Side::Buy);
    Order sell(2, 1, 1001, 100400, 100, Side::Sell);
    
    auto trades = engine.process_order(buy);
    assert(trades.empty());
    
    trades = engine.process_order(sell);
    assert(trades.size() == 1);
    assert(trades[0].price == 100500);
    assert(trades[0].quantity == 100);
    
    std::cout << "PASSED\n";
}

void test_matching_engine_partial_fill() {
    std::cout << "Test: MatchingEngine Partial Fill... ";
    
    MatchingEngine engine;
    
    Order buy(1, 1, 1000, 100500, 200, Side::Buy);
    Order sell(2, 1, 1001, 100400, 100, Side::Sell);
    
    engine.process_order(buy);
    auto trades = engine.process_order(sell);
    
    assert(trades.size() == 1);
    assert(trades[0].quantity == 100);
    assert(engine.order_book().has_bids() == true);
    
    auto* remaining = engine.order_book().get_best_bid();
    assert(remaining->remaining_quantity() == 100);
    
    std::cout << "PASSED\n";
}

void test_matching_engine_multiple_matches() {
    std::cout << "Test: MatchingEngine Multiple Matches... ";
    
    MatchingEngine engine;
    
    Order buy1(1, 1, 1000, 100500, 50, Side::Buy);
    Order buy2(2, 1, 1001, 100500, 50, Side::Buy);
    
    Order sell(3, 1, 1002, 100400, 80, Side::Sell);
    
    engine.process_order(buy1);
    engine.process_order(buy2);
    auto trades = engine.process_order(sell);
    
    assert(trades.size() == 2);
    assert(trades[0].quantity == 50);
    assert(trades[1].quantity == 30);
    
    std::cout << "PASSED\n";
}

void test_market_order() {
    std::cout << "Test: Market Order... ";
    
    MatchingEngine engine;
    
    Order sell1(1, 1, 1000, 100400, 100, Side::Sell);
    Order sell2(2, 1, 1001, 100500, 100, Side::Sell);
    
    engine.process_order(sell1);
    engine.process_order(sell2);
    
    Order market_buy(3, 1, 1002, 0, 150, Side::Buy, OrderType::Market);
    auto trades = engine.process_order(market_buy);
    
    assert(trades.size() == 2);
    assert(trades[0].price == 100400);
    assert(trades[1].price == 100500);
    assert(trades[0].quantity == 100);
    assert(trades[1].quantity == 50);
    
    std::cout << "PASSED\n";
}

void test_lock_free_queue_basic() {
    std::cout << "Test: LockFreeQueue Basic... ";
    
    LockFreeQueue<int> q;
    int val;
    
    assert(q.is_empty());
    
    q.enqueue(1);
    q.enqueue(2);
    q.enqueue(3);
    
    assert(!q.is_empty());
    
    assert(q.dequeue(val));
    assert(val == 1);
    
    assert(q.dequeue(val));
    assert(val == 2);
    
    assert(q.dequeue(val));
    assert(val == 3);
    
    assert(!q.dequeue(val));
    assert(q.is_empty());
    
    std::cout << "PASSED\n";
}

void test_spsc_queue() {
    std::cout << "Test: SPSCQueue... ";
    
    SPSCQueue<int> q(1024);
    int val;
    
    for (int i = 0; i < 100; i++) {
        assert(q.enqueue(i));
    }
    
    assert(q.size() == 100);
    
    for (int i = 0; i < 50; i++) {
        assert(q.dequeue(val));
        assert(val == i);
    }
    
    assert(q.size() == 50);
    
    std::cout << "PASSED\n";
}

void test_thread_safe_engine() {
    std::cout << "Test: ThreadSafeMatchingEngine... ";
    
    ThreadSafeMatchingEngine engine;
    
    Order buy(1, 1, 1000, 100500, 100, Side::Buy);
    Order sell(2, 1, 1001, 100400, 100, Side::Sell);
    
    assert(engine.enqueue_order(buy));
    assert(engine.enqueue_order(sell));
    
    assert(engine.pending_orders() == 2);
    
    engine.process_all_pending();
    
    assert(engine.pending_orders() == 0);
    assert(engine.total_trades() == 1);
    assert(engine.orders_processed() == 2);
    
    std::cout << "PASSED\n";
}

void test_multi_producer() {
    std::cout << "Test: Multi-Producer... ";
    
    ThreadSafeMatchingEngine engine(65536);
    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};
    
    auto producer = [&](int start, int count) {
        for (int i = 0; i < count; i++) {
            Order order(start + i, 1, 1000, 100000 + (start + i), 10, Side::Buy);
            if (engine.enqueue_order(order)) {
                success_count++;
            } else {
                fail_count++;
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(producer, i * 1000, 1000);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    engine.process_all_pending();
    
    assert(engine.orders_processed() == success_count.load());
    
    std::cout << "PASSED (" << success_count.load() << " orders)\n";
}

void test_thread_pool() {
    std::cout << "Test: ThreadPool... ";
    
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    
    for (int i = 0; i < 100; i++) {
        pool.enqueue([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
    
    pool.wait_all();
    assert(counter.load() == 100);
    
    std::cout << "PASSED\n";
}

void test_thread_pool_batched() {
    std::cout << "Test: BatchedThreadPool... ";
    
    BatchedThreadPool pool(4, 10);
    std::vector<int> items(100);
    for (int i = 0; i < 100; i++) items[i] = i;
    
    auto futures = pool.map_async([](int x) { return x * 2; }, items);
    
    std::vector<int> results;
    results.reserve(100);
    for (auto& f : futures) {
        results.push_back(f.get());
    }
    
    assert(results.size() == 100);
    assert(results[50] == 100);
    
    std::cout << "PASSED\n";
}

void test_order_pool() {
    std::cout << "Test: OrderPool... ";
    
    OrderPool pool(1000);
    assert(pool.available() == 1000);
    
    Order* order = pool.create(1, 1, 1000, 100000, 100, Side::Buy);
    assert(order != nullptr);
    assert(pool.available() == 999);
    
    pool.destroy(order);
    assert(pool.available() == 1000);
    
    std::cout << "PASSED\n";
}

void test_trade_pool() {
    std::cout << "Test: TradePool... ";
    
    TradePool pool(1000);
    assert(pool.available() == 1000);
    
    Trade* trade = pool.create(1, 2, 3, 1000, 100000, 100, Side::Buy);
    assert(trade != nullptr);
    assert(pool.available() == 999);
    
    pool.destroy(trade);
    assert(pool.available() == 1000);
    
    std::cout << "PASSED\n";
}

void test_object_pool_performance() {
    std::cout << "Test: ObjectPool Performance... ";
    
    OrderPool pool(10000);
    
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<Order*> orders;
    orders.reserve(10000);
    
    for (int i = 0; i < 10000; i++) {
        orders.push_back(pool.create(i, 1, 1000, 100000, 100, Side::Buy));
    }
    
    for (auto* o : orders) {
        pool.destroy(o);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    assert(pool.available() == 10000);
    assert(orders.size() == 10000);
    
    std::cout << "PASSED (" << ns/1000 << "us for 10k alloc/dealloc)\n";
}

void test_cancel_order() {
    std::cout << "Test: Cancel Order... ";
    
    OrderBook ob;
    
    Order buy1(1, 1, 1000, 100000, 100, Side::Buy);
    Order buy2(2, 1, 1001, 100100, 100, Side::Buy);
    
    ob.add_order(buy1);
    ob.add_order(buy2);
    
    assert(ob.size() == 2);
    
    bool removed = ob.remove_order(1);
    assert(removed == true);
    assert(ob.size() == 1);
    
    assert(ob.find_order(1) == nullptr);
    assert(ob.find_order(2) != nullptr);
    
    std::cout << "PASSED\n";
}

void test_amend_order() {
    std::cout << "Test: Amend Order... ";
    
    OrderBook ob;
    
    Order buy(1, 1, 1000, 100000, 100, Side::Buy);
    ob.add_order(buy);
    
    assert(ob.find_order(1)->remaining_quantity() == 100);
    
    bool amended = ob.modify_quantity(1, 200);
    assert(amended == true);
    assert(ob.find_order(1)->remaining_quantity() == 200);
    
    amended = ob.modify_quantity(1, 50);
    assert(amended == true);
    assert(ob.find_order(1)->remaining_quantity() == 50);
    
    amended = ob.modify_quantity(999, 100);
    assert(amended == false);
    
    std::cout << "PASSED\n";
}

void test_matching_engine_cancel() {
    std::cout << "Test: MatchingEngine Cancel... ";
    
    MatchingEngine engine;
    
    Order buy1(1, 1, 1000, 100000, 100, Side::Buy);
    Order buy2(2, 1, 1001, 100100, 100, Side::Buy);
    
    engine.process_order(buy1);
    engine.process_order(buy2);
    
    assert(engine.order_book().size() == 2);
    
    bool cancelled = engine.cancel_order(1);
    assert(cancelled == true);
    assert(engine.order_book().size() == 1);
    
    bool amended = engine.amend_order(2, 300);
    assert(amended == true);
    assert(engine.order_book().find_order(2)->remaining_quantity() == 300);
    
    std::cout << "PASSED\n";
}

void test_risk_manager_basic() {
    std::cout << "Test: RiskManager Basic... ";
    
    RiskManager rm;
    
    Order order(1, 1, 1000, 100000, 100, Side::Buy);
    
    auto result = rm.check_order(order);
    assert(result.approved == true);
    
    std::cout << "PASSED\n";
}

void test_risk_manager_order_size() {
    std::cout << "Test: RiskManager Order Size... ";
    
    RiskLimits limits;
    limits.max_order_size = 100;
    RiskManager rm(limits);
    
    Order small_order(1, 1, 1000, 100000, 50, Side::Buy);
    assert(rm.check_order(small_order).approved == true);
    
    Order big_order(2, 1, 1000, 100000, 200, Side::Buy);
    assert(rm.check_order(big_order).approved == false);
    
    std::cout << "PASSED\n";
}

void test_risk_manager_position() {
    std::cout << "Test: RiskManager Position... ";
    
    RiskLimits limits;
    limits.max_position = 500;
    RiskManager rm(limits);
    
    Order buy1(1, 1, 1000, 100000, 200, Side::Buy);
    Order buy2(2, 1, 1000, 100000, 200, Side::Buy);
    Order buy3(3, 1, 1000, 100000, 200, Side::Buy);
    
    assert(rm.check_position(buy1, 0).approved == true);
    rm.record_trade({1, 1, 2, 1000, 100000, 200, Side::Buy});
    
    assert(rm.check_position(buy2, rm.net_position()).approved == true);
    rm.record_trade({2, 2, 3, 1000, 100000, 200, Side::Buy});
    
    assert(rm.check_position(buy3, rm.net_position()).approved == false);
    
    std::cout << "PASSED\n";
}

void test_risk_checked_engine() {
    std::cout << "Test: RiskCheckedMatchingEngine... ";
    
    RiskLimits limits;
    limits.max_order_size = 100;
    RiskCheckedMatchingEngine engine(limits);
    
    Order valid_order(1, 1, 1000, 100000, 50, Side::Buy);
    bool processed = engine.process_order(valid_order);
    assert(processed == true);
    
    Order invalid_order(2, 1, 1000, 100000, 200, Side::Buy);
    processed = engine.process_order(invalid_order);
    assert(processed == false);
    
    std::cout << "PASSED\n";
}

void test_order_book_empty() {
    std::cout << "Test: OrderBook Empty... ";
    
    OrderBook ob;
    assert(ob.is_empty());
    assert(ob.size() == 0);
    assert(!ob.has_bids());
    assert(!ob.has_asks());
    assert(ob.best_bid() == 0);
    assert(ob.best_ask() == std::numeric_limits<int64_t>::max());
    assert(ob.find_order(1) == nullptr);
    
    std::cout << "PASSED\n";
}

void test_order_book_single_order() {
    std::cout << "Test: OrderBook Single Order... ";
    
    OrderBook ob;
    Order buy(1, 1, 1000, 100000, 100, Side::Buy);
    ob.add_order(buy);
    
    assert(ob.size() == 1);
    assert(ob.has_bids());
    assert(!ob.has_asks());
    assert(ob.best_bid() == 100000);
    assert(ob.best_bid_size() == 100);
    
    std::cout << "PASSED\n";
}

void test_order_book_multiple_levels() {
    std::cout << "Test: OrderBook Multiple Levels... ";
    
    OrderBook ob;
    ob.add_order(Order(1, 1, 1000, 100000, 100, Side::Buy));
    ob.add_order(Order(2, 1, 1001, 100100, 200, Side::Buy));
    ob.add_order(Order(3, 1, 1002, 100200, 150, Side::Buy));
    
    assert(ob.bid_levels() == 3);
    assert(ob.best_bid() == 100200);
    assert(ob.best_bid_size() == 150);
    assert(ob.total_bid_volume() == 450);
    
    std::cout << "PASSED\n";
}

void test_order_book_remove_best() {
    std::cout << "Test: OrderBook Remove Best... ";
    
    OrderBook ob;
    ob.add_order(Order(1, 1, 1000, 100000, 100, Side::Buy));
    ob.add_order(Order(2, 1, 1001, 100100, 100, Side::Buy));
    
    auto removed = ob.pop_best_bid();
    assert(removed.has_value());
    assert(removed->order_id == 2);
    assert(ob.best_bid() == 100000);
    
    std::cout << "PASSED\n";
}

void test_order_book_clear() {
    std::cout << "Test: OrderBook Clear... ";
    
    OrderBook ob;
    ob.add_order(Order(1, 1, 1000, 100000, 100, Side::Buy));
    ob.add_order(Order(2, 1, 1000, 100100, 100, Side::Buy));
    ob.add_order(Order(3, 1, 1000, 100000, 100, Side::Sell));
    
    assert(ob.size() == 3);
    
    ob.clear();
    assert(ob.is_empty());
    assert(ob.size() == 0);
    
    std::cout << "PASSED\n";
}

void test_matching_engine_no_match() {
    std::cout << "Test: MatchingEngine No Match... ";
    
    MatchingEngine engine;
    
    Order buy(1, 1, 1000, 100000, 100, Side::Buy);
    Order sell(2, 1, 1001, 100200, 100, Side::Sell);
    
    engine.process_order(buy);
    auto trades = engine.process_order(sell);
    
    assert(trades.empty());
    assert(engine.order_book().size() == 2);
    
    std::cout << "PASSED\n";
}

void test_matching_engine_exact_match() {
    std::cout << "Test: MatchingEngine Exact Match... ";
    
    MatchingEngine engine;
    
    Order buy(1, 1, 1000, 100000, 100, Side::Buy);
    Order sell(2, 1, 1001, 100000, 100, Side::Sell);
    
    engine.process_order(buy);
    auto trades = engine.process_order(sell);
    
    assert(trades.size() == 1);
    assert(trades[0].quantity == 100);
    assert(engine.order_book().is_empty());
    
    std::cout << "PASSED\n";
}

void test_matching_engine_multi_level_match() {
    std::cout << "Test: MatchingEngine Multi-Level Match... ";
    
    MatchingEngine engine;
    
    engine.process_order(Order(1, 1, 1000, 100000, 50, Side::Sell));
    engine.process_order(Order(2, 1, 1001, 100000, 50, Side::Sell));
    engine.process_order(Order(3, 1, 1002, 100000, 50, Side::Sell));
    
    Order buy(4, 1, 1003, 100000, 100, Side::Buy);
    auto trades = engine.process_order(buy);
    
    assert(trades.size() == 2);
    assert(trades[0].quantity == 50);
    assert(trades[1].quantity == 50);
    
    std::cout << "PASSED\n";
}

void test_matching_engine_trade_ids() {
    std::cout << "Test: MatchingEngine Trade IDs... ";
    
    MatchingEngine engine;
    
    engine.process_order(Order(1, 1, 1000, 100000, 50, Side::Buy));
    engine.process_order(Order(2, 1, 1001, 100000, 50, Side::Sell));
    engine.process_order(Order(3, 1, 1002, 100000, 50, Side::Buy));
    engine.process_order(Order(4, 1, 1003, 100000, 50, Side::Sell));
    
    assert(engine.total_trades() == 2);
    
    std::cout << "PASSED\n";
}

void test_spsc_queue_stress() {
    std::cout << "Test: SPSCQueue Stress... ";
    
    SPSCQueue<int> q(16384);
    
    for (int i = 0; i < 10000; i++) {
        assert(q.enqueue(i));
    }
    
    assert(q.size() == 10000);
    
    int val;
    for (int i = 0; i < 10000; i++) {
        assert(q.dequeue(val));
        assert(val == i);
    }
    
    assert(q.is_empty());
    
    std::cout << "PASSED\n";
}

void test_lock_free_queue_stress() {
    std::cout << "Test: LockFreeQueue Stress... ";
    
    LockFreeQueue<int> q;
    int val;
    
    for (int i = 0; i < 10000; i++) {
        q.enqueue(i);
    }
    
    for (int i = 0; i < 10000; i++) {
        assert(q.dequeue(val));
        assert(val == i);
    }
    
    assert(q.is_empty());
    
    std::cout << "PASSED\n";
}

void test_thread_pool_stress() {
    std::cout << "Test: ThreadPool Stress... ";
    
    ThreadPool pool(8);
    std::atomic<int> counter{0};
    
    for (int i = 0; i < 1000; i++) {
        pool.enqueue([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
    
    pool.wait_all();
    assert(counter.load() == 1000);
    
    std::cout << "PASSED\n";
}

void test_order_pool_stress() {
    std::cout << "Test: OrderPool Stress... ";
    
    OrderPool pool(10000);
    
    std::vector<Order*> orders;
    for (int i = 0; i < 10000; i++) {
        orders.push_back(pool.create(i, 1, 1000, 100000, 100, Side::Buy));
    }
    
    for (auto* o : orders) {
        pool.destroy(o);
    }
    
    assert(pool.available() == 10000);
    
    std::cout << "PASSED\n";
}

void test_order_book_price_levels() {
    std::cout << "Test: OrderBook Price Levels... ";
    
    OrderBook ob;
    
    for (int i = 0; i < 10; i++) {
        ob.add_order(Order(i, 1, 1000 + i, 100000, 100, Side::Buy));
    }
    
    assert(ob.bid_levels() == 1);
    
    ob.add_order(Order(100, 1, 2000, 100100, 100, Side::Buy));
    assert(ob.bid_levels() == 2);
    
    std::cout << "PASSED\n";
}

void test_order_book_ask_levels() {
    std::cout << "Test: OrderBook Ask Levels... ";
    
    OrderBook ob;
    
    for (int i = 0; i < 10; i++) {
        ob.add_order(Order(i, 1, 1000 + i, 100000, 100, Side::Sell));
    }
    
    assert(ob.ask_levels() == 1);
    
    ob.add_order(Order(100, 1, 2000, 100100, 100, Side::Sell));
    assert(ob.ask_levels() == 2);
    
    std::cout << "PASSED\n";
}

void test_matching_engine_order_idempotency() {
    std::cout << "Test: MatchingEngine Order Idempotency... ";
    
    MatchingEngine engine;
    
    Order buy(1, 1, 1000, 100000, 100, Side::Buy);
    
    auto trades1 = engine.process_order(buy);
    assert(trades1.empty());
    assert(engine.order_book().size() == 1);
    
    auto trades2 = engine.process_order(buy);
    assert(trades2.empty());
    assert(engine.order_book().size() == 1);
    
    std::cout << "PASSED\n";
}

void test_matching_engine_fill_price() {
    std::cout << "Test: MatchingEngine Fill Price... ";
    
    MatchingEngine engine;
    
    Order sell(1, 1, 1000, 100000, 100, Side::Sell);
    engine.process_order(sell);
    
    Order buy(2, 1, 1001, 100100, 50, Side::Buy);
    auto trades = engine.process_order(buy);
    
    assert(trades.size() == 1);
    assert(trades[0].price == 100000);
    
    std::cout << "PASSED\n";
}

void test_matching_engine_partial_fill_full() {
    std::cout << "Test: MatchingEngine Partial Fill Full... ";
    
    MatchingEngine engine;
    
    engine.process_order(Order(1, 1, 1000, 100000, 50, Side::Buy));
    
    Order sell(2, 1, 1001, 100000, 100, Side::Sell);
    auto trades = engine.process_order(sell);
    
    assert(trades.size() == 1);
    assert(trades[0].quantity == 50);
    assert(engine.order_book().has_asks());
    assert(!engine.order_book().is_empty());
    
    std::cout << "PASSED\n";
}

void test_risk_manager_reset() {
    std::cout << "Test: RiskManager Reset... ";
    
    RiskLimits limits;
    limits.max_position = 100;
    RiskManager rm(limits);
    
    rm.record_trade({1, 1, 2, 1000, 100000, 50, Side::Buy});
    assert(rm.net_position() == 50);
    assert(rm.order_count() == 1);
    
    rm.reset_daily();
    assert(rm.daily_pnl() == 0);
    assert(rm.order_count() == 0);
    assert(rm.net_position() == 50);
    
    std::cout << "PASSED\n";
}

void test_risk_manager_daily_loss() {
    std::cout << "Test: RiskManager Daily Loss... ";
    
    RiskLimits limits;
    limits.max_daily_loss = -1000;
    RiskManager rm(limits);
    
    rm.record_trade({1, 1, 2, 1000, 100000, 50, Side::Sell});
    assert(rm.daily_pnl() > 0);
    
    auto check = rm.check_daily_loss();
    assert(check.approved == true);
    
    limits.max_daily_loss = -10000000000;
    rm.set_limits(limits);
    check = rm.check_daily_loss();
    assert(check.approved == true);
    
    std::cout << "PASSED\n";
}

void test_matching_engine_reset() {
    std::cout << "Test: MatchingEngine Reset... ";
    
    MatchingEngine engine;
    
    engine.process_order(Order(1, 1, 1000, 100000, 100, Side::Buy));
    engine.process_order(Order(2, 1, 1001, 100200, 100, Side::Sell));
    
    assert(engine.order_book().size() == 2);
    assert(engine.total_trades() == 0);
    
    engine.reset();
    assert(engine.order_book().is_empty());
    assert(engine.total_trades() == 0);
    
    std::cout << "PASSED\n";
}

void test_order_book_modify() {
    std::cout << "Test: OrderBook Modify... ";
    
    OrderBook ob;
    ob.add_order(Order(1, 1, 1000, 100000, 100, Side::Buy));
    
    bool mod = ob.modify_quantity(1, 200);
    assert(mod == true);
    assert(ob.find_order(1)->remaining_quantity() == 200);
    
    mod = ob.modify_quantity(1, 0);
    assert(mod == false);
    
    mod = ob.modify_quantity(999, 100);
    assert(mod == false);
    
    std::cout << "PASSED\n";
}

void test_thread_safe_engine_drain() {
    std::cout << "Test: ThreadSafeMatchingEngine Drain... ";
    
    ThreadSafeMatchingEngine engine;
    
    for (int i = 0; i < 100; i++) {
        engine.enqueue_order(Order(i, 1, 1000, 100000, 10, Side::Buy));
    }
    
    assert(engine.pending_orders() == 100);
    
    size_t processed = engine.drain_pending();
    assert(processed == 100);
    assert(engine.pending_orders() == 0);
    
    std::cout << "PASSED\n";
}

void test_generic_object_pool() {
    std::cout << "Test: GenericObjectPool... ";
    
    GenericObjectPool<int> pool(100);
    assert(pool.available() == 100);
    
    int* val = pool.create(42);
    assert(*val == 42);
    assert(pool.available() == 99);
    
    pool.destroy(val);
    assert(pool.available() == 100);
    
    std::cout << "PASSED\n";
}

void test_aligned_order_pool() {
    std::cout << "Test: AlignedOrderPool... ";
    
    AlignedOrderPool pool(100);
    assert(pool.available() == 100);
    
    Order* o = pool.create(1, 1, 1000, 100000, 100, Side::Buy);
    assert(o != nullptr);
    assert(pool.available() == 99);
    
    pool.destroy(o);
    assert(pool.available() == 100);
    
    std::cout << "PASSED\n";
}

void test_matching_engine_callback() {
    std::cout << "Test: MatchingEngine Callback... ";
    
    MatchingEngine engine;
    std::atomic<int> trade_count{0};
    
    engine.set_trade_callback([&trade_count](const Trade&) {
        trade_count.fetch_add(1);
    });
    
    engine.process_order(Order(1, 1, 1000, 100000, 50, Side::Buy));
    engine.process_order(Order(2, 1, 1001, 100000, 50, Side::Sell));
    
    assert(trade_count.load() == 1);
    
    std::cout << "PASSED\n";
}

void test_order_book_get_levels() {
    std::cout << "Test: OrderBook Get Levels... ";
    
    OrderBook ob;
    ob.add_order(Order(1, 1, 1000, 100000, 100, Side::Buy));
    ob.add_order(Order(2, 1, 1001, 100100, 100, Side::Buy));
    ob.add_order(Order(3, 1, 1002, 100000, 100, Side::Sell));
    ob.add_order(Order(4, 1, 1003, 100200, 100, Side::Sell));
    
    auto& bids = ob.get_bids();
    auto& asks = ob.get_asks();
    
    assert(bids.size() == 2);
    assert(asks.size() == 2);
    assert(bids[0].price == 100100);
    assert(asks[0].price == 100000);
    
    std::cout << "PASSED\n";
}

void test_matching_engine_market_fills() {
    std::cout << "Test: MatchingEngine Market Fills... ";
    
    MatchingEngine engine;
    
    engine.process_order(Order(1, 1, 1000, 100000, 50, Side::Sell));
    engine.process_order(Order(2, 1, 1001, 100000, 50, Side::Sell));
    
    Order market_buy(3, 1, 1002, 0, 80, Side::Buy, OrderType::Market);
    auto trades = engine.process_order(market_buy);
    
    assert(trades.size() == 2);
    assert(trades[0].quantity == 50);
    assert(trades[1].quantity == 30);
    
    std::cout << "PASSED\n";
}

void test_spsc_queue_wrap() {
    std::cout << "Test: SPSCQueue Wrap... ";
    
    SPSCQueue<int> q(64);
    
    for (int i = 0; i < 32; i++) {
        assert(q.enqueue(i));
    }
    
    assert(q.size() == 32);
    
    int val;
    for (int i = 0; i < 32; i++) {
        assert(q.dequeue(val));
        assert(val == i);
    }
    
    assert(q.is_empty());
    
    std::cout << "PASSED\n";
}

void test_batched_thread_pool() {
    std::cout << "Test: BatchedThreadPool... ";
    
    BatchedThreadPool pool(4, 5);
    std::vector<int> items(25);
    for (int i = 0; i < 25; i++) items[i] = i;
    
    auto futures = pool.map([](int x) { return x * 3; }, items);
    
    std::vector<int> results;
    for (auto& f : futures) {
        auto batch_result = f.get();
        results.insert(results.end(), batch_result.begin(), batch_result.end());
    }
    
    assert(results.size() == 25);
    assert(results[12] == 36);
    
    std::cout << "PASSED\n";
}

void test_stop_loss_order() {
    std::cout << "Test: StopLoss Order... ";
    
    MatchingEngine engine;
    
    engine.process_order(Order(1, 1, 1000, 100000, 100, Side::Sell));
    
    Order buy_stop(2, 1, 1001, 100000, 100, Side::Buy, OrderType::StopLoss);
    buy_stop.stop_price = 100050;
    
    auto trades = engine.process_order(buy_stop);
    assert(trades.empty());
    assert(engine.order_book().pending_stops_count() == 1);
    
    trades = engine.on_market_tick(100050);
    assert(trades.size() == 1);
    assert(engine.order_book().pending_stops_count() == 0);
    
    std::cout << "PASSED\n";
}

void test_stop_loss_not_triggered() {
    std::cout << "Test: StopLoss Not Triggered... ";
    
    MatchingEngine engine;
    
    engine.process_order(Order(1, 1, 1000, 100000, 100, Side::Sell));
    
    Order buy_stop(2, 1, 1001, 100000, 100, Side::Buy, OrderType::StopLoss);
    buy_stop.stop_price = 100100;
    
    auto trades = engine.process_order(buy_stop);
    assert(trades.empty());
    assert(engine.order_book().pending_stops_count() == 1);
    
    trades = engine.on_market_tick(100050);
    assert(trades.empty());
    assert(engine.order_book().pending_stops_count() == 1);
    
    trades = engine.on_market_tick(100100);
    assert(trades.size() == 1);
    assert(engine.order_book().pending_stops_count() == 0);
    
    std::cout << "PASSED\n";
}

void test_stop_limit_order() {
    std::cout << "Test: StopLimit Order... ";
    
    MatchingEngine engine;
    
    Order sell_stop(1, 1, 1000, 100000, 100, Side::Sell, OrderType::StopLimit);
    sell_stop.stop_price = 99900;
    
    auto trades = engine.process_order(sell_stop);
    assert(trades.empty());
    assert(engine.order_book().pending_stops_count() == 1);
    
    trades = engine.on_market_tick(99900);
    assert(trades.empty());
    assert(engine.order_book().pending_stops_count() == 0);
    assert(engine.order_book().has_asks());
    
    std::cout << "PASSED\n";
}

void test_iceberg_order() {
    std::cout << "Test: Iceberg Order... ";
    
    MatchingEngine engine;
    
    Order iceberg(1, 1, 1000, 100000, 1000, Side::Buy, OrderType::Iceberg);
    iceberg.visible_quantity = 100;
    
    auto trades = engine.process_order(iceberg);
    assert(trades.empty());
    
    assert(engine.order_book().size() == 1);
    auto* order = engine.order_book().find_order(1);
    assert(order != nullptr);
    assert(order->remaining_quantity() == 100);
    
    std::cout << "PASSED\n";
}

void test_iceberg_with_match() {
    std::cout << "Test: Iceberg With Match... ";
    
    MatchingEngine engine;
    
    engine.process_order(Order(1, 1, 1000, 100000, 500, Side::Sell));
    
    Order iceberg(2, 1, 1001, 100000, 100, Side::Buy, OrderType::Iceberg);
    iceberg.visible_quantity = 100;
    
    auto trades = engine.process_order(iceberg);
    assert(trades.size() == 1);
    assert(trades[0].quantity == 100);
    assert(!engine.order_book().is_empty());
    
    std::cout << "PASSED\n";
}

void test_oco_link() {
    std::cout << "Test: OCO Link... ";
    
    OrderBook ob;
    
    Order order1(1, 1, 1000, 100000, 100, Side::Buy);
    Order order2(2, 1, 1000, 100200, 100, Side::Sell);
    
    ob.add_order(order1);
    ob.add_order(order2);
    
    ob.link_oco_orders(1, 2);
    
    assert(ob.get_linked_order_id(1) == 2);
    assert(ob.get_linked_order_id(2) == 1);
    assert(ob.oco_links_count() == 1);
    
    std::cout << "PASSED\n";
}

void test_oco_cancel() {
    std::cout << "Test: OCO Cancel... ";
    
    MatchingEngine engine;
    
    engine.link_oco(1, 2);
    
    engine.process_order(Order(1, 1, 1000, 100000, 100, Side::Buy));
    engine.process_order(Order(2, 1, 1000, 100200, 100, Side::Sell));
    
    assert(engine.order_book().size() == 2);
    
    bool cancelled = engine.cancel_order(1);
    assert(cancelled == true);
    assert(engine.order_book().size() == 0);
    
    std::cout << "PASSED\n";
}

void test_oco_fill() {
    std::cout << "Test: OCO Fill... ";
    
    MatchingEngine engine;
    
    engine.link_oco(1, 2);
    
    engine.process_order(Order(1, 1, 1000, 100000, 100, Side::Buy));
    
    engine.process_order(Order(2, 1, 1001, 100000, 100, Side::Sell));
    engine.process_order(Order(3, 1, 1002, 100000, 100, Side::Buy));
    
    assert(engine.order_book().size() == 1);
    
    std::cout << "PASSED\n";
}

void test_order_type_enum() {
    std::cout << "Test: OrderType Enum... ";
    
    assert(static_cast<int>(OrderType::Market) == 0);
    assert(static_cast<int>(OrderType::Limit) == 1);
    assert(static_cast<int>(OrderType::StopLoss) == 2);
    assert(static_cast<int>(OrderType::StopLimit) == 3);
    assert(static_cast<int>(OrderType::Iceberg) == 4);
    
    std::cout << "PASSED\n";
}

void test_order_status_enum() {
    std::cout << "Test: OrderStatus Enum... ";
    
    assert(static_cast<int>(OrderStatus::New) == 0);
    assert(static_cast<int>(OrderStatus::PartiallyFilled) == 1);
    assert(static_cast<int>(OrderStatus::Filled) == 2);
    assert(static_cast<int>(OrderStatus::Cancelled) == 3);
    assert(static_cast<int>(OrderStatus::Rejected) == 4);
    assert(static_cast<int>(OrderStatus::StopPending) == 5);
    assert(static_cast<int>(OrderStatus::Triggered) == 6);
    
    std::cout << "PASSED\n";
}

void test_order_fields() {
    std::cout << "Test: Order Fields... ";
    
    Order order(1, 2, 1000, 50000, 100, Side::Buy, OrderType::Iceberg);
    order.stop_price = 51000;
    order.visible_quantity = 10;
    order.linked_order_id = 999;
    
    assert(order.order_id == 1);
    assert(order.client_id == 2);
    assert(order.stop_price == 51000);
    assert(order.visible_quantity == 10);
    assert(order.linked_order_id == 999);
    assert(order.hidden_quantity() == 90);
    assert(order.remaining_visible() == 10);
    
    std::cout << "PASSED\n";
}

void test_pending_stops() {
    std::cout << "Test: Pending Stops... ";
    
    OrderBook ob;
    
    Order stop1(1, 1, 1000, 100000, 100, Side::Buy, OrderType::StopLoss);
    stop1.stop_price = 100050;
    ob.add_pending_stop(stop1);
    
    Order stop2(2, 1, 1000, 100000, 100, Side::Sell, OrderType::StopLoss);
    stop2.stop_price = 99950;
    ob.add_pending_stop(stop2);
    
    assert(ob.pending_stops_count() == 2);
    
    auto triggered = ob.check_and_trigger_all_stops(100050);
    assert(triggered.size() == 1);
    assert(triggered[0].order_id == 1);
    assert(ob.pending_stops_count() == 1);
    
    triggered = ob.check_and_trigger_all_stops(99950);
    assert(triggered.size() == 1);
    assert(triggered[0].order_id == 2);
    assert(ob.pending_stops_count() == 0);
    
    std::cout << "PASSED\n";
}

void test_vwap_strategy() {
    std::cout << "Test: VWAPStrategy... ";
    
    VWAPStrategy strategy(1000, 10);
    assert(strategy.remaining_slices() == 10);
    assert(strategy.position() == 0);
    assert(strategy.vwap_price() == 0);
    
    MarketTick tick{100, 100000, 100100, 1000, 1000};
    
    auto orders = strategy.on_tick(tick, 1, 1);
    assert(orders.size() <= 1);
    
    strategy.reset();
    assert(strategy.remaining_slices() == 10);
    
    std::cout << "PASSED\n";
}

void test_vwap_market_maker() {
    std::cout << "Test: VWAPMarketMaker... ";
    
    VWAPMarketMaker mm(100, 500);
    assert(mm.current_vwap() == 0);
    
    MarketTick tick{100, 100000, 100100, 1000, 1000};
    auto orders = mm.on_tick(tick, 1, 1);
    assert(orders.size() == 2);
    assert(orders[0].side == Side::Buy);
    assert(orders[1].side == Side::Sell);
    
    int64_t mid = (tick.bid_price + tick.ask_price) / 2;
    assert(orders[0].price == mid - 50);
    assert(orders[1].price == mid + 50);
    
    mm.reset();
    assert(mm.current_vwap() == 0);
    
    std::cout << "PASSED\n";
}

int main() {
    std::cout << "Running HFT Unit Tests\n";
    std::cout << "======================\n\n";
    
    test_order_book_basic();
    test_order_book_match();
    test_matching_engine_simple();
    test_matching_engine_partial_fill();
    test_matching_engine_multiple_matches();
    test_market_order();
    test_lock_free_queue_basic();
    test_spsc_queue();
    test_thread_safe_engine();
    test_multi_producer();
    test_thread_pool();
    test_thread_pool_batched();
    test_order_pool();
    test_trade_pool();
    test_object_pool_performance();
    test_cancel_order();
    test_amend_order();
    test_matching_engine_cancel();
    test_risk_manager_basic();
    test_risk_manager_order_size();
    test_risk_manager_position();
    test_risk_checked_engine();
    test_order_book_empty();
    test_order_book_single_order();
    test_order_book_multiple_levels();
    test_order_book_remove_best();
    test_order_book_clear();
    test_matching_engine_no_match();
    test_matching_engine_exact_match();
    test_matching_engine_multi_level_match();
    test_matching_engine_trade_ids();
    test_spsc_queue_stress();
    test_lock_free_queue_stress();
    test_thread_pool_stress();
    test_order_pool_stress();
    test_order_book_price_levels();
    test_order_book_ask_levels();
    test_matching_engine_order_idempotency();
    test_matching_engine_fill_price();
    test_matching_engine_partial_fill_full();
    test_risk_manager_reset();
    test_risk_manager_daily_loss();
    test_matching_engine_reset();
    test_order_book_modify();
    test_thread_safe_engine_drain();
    test_generic_object_pool();
    test_aligned_order_pool();
    test_matching_engine_callback();
    test_order_book_get_levels();
    test_matching_engine_market_fills();
    test_spsc_queue_wrap();
    test_batched_thread_pool();
    test_stop_loss_order();
    test_stop_loss_not_triggered();
    test_stop_limit_order();
    test_iceberg_order();
    test_iceberg_with_match();
    test_oco_link();
    test_oco_cancel();
    test_oco_fill();
    test_order_type_enum();
    test_order_status_enum();
    test_order_fields();
    test_pending_stops();
    test_vwap_strategy();
    test_vwap_market_maker();
    
    std::cout << "\nAll tests passed!\n";
    return 0;
}