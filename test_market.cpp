#include <iostream>
#include "hft/order.hpp"
#include "hft/matching_engine.hpp"

using namespace hft;

int main() {
    MatchingEngine engine;
    
    Order sell1(1, 1, 1000, 100400, 100, Side::Sell);
    Order sell2(2, 1, 1001, 100500, 100, Side::Sell);
    
    engine.process_order(sell1);
    std::cout << "After sell1 - asks: " << engine.order_book().asks.size() << ", best_ask: " << engine.order_book().best_ask() << std::endl;
    
    engine.process_order(sell2);
    std::cout << "After sell2 - asks: " << engine.order_book().asks.size() << ", best_ask: " << engine.order_book().best_ask() << std::endl;
    
    Order market_buy(3, 1, 1002, 0, 150, Side::Buy, OrderType::Market);
    auto trades = engine.process_order(market_buy);
    
    std::cout << "Trades: " << trades.size() << std::endl;
    for (size_t i = 0; i < trades.size(); i++) {
        std::cout << "  Trade " << i << ": price=" << trades[i].price << ", qty=" << trades[i].quantity << std::endl;
    }
    
    return 0;
}
