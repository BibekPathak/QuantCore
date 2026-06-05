#include <iostream>
#include "hft/order.hpp"
#include "hft/order_book.hpp"

using namespace hft;

int main() {
    OrderBook ob;
    
    Order buy(1, 1, 1000, 100500, 200, Side::Buy);
    Order sell(2, 1, 1001, 100400, 100, Side::Sell);
    
    ob.add_order(buy);
    std::cout << "After adding buy:" << std::endl;
    std::cout << "  bids: " << ob.bids.size() << ", asks: " << ob.asks.size() << std::endl;
    std::cout << "  best_bid: " << ob.best_bid() << std::endl;
    
    // Let's manually trace through matching
    std::cout << "\nTrying to match sell..." << std::endl;
    std::cout << "  sell.remaining_quantity(): " << sell.remaining_quantity() << std::endl;
    std::cout << "  ob.has_bids(): " << ob.has_bids() << std::endl;
    std::cout << "  sell.price <= ob.best_bid(): " << (sell.price <= ob.best_bid()) << std::endl;
    
    // Test remaining_quantity on const order
    Order const_order = buy;
    std::cout << "\nconst_order.remaining_quantity(): " << const_order.remaining_quantity() << std::endl;
    
    // Test mutable copy
    Order mutable_copy = buy;
    mutable_copy.filled_quantity += 100;
    std::cout << "After filling 100, mutable_copy.remaining_quantity(): " << mutable_copy.remaining_quantity() << std::endl;
    
    return 0;
}
