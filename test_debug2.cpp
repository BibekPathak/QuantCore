#include <iostream>
#include "hft/order.hpp"
#include "hft/matching_engine.hpp"

using namespace hft;

int main() {
    MatchingEngine engine;
    
    Order buy(1, 1, 1000, 100500, 200, Side::Buy);
    Order sell(2, 1, 1001, 100400, 100, Side::Sell);
    
    engine.process_order(buy);
    std::cout << "After buy - book size: " << engine.order_book().size() << std::endl;
    std::cout << "has_bids: " << engine.order_book().has_bids() << std::endl;
    std::cout << "has_asks: " << engine.order_book().has_asks() << std::endl;
    std::cout << "best_bid: " << engine.order_book().best_bid() << std::endl;
    std::cout << "can_match: " << engine.order_book().can_match() << std::endl;
    
    auto trades = engine.process_order(sell);
    std::cout << "After sell - trades: " << trades.size() << std::endl;
    std::cout << "book size: " << engine.order_book().size() << std::endl;
    std::cout << "has_bids: " << engine.order_book().has_bids() << std::endl;
    std::cout << "has_asks: " << engine.order_book().has_asks() << std::endl;
    if (engine.order_book().has_bids()) {
        std::cout << "best_bid: " << engine.order_book().best_bid() << std::endl;
    }
    if (engine.order_book().has_asks()) {
        std::cout << "best_ask: " << engine.order_book().best_ask() << std::endl;
    }
    
    return 0;
}
