#pragma once

#include "hft/order.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>

namespace hft {

class Arena {
public:
    Arena() = default;

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    Arena(Arena&& other) noexcept
        : buffer_(other.buffer_)
        , capacity_(other.capacity_)
        , bump_offset_(other.bump_offset_)
        , free_head_(other.free_head_)
    {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
        other.bump_offset_ = 0;
        other.free_head_ = nullptr;
    }

    Arena& operator=(Arena&& other) noexcept {
        if (this != &other) {
            std::free(buffer_);
            buffer_ = other.buffer_;
            capacity_ = other.capacity_;
            bump_offset_ = other.bump_offset_;
            free_head_ = other.free_head_;
            other.buffer_ = nullptr;
            other.capacity_ = 0;
            other.bump_offset_ = 0;
            other.free_head_ = nullptr;
        }
        return *this;
    }

    ~Arena() {
        std::free(buffer_);
    }

    void init(size_t capacity) {
        buffer_ = static_cast<char*>(std::aligned_alloc(alignof(Order), capacity));
        capacity_ = capacity;
        bump_offset_ = 0;
        free_head_ = nullptr;
    }

    Order* allocate() {
        if (free_head_) {
            Order* o = free_head_;
            free_head_ = static_cast<Order*>(o->next);
            o->next = nullptr;
            o->prev = nullptr;
            return o;
        }
        if (bump_offset_ + sizeof(Order) > capacity_) {
            return nullptr;
        }
        Order* o = reinterpret_cast<Order*>(buffer_ + bump_offset_);
        bump_offset_ += sizeof(Order);
        o->next = nullptr;
        o->prev = nullptr;
        return o;
    }

    void deallocate(Order* o) {
        o->next = static_cast<Order*>(free_head_);
        o->prev = nullptr;
        free_head_ = o;
    }

    void reset() {
        bump_offset_ = 0;
        free_head_ = nullptr;
    }

    size_t capacity() const { return capacity_; }
    size_t used() const { return bump_offset_ / sizeof(Order); }
    size_t free_list_size() const {
        size_t count = 0;
        Order* cur = free_head_;
        while (cur) {
            count++;
            cur = static_cast<Order*>(cur->next);
        }
        return count;
    }

private:
    char* buffer_ = nullptr;
    size_t capacity_ = 0;
    size_t bump_offset_ = 0;
    Order* free_head_ = nullptr;
};

}
