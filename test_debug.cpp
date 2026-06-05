#include <iostream>
#include "hft/order.hpp"
#include "hft/matching_engine.hpp"

using namespace hft;

int main() {
    MatchingEngine engine;
    
    Order buy(1, 1, 1000, 100500, 200, Side::Buy);
    Order sell(2, 1, 1001, 100400, 100, Side::Sell);
    
    std::cout << "Processing buy order..." << std::endl;
    auto trades1 = engine.process_order(buy);
    std::cout << "Trades: " << trades1.size() << std::endl;
    std::cout << "Order book size: " << engine.order_book().size() << std::endl;
    if (engine.order_book().has_bids()) {
        auto* bid = engine.order_book().get_best_bid();
        std::cout << "Best bid: " << bid->price << " qty: " << bid->quantity 
                  << " filled: " << bid->filled_quantity 
                  << " remaining: " << bid->remaining_quantity() << std::endl;
    }
    
    std::cout << "Processing sell order..." << std::endl;
    auto trades2 = engine.process_order(sell);
    std::cout << "Trades: " << trades2.size() << std::endl;
    for (auto& t : trades2) {
        std::cout << "  Trade: price=" << t.price << " qty=" << t.quantity << std::endl;
    }
    std::cout << "Order book size: " << engine.order_book().size() << std::endl;
    if (engine.order_book().has_bids()) {
        auto* bid = engine.order_book().get_best_bid();
        std::cout << "Best bid: " << bid->price << " qty: " << bid->quantity 
                  << " filled: " << bid->filled_quantity 
                  << " remaining: " << bid->remaining_quantity() << std::endl;
    } else {
        std::cout << "No bids in book!" << std::endl;
    }
    
    return 0;
}
