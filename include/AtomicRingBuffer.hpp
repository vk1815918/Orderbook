#pragma once
#include <atomic>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <cstdlib>

// Cache line size for x86-64
static constexpr size_t CACHE_LINE_SIZE = 64;

template<typename T>
class alignas(CACHE_LINE_SIZE) AtomicRingBuffer {
private:
    // Producer and consumer indices - separate cache lines to prevent false sharing
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail;
    
    const size_t capacity;
    const size_t mask;  // For power-of-2 optimization
    
    // Buffer - aligned to cache line
    alignas(CACHE_LINE_SIZE) T* buffer;

public:
    // explicit AtomicRingBuffer(size_t size) 
    //     : head(0), tail(0), capacity(nextPowerOf2(size)), mask(nextPowerOf2(size) - 1) {
    //     // Ensure size is power of 2 for efficient modulo operations
    //     assert((capacity & (capacity - 1)) == 0);
        
    //     // Allocate aligned memory
    //     buffer = static_cast<T*>(aligned_alloc(CACHE_LINE_SIZE, capacity * sizeof(T)));
    //     assert(buffer != nullptr);
    // }

    explicit AtomicRingBuffer(size_t size) : head(0), tail(0), capacity(nextPowerOf2(size)), mask(nextPowerOf2(size) - 1) {
        buffer = static_cast<T*>(aligned_alloc(CACHE_LINE_SIZE, capacity * sizeof(T)));
        assert(buffer != nullptr);

        assert((capacity & (capacity - 1)) == 0);

        static_assert(std::is_trivially_copyable_v<T>);

    }
    
    ~AtomicRingBuffer() {
        if (buffer) {
            free(buffer);
        }
    }
    
    // not copyable or movable
    AtomicRingBuffer(const AtomicRingBuffer&) = delete;
    AtomicRingBuffer& operator=(const AtomicRingBuffer&) = delete;
    
    bool push(const T& item) noexcept {
        size_t current_tail = tail.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) & mask;

        if (next_tail == head.load(std::memory_order_acquire)) {
            return false;
        }
        
        // we checked in constructor if T is trivially copyable.
        std::memcpy(&buffer[current_tail], &item, sizeof(T));
        
        // now update tail with memory order release
        tail.store(next_tail, std::memory_order_release);
        return true;
    }

    bool pop(T& item) noexcept {
        size_t current_head = head.load(std::memory_order_relaxed);

        if (current_head == tail.load(std::memory_order_acquire)) {
            return false;
        }

        std::memcpy(&item, &buffer[current_head], sizeof(T));

        head.store((current_head + 1) & mask, std::memory_order_release);
        return true;
    }
    
   
    size_t pushBatch(const T* items, size_t count) noexcept {
        size_t pushed = 0;

        for (size_t i = 0; i < count; ++i) {
            if (!push(items[i])) {
                break;
            } else {
                pushed++;
            }
        }

        return pushed;
    }

    size_t popBatch(T* items, size_t max_count) noexcept {
        size_t popped = 0;

        for (size_t i = 0; i < max_count; ++i) {
            if (!pop(items[i])) {
                break;
            } else {
                popped++;
            }
        }

        return popped;
    }
   
    bool empty() {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }
    
    bool full() const noexcept {
        size_t next_tail = (tail.load(std::memory_order_relaxed) + 1) & mask;
        return next_tail == head.load(std::memory_order_acquire);
    }
    
    size_t size() const noexcept {
        size_t h = head.load(std::memory_order_acquire);
        size_t t = tail.load(std::memory_order_acquire);
        return (t - h + capacity) & mask;
    }
    
    size_t available() const noexcept {
        return capacity - size() - 1;
    }
    
    void clear() noexcept {
        head.store(0, std::memory_order_relaxed);
        tail.store(0, std::memory_order_relaxed);
    }

private:
    // Helper: get next power of 2
    static size_t nextPowerOf2(size_t n) {
        if (n <= 1) return 2;
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        return n + 1;
    }
};

