#pragma once

#include "hft/order.hpp"
#include <vector>
#include <memory>
#include <atomic>
#include <cstdlib>

namespace hft {

class OrderPool {
public:
    explicit OrderPool(size_t preallocate = 65536) {
        pool_.reserve(preallocate);
        for (size_t i = 0; i < preallocate; ++i) {
            pool_.push_back(new Order());
            free_list_.push_back(pool_.back());
        }
    }

    ~OrderPool() {
        for (auto* p : pool_) {
            delete p;
        }
    }

    Order* acquire() {
        auto* order = free_list_.back();
        free_list_.pop_back();
        return order;
    }

    void release(Order* order) {
        if (order) {
            order->filled_quantity = 0;
            order->status = OrderStatus::New;
            free_list_.push_back(order);
        }
    }

    template<typename... Args>
    Order* create(Args&&... args) {
        Order* order = acquire();
        new (order) Order(std::forward<Args>(args)...);
        return order;
    }

    void destroy(Order* order) {
        if (order) {
            order->~Order();
            release(order);
        }
    }

    size_t available() const { return free_list_.size(); }
    size_t total() const { return pool_.size(); }

private:
    std::vector<Order*> pool_;
    std::vector<Order*> free_list_;
};

class TradePool {
public:
    explicit TradePool(size_t preallocate = 65536) {
        pool_.reserve(preallocate);
        for (size_t i = 0; i < preallocate; ++i) {
            pool_.push_back(new Trade());
            free_list_.push_back(pool_.back());
        }
    }

    ~TradePool() {
        for (auto* p : pool_) {
            delete p;
        }
    }

    Trade* acquire() {
        auto* trade = free_list_.back();
        free_list_.pop_back();
        return trade;
    }

    void release(Trade* trade) {
        if (trade) {
            free_list_.push_back(trade);
        }
    }

    template<typename... Args>
    Trade* create(Args&&... args) {
        Trade* trade = acquire();
        new (trade) Trade(std::forward<Args>(args)...);
        return trade;
    }

    void destroy(Trade* trade) {
        if (trade) {
            trade->~Trade();
            release(trade);
        }
    }

    size_t available() const { return free_list_.size(); }
    size_t total() const { return pool_.size(); }

private:
    std::vector<Trade*> pool_;
    std::vector<Trade*> free_list_;
};

template<typename T>
class GenericObjectPool {
public:
    explicit GenericObjectPool(size_t preallocate = 65536) 
        : preallocate_(preallocate) {
        expand(preallocate_);
    }

    ~GenericObjectPool() {
        for (auto* p : objects_) {
            delete p;
        }
    }

    T* acquire() {
        if (free_list_.empty()) {
            expand(preallocate_);
        }
        T* obj = free_list_.back();
        free_list_.pop_back();
        return obj;
    }

    void release(T* obj) {
        if (obj) {
            free_list_.push_back(obj);
        }
    }

    template<typename... Args>
    T* create(Args&&... args) {
        T* obj = acquire();
        new (obj) T(std::forward<Args>(args)...);
        return obj;
    }

    void destroy(T* obj) {
        if (obj) {
            obj->~T();
            release(obj);
        }
    }

    size_t available() const { return free_list_.size(); }
    size_t total() const { return objects_.size(); }

private:
    void expand(size_t count) {
        for (size_t i = 0; i < count; ++i) {
            T* obj = new T();
            objects_.push_back(obj);
            free_list_.push_back(obj);
        }
    }

    size_t preallocate_;
    std::vector<T*> objects_;
    std::vector<T*> free_list_;
};

class AlignedOrderPool {
public:
    static constexpr size_t CACHE_LINE = 64;
    static constexpr size_t ALIGNMENT = 16;
    static constexpr size_t OBJ_SIZE = sizeof(Order);
    static constexpr size_t BLOCK_SIZE = OBJ_SIZE + CACHE_LINE;

    explicit AlignedOrderPool(size_t preallocate = 65536) {
        const size_t bytes = preallocate * BLOCK_SIZE + ALIGNMENT;
        memory_.reset(static_cast<char*>(std::aligned_alloc(ALIGNMENT, bytes)));
        
        for (size_t i = 0; i < preallocate; ++i) {
            char* ptr = memory_.get() + i * BLOCK_SIZE;
            free_list_.push_back(ptr);
        }
        capacity_ = preallocate;
    }

    Order* acquire() {
        if (free_list_.empty()) {
            return nullptr;
        }
        char* ptr = free_list_.back();
        free_list_.pop_back();
        return reinterpret_cast<Order*>(ptr);
    }

    void release(Order* order) {
        if (order) {
            free_list_.push_back(reinterpret_cast<char*>(order));
        }
    }

    template<typename... Args>
    Order* create(Args&&... args) {
        Order* order = acquire();
        if (order) {
            new (order) Order(std::forward<Args>(args)...);
        }
        return order;
    }

    void destroy(Order* order) {
        if (order) {
            order->~Order();
            release(order);
        }
    }

    size_t available() const { return free_list_.size(); }
    size_t capacity() const { return capacity_; }

private:
    std::unique_ptr<char[]> memory_;
    std::vector<char*> free_list_;
    size_t capacity_;
};

}
