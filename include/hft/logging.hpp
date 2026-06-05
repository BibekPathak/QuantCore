#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <iostream>

namespace hft {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void set_level(LogLevel level) {
        level_.store(static_cast<int>(level), std::memory_order_release);
    }

    void log(LogLevel level, const char* message) {
        if (static_cast<int>(level) < level_.load(std::memory_order_acquire)) {
            return;
        }
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        oss << "[" << std::put_time(std::localtime(&time), "%H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "]";
        oss << "[" << level_string(level) << "] ";
        oss << message << "\n";
        
        if (level >= LogLevel::ERROR) {
            std::cerr << oss.str();
        } else {
            std::cout << oss.str();
        }
    }

    void debug(const char* message) { log(LogLevel::DEBUG, message); }
    void info(const char* message) { log(LogLevel::INFO, message); }
    void warn(const char* message) { log(LogLevel::WARN, message); }
    void error(const char* message) { log(LogLevel::ERROR, message); }

    template<typename... Args>
    void log_format(LogLevel level, const char* format, Args... args) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format, args...);
        log(level, buffer);
    }

private:
    Logger() : level_(static_cast<int>(LogLevel::INFO)) {}

    const char* level_string(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    std::atomic<int> level_;
    std::mutex mutex_;
};

#define LOG_DEBUG(msg) Logger::instance().debug(msg)
#define LOG_INFO(msg) Logger::instance().info(msg)
#define LOG_WARN(msg) Logger::instance().warn(msg)
#define LOG_ERROR(msg) Logger::instance().error(msg)

struct PrometheusMetric {
    std::string name;
    std::string help;
    std::string type;
    double value;
};

class MetricsExporter {
public:
    static std::string to_prometheus_format(const std::vector<PrometheusMetric>& metrics) {
        std::ostringstream oss;
        for (const auto& m : metrics) {
            oss << "# HELP " << m.name << " " << m.help << "\n";
            oss << "# TYPE " << m.name << " " << m.type << "\n";
            oss << m.name << " " << std::fixed << std::setprecision(6) << m.value << "\n";
        }
        return oss.str();
    }

    static std::string to_json_format(const std::vector<PrometheusMetric>& metrics) {
        std::ostringstream oss;
        oss << "{";
        oss << "\"metrics\":[";
        for (size_t i = 0; i < metrics.size(); i++) {
            const auto& m = metrics[i];
            if (i > 0) oss << ",";
            oss << "{";
            oss << "\"name\":\"" << m.name << "\",";
            oss << "\"value\":" << std::fixed << std::setprecision(6) << m.value;
            oss << "}";
        }
        oss << "]}";
        return oss.str();
    }
};

class PerformanceMetrics {
public:
    void record_order() {
        orders_.fetch_add(1, std::memory_order_relaxed);
    }

    void record_trade() {
        trades_.fetch_add(1, std::memory_order_relaxed);
    }

    void record_latency_ns(uint64_t ns) {
        auto idx = latency_index(ns);
        latency_counts_[idx].fetch_add(1, std::memory_order_relaxed);
        
        uint64_t expected = 0;
        min_latency_.compare_exchange_strong(expected, ns, std::memory_order_relaxed);
        max_latency_.store(ns, std::memory_order_relaxed);
    }

    void record_volume(uint64_t volume) {
        volume_.fetch_add(volume, std::memory_order_relaxed);
    }

    void record_pnl(int64_t pnl) {
        pnl_.fetch_add(pnl, std::memory_order_relaxed);
    }

    std::vector<PrometheusMetric> get_metrics() const {
        std::vector<PrometheusMetric> metrics;
        
        metrics.push_back({"hft_orders_total", "Total orders processed", "counter", 
            static_cast<double>(orders_.load())});
        metrics.push_back({"hft_trades_total", "Total trades executed", "counter", 
            static_cast<double>(trades_.load())});
        metrics.push_back({"hft_volume_total", "Total trading volume", "counter", 
            static_cast<double>(volume_.load())});
        metrics.push_back({"hft_pnl_total", "Total profit/loss", "gauge", 
            static_cast<double>(pnl_.load())});
        metrics.push_back({"hft_min_latency_ns", "Minimum latency in ns", "gauge", 
            static_cast<double>(min_latency_.load())});
        metrics.push_back({"hft_max_latency_ns", "Maximum latency in ns", "gauge", 
            static_cast<double>(max_latency_.load())});
        
        return metrics;
    }

    void reset() {
        orders_.store(0);
        trades_.store(0);
        volume_.store(0);
        pnl_.store(0);
        min_latency_.store(UINT64_MAX);
        max_latency_.store(0);
        for (auto& c : latency_counts_) {
            c.store(0);
        }
    }

    uint64_t orders() const { return orders_.load(); }
    uint64_t trades() const { return trades_.load(); }
    uint64_t volume() const { return volume_.load(); }
    int64_t pnl() const { return pnl_.load(); }
    uint64_t min_latency() const { return min_latency_.load(); }
    uint64_t max_latency() const { return max_latency_.load(); }

private:
    size_t latency_index(uint64_t ns) {
        if (ns < 100) return 0;
        if (ns < 1000) return 1;
        if (ns < 10000) return 2;
        if (ns < 100000) return 3;
        if (ns < 1000000) return 4;
        return 5;
    }

    std::atomic<uint64_t> orders_{0};
    std::atomic<uint64_t> trades_{0};
    std::atomic<uint64_t> volume_{0};
    std::atomic<int64_t> pnl_{0};
    std::atomic<uint64_t> min_latency_{UINT64_MAX};
    std::atomic<uint64_t> max_latency_{0};
    std::atomic<uint64_t> latency_counts_[6] = {};
};

}
