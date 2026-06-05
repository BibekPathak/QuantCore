#include "hft/order_book.hpp"
#include "hft/matching_engine.hpp"
#include <cassert>
#include <iostream>

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

int main() {
    std::cout << "Running HFT Unit Tests\n";
    std::cout << "======================\n\n";
    
    test_order_book_basic();
    test_order_book_match();
    test_matching_engine_simple();
    test_matching_engine_partial_fill();
    test_matching_engine_multiple_matches();
    test_market_order();
    
    std::cout << "\nAll tests passed!\n";
    return 0;
}
