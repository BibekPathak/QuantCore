#include "hft/backtester.hpp"
#include "hft/strategy.hpp"
#include <iostream>
#include <memory>
#include <chrono>

using namespace hft;

int main() {
    std::cout << "HFT Trading System - Starting Backtest\n";
    std::cout << "=========================================\n\n";

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
    
    engine.set_trade_callback([](const Trade& t) {});
    
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
