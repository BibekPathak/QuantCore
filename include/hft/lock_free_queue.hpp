#pragma once

#include <atomic>
#include <memory>
#include <vector>

namespace hft {

template<typename T>
struct LFNode {
    T data;
    std::atomic<LFNode<T>*> next{nullptr};
    
    LFNode() = default;
    explicit LFNode(T d) : data(std::move(d)) {}
};

template<typename T>
class LockFreeQueue {
public:
    LockFreeQueue() {
        head_.store(&dummy_, std::memory_order_relaxed);
        tail_ = &dummy_;
    }

    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    void enqueue(T value) {
        LFNode<T>* node = new LFNode<T>(std::move(value));
        LFNode<T>* prev = head_.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_relaxed);
    }

    bool dequeue(T& result) {
        LFNode<T>* tail = tail_;
        LFNode<T>* next = tail->next.load(std::memory_order_acquire);
        
        if (next == nullptr) {
            return false;
        }
        
        result = std::move(next->data);
        tail_ = next;
        return true;
    }

    bool try_dequeue(T& result) {
        return dequeue(result);
    }

    bool is_empty() const {
        LFNode<T>* tail = tail_;
        LFNode<T>* next = tail->next.load(std::memory_order_acquire);
        return next == nullptr;
    }

    void clear() {
        T dummy;
        while (dequeue(dummy)) {}
    }

private:
    alignas(64) LFNode<T> dummy_;
    alignas(64) std::atomic<LFNode<T>*> head_;
    LFNode<T>* tail_;
};

template<typename T>
class ThreadSafeOrderQueue {
public:
    explicit ThreadSafeOrderQueue(size_t capacity) 
        : capacity_(capacity), size_(0) {
        buffer_.resize(capacity);
    }

    bool enqueue(const T& item) {
        if (size_.load(std::memory_order_acquire) >= capacity_) {
            return false;
        }
        size_t idx = head_.fetch_add(1, std::memory_order_relaxed);
        buffer_[idx % capacity_] = item;
        size_.fetch_add(1, std::memory_order_release);
        return true;
    }

    bool dequeue(T& result) {
        if (size_.load(std::memory_order_acquire) == 0) {
            return false;
        }
        size_t idx = tail_.fetch_add(1, std::memory_order_relaxed);
        result = buffer_[idx % capacity_];
        size_.fetch_sub(1, std::memory_order_release);
        return true;
    }

    size_t size() const {
        return size_.load(std::memory_order_acquire);
    }

    bool is_empty() const {
        return size_.load(std::memory_order_acquire) == 0;
    }

    bool is_full() const {
        return size_.load(std::memory_order_acquire) >= capacity_;
    }

    void clear() {
        T dummy;
        while (dequeue(dummy)) {}
    }

private:
    std::vector<T> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
    std::atomic<size_t> size_{0};
    size_t capacity_;
};

template<typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(size_t capacity) 
        : capacity_(capacity), buffer_(capacity) {}

    bool enqueue(T value) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t tail = tail_.load(std::memory_order_acquire);
        if (head - tail >= capacity_) {
            return false;
        }
        buffer_[head % capacity_] = std::move(value);
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    bool dequeue(T& result) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t head = head_.load(std::memory_order_acquire);
        if (tail >= head) {
            return false;
        }
        result = std::move(buffer_[tail % capacity_]);
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    size_t size() const {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        return head > tail ? head - tail : 0;
    }

    bool is_empty() const {
        return size() == 0;
    }

private:
    const size_t capacity_;
    std::vector<T> buffer_;
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
};

}
