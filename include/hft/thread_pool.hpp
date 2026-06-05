#pragma once

#include <atomic>
#include <vector>
#include <queue>
#include <thread>
#include <functional>
#include <memory>
#include <optional>
#include <future>
#include <condition_variable>

namespace hft {

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads)
        : running_(true) {
        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    ~ThreadPool() {
        running_.store(false, std::memory_order_release);
        cv_.notify_all();
        for (auto& w : workers_) {
            if (w.joinable()) {
                w.join();
            }
        }
    }

    template<typename F>
    auto enqueue(F&& task) -> std::future<std::invoke_result_t<F>> {
        using ReturnType = std::invoke_result_t<F>;
        auto packaged_task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(task));
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            tasks_.emplace([packaged_task]() { (*packaged_task)(); });
        }
        cv_.notify_one();
        return packaged_task->get_future();
    }

    size_t num_threads() const { return workers_.size(); }
    size_t queue_size() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

    void wait_all() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        cv_.wait(lock, [this] { return tasks_.empty() && active_tasks_ == 0; });
    }

private:
    void worker_loop() {
        while (running_.load(std::memory_order_acquire)) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                cv_.wait(lock, [this] { 
                    return !running_.load(std::memory_order_acquire) || !tasks_.empty(); 
                });
                if (!running_.load(std::memory_order_acquire)) {
                    break;
                }
                if (tasks_.empty()) {
                    continue;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
                active_tasks_++;
            }
            if (task) {
                task();
            }
            active_tasks_--;
            cv_.notify_all();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_;
    std::atomic<size_t> active_tasks_{0};
};

class BatchedThreadPool {
public:
    explicit BatchedThreadPool(size_t num_threads, size_t batch_size = 1024)
        : pool_(num_threads)
        , batch_size_(batch_size)
    {}

    template<typename F, typename T>
    std::vector<std::future<std::vector<std::invoke_result_t<F, T>>>> map(F&& func, std::vector<T> items) {
        std::vector<std::future<std::vector<std::invoke_result_t<F, T>>>> futures;
        
        size_t num_batches = (items.size() + batch_size_ - 1) / batch_size_;
        futures.reserve(num_batches);
        
        for (size_t i = 0; i < num_batches; ++i) {
            size_t start = i * batch_size_;
            size_t end = std::min(start + batch_size_, items.size());
            
            auto batch = std::vector<T>(items.begin() + start, items.begin() + end);
            futures.push_back(pool_.enqueue([func = std::forward<F>(func), batch = std::move(batch)]() mutable {
                std::vector<std::invoke_result_t<F, T>> results;
                results.reserve(batch.size());
                for (auto& item : batch) {
                    results.push_back(func(item));
                }
                return results;
            }));
        }
        
        return futures;
    }

    template<typename F, typename T>
    std::vector<std::future<std::invoke_result_t<F, T>>> map_async(F&& func, std::vector<T> items) {
        std::vector<std::future<std::invoke_result_t<F, T>>> futures;
        futures.reserve(items.size());
        
        for (auto& item : items) {
            futures.push_back(pool_.enqueue([&func, &item]() {
                return func(item);
            }));
        }
        
        return futures;
    }

    size_t queue_size() const { return pool_.queue_size(); }
    void wait_all() { pool_.wait_all(); }

private:
    ThreadPool pool_;
    size_t batch_size_;
};

}
