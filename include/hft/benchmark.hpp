#pragma once

#include <chrono>
#include <vector>
#include <atomic>
#include <functional>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>

namespace hft {

struct BenchmarkResult {
    double total_ns;
    double avg_ns;
    double min_ns;
    double max_ns;
    double p50_ns;
    double p90_ns;
    double p99_ns;
    double p999_ns;
    uint64_t iterations;
    double throughput;

    void print(const char* name) const {
        std::cout << std::setw(40) << std::left << name << " ";
        std::cout << std::right << std::setw(12) << std::fixed << std::setprecision(1);
        std::cout << (iterations / (total_ns / 1e9)) << " ops/sec | ";
        std::cout << "avg: " << std::setw(8) << avg_ns << " ns | ";
        std::cout << "p50: " << std::setw(8) << p50_ns << " ns | ";
        std::cout << "p99: " << std::setw(8) << p99_ns << " ns";
        std::cout << "\n";
    }
};

class Benchmark {
public:
    template<typename Func>
    static BenchmarkResult run(const char* name, Func func, uint64_t iterations, bool warmup = true) {
        if (warmup) {
            for (uint64_t i = 0; i < 1000; i++) {
                func();
            }
        }

        std::vector<double> latencies;
        latencies.reserve(iterations);

        auto start = std::chrono::high_resolution_clock::now();
        for (uint64_t i = 0; i < iterations; i++) {
            auto t_start = std::chrono::high_resolution_clock::now();
            func();
            auto t_end = std::chrono::high_resolution_clock::now();
            latencies.push_back(std::chrono::duration<double, std::nano>(t_end - t_start).count());
        }
        auto end = std::chrono::high_resolution_clock::now();

        double total_ns = std::chrono::duration<double, std::nano>(end - start).count();

        std::sort(latencies.begin(), latencies.end());

        BenchmarkResult result;
        result.total_ns = total_ns;
        result.avg_ns = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
        result.min_ns = latencies.front();
        result.max_ns = latencies.back();
        result.p50_ns = latencies[iterations * 0.5];
        result.p90_ns = latencies[iterations * 0.9];
        result.p99_ns = latencies[iterations * 0.99];
        result.p999_ns = latencies[iterations * 0.999];
        result.iterations = iterations;
        result.throughput = iterations / (total_ns / 1e9);

        return result;
    }

    template<typename Func>
    static BenchmarkResult run_no_overhead(const char* name, Func func, uint64_t iterations) {
        std::vector<double> latencies;
        latencies.reserve(iterations);

        for (uint64_t i = 0; i < iterations; i++) {
            auto t_start = std::chrono::high_resolution_clock::now();
            func();
            auto t_end = std::chrono::high_resolution_clock::now();
            latencies.push_back(std::chrono::duration<double, std::nano>(t_end - t_start).count());
        }

        double total_ns = std::accumulate(latencies.begin(), latencies.end(), 0.0);

        std::sort(latencies.begin(), latencies.end());

        BenchmarkResult result;
        result.total_ns = total_ns;
        result.avg_ns = total_ns / iterations;
        result.min_ns = latencies.front();
        result.max_ns = latencies.back();
        result.p50_ns = latencies[iterations * 0.5];
        result.p90_ns = latencies[iterations * 0.9];
        result.p99_ns = latencies[iterations * 0.99];
        result.p999_ns = latencies[iterations * 0.999];
        result.iterations = iterations;
        result.throughput = iterations / (total_ns / 1e9);

        return result;
    }
};

class ThroughputBenchmark {
public:
    template<typename Func>
    static double measure_throughput(const char* name, Func func, uint64_t iterations, uint64_t batch_size = 1000) {
        auto start = std::chrono::high_resolution_clock::now();
        for (uint64_t i = 0; i < iterations; i++) {
            func();
        }
        auto end = std::chrono::high_resolution_clock::now();

        double ns = std::chrono::duration<double, std::nano>(end - start).count();
        double ops_per_sec = iterations / (ns / 1e9);

        std::cout << std::setw(40) << std::left << name << " ";
        std::cout << std::right << std::setw(12) << std::fixed << std::setprecision(0);
        std::cout << ops_per_sec << " ops/sec\n";

        return ops_per_sec;
    }

    template<typename Func>
    static double measure_latency(const char* name, Func func, uint64_t iterations) {
        std::vector<double> latencies;
        latencies.reserve(iterations);

        for (uint64_t i = 0; i < iterations; i++) {
            auto t_start = std::chrono::high_resolution_clock::now();
            func();
            auto t_end = std::chrono::high_resolution_clock::now();
            latencies.push_back(std::chrono::duration<double, std::nano>(t_end - t_start).count());
        }

        std::sort(latencies.begin(), latencies.end());

        double avg = std::accumulate(latencies.begin(), latencies.end(), 0.0) / iterations;

        std::cout << std::setw(40) << std::left << name << " ";
        std::cout << std::right << std::setw(12) << std::fixed << std::setprecision(1);
        std::cout << "avg: " << std::setw(8) << avg << " ns | ";
        std::cout << "p50: " << std::setw(8) << latencies[iterations * 0.5] << " ns | ";
        std::cout << "p99: " << std::setw(8) << latencies[iterations * 0.99] << " ns";
        std::cout << "\n";

        return avg;
    }
};

}
