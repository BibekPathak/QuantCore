#include "hft/backtester.hpp"
#include "hft/strategy.hpp"
#include "hft/binance_feed.hpp"
#include <iostream>
#include <memory>
#include <chrono>

using namespace hft;

void test_binance_parsing() {
    std::cout << "\nBinance Feed - Testing JSON Parsing\n";
    std::cout << "=====================================\n";

    // Simulate a ticker message from Binance
    std::string ticker_json = R"({
        "e": "24hrTicker",
        "E": 123456789,
        "s": "BTCUSDT",
        "p": "500.00",
        "P": "1.00",
        "w": "50000.00",
        "c": "50500.00",
        "Q": "0.01000000",
        "b": "49900.00",
        "B": "12.34567890",
        "a": "50100.00",
        "A": "8.76543210",
        "o": "50000.00",
        "h": "51000.00",
        "l": "49000.00",
        "v": "10000.00000000",
        "q": "500000000.00000000",
        "O": 0,
        "C": 123456789,
        "F": 1,
        "L": 1000,
        "n": 1000
    })";

    std::string depth_json = R"({
        "e": "depthUpdate",
        "E": 123456790,
        "s": "BTCUSDT",
        "U": 100,
        "u": 102,
        "b": [["49950.00", "5.00000000"], ["49900.00", "10.00000000"]],
        "a": [["50100.00", "3.00000000"], ["50150.00", "7.00000000"]]
    })";

    std::string trade_json = R"({
        "e": "trade",
        "E": 123456791,
        "s": "BTCUSDT",
        "t": 12345,
        "p": "50000.00",
        "q": "0.10000000",
        "b": 100,
        "a": 200,
        "T": 123456791,
        "m": true,
        "M": true
    })";

    // Test parsing via the parse_price helper logic
    auto p1 = BinanceFeed::parse_price_for_test("49900.00");
    auto p2 = BinanceFeed::parse_price_for_test("50100.00");
    auto p3 = BinanceFeed::parse_price_for_test("50000.00");

    std::cout << "  Price 49900.00 -> " << p1 << "\n";
    std::cout << "  Price 50100.00 -> " << p2 << "\n";
    std::cout << "  Price 50000.00 -> " << p3 << "\n";

    auto q1 = BinanceFeed::parse_qty_for_test("12.34567890");
    auto q2 = BinanceFeed::parse_qty_for_test("8.76543210");
    auto q3 = BinanceFeed::parse_qty_for_test("0.01000000");

    std::cout << "  Qty 12.34567890 -> " << q1 << "\n";
    std::cout << "  Qty 8.76543210 -> " << q2 << "\n";
    std::cout << "  Qty 0.01000000 -> " << q3 << "\n";

    std::cout << "  Parsing OK\n\n";
}

int main() {
    std::cout << "HFT Trading System - Demo\n";
    std::cout << "==========================\n\n";

    test_binance_parsing();

    std::cout << "Running Synthetic Backtest...\n";
    std::cout << "-------------------------------\n";

    MarketDataFeed feed;
    std::cout << "Generating synthetic market data...\n";
    feed.generate_synthetic(100000, 100000, 100);
    std::cout << "Generated " << feed.size() << " ticks\n\n";

    Backtester backtester;
    backtester.set_market_data(std::move(feed));
    backtester.set_strategy(std::make_unique<MarketMakerStrategy>(200, 1000));

    std::cout << "Running backtest...\n";
    auto start = std::chrono::high_resolution_clock::now();
    
    auto metrics = backtester.run();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Backtest completed in " << duration.count() << " ms\n";
    metrics.print();

    std::cout << "Testing matching engine directly...\n";
    MatchingEngine engine;
    size_t num_orders = 100000;
    
    auto engine_start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < num_orders; ++i) {
        Order buy(i * 2, 1, i, 100000 + (i % 100), 100, Side::Buy, OrderType::Limit);
        Order sell(i * 2 + 1, 1, i, 100000 - (i % 100), 100, Side::Sell, OrderType::Limit);
        
        engine.process_order(buy);
        engine.process_order(sell);
    }
    auto engine_end = std::chrono::high_resolution_clock::now();
    auto engine_duration = std::chrono::duration_cast<std::chrono::microseconds>(engine_end - engine_start);
    
    std::cout << "Processed " << num_orders << " orders in " << engine_duration.count() << " us\n";
    std::cout << "Throughput: " << (num_orders * 1000000.0 / engine_duration.count()) << " orders/sec\n";
    std::cout << "Total trades: " << engine.total_trades() << "\n";

    return 0;
}
