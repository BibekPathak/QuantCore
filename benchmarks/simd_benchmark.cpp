#include "hft/simd.hpp"
#include "hft/benchmark.hpp"
#include <iostream>
#include <vector>
#include <cstdlib>

using namespace hft;

int main() {
    std::cout << "SIMD Benchmarks\n";
    std::cout << "===============\n\n";

    std::cout << "AVX2 available: " << (SimdProcessor::has_avx2() ? "yes" : "no") << "\n\n";

    constexpr size_t COUNT = 4096;

    std::vector<int32_t> prices(COUNT);
    std::vector<int32_t> quantities(COUNT);
    std::vector<int32_t> max_prices(COUNT);
    std::vector<int32_t> max_quantities(COUNT);
    std::vector<bool> results(COUNT);
    std::vector<int64_t> values(COUNT);

    for (size_t i = 0; i < COUNT; i++) {
        prices[i] = static_cast<int32_t>(100000 + i);
        quantities[i] = static_cast<int32_t>(100 + (i % 1000));
        max_prices[i] = 200000;
        max_quantities[i] = 10000;
    }

    std::cout << "=== Batch Computation ===\n";

    auto value_result = Benchmark::run("Batch compute value", [&]() {
        SimdProcessor::batch_compute_value(prices.data(), quantities.data(), values.data(), COUNT);
    }, 1000);
    value_result.print("Batch compute value");

    auto sum_result = Benchmark::run("Batch sum", [&]() {
        int64_t sum = 0;
        SimdProcessor::batch_sum(prices.data(), &sum, COUNT);
    }, 1000);
    sum_result.print("Batch sum");

    int32_t min_v = 0, max_v = 0;
    auto minmax_result = Benchmark::run("Batch min/max", [&]() {
        SimdProcessor::batch_find_min_max(prices.data(), &min_v, &max_v, COUNT);
    }, 1000);
    minmax_result.print("Batch min/max");

    std::cout << "\nMin: " << min_v << ", Max: " << max_v << "\n";

    std::cout << "\nSIMD Benchmarks complete!\n";
    return 0;
}
