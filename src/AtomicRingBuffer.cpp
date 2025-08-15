
#pragma once

#include <atomic>
#include <cstddef>


template<typename T>
class AtomicRingBuffer {

private:

    std::atomic<size_t> head;
    std::atomic<size_t> tail;

    const size_t capacity;
    const size_t mask;
    T* buffer;

    // helper
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

public:

    // design constructor
    AtomicRingBuffer(size_t size) : head(0), tail(0), capacity(nextPowerOf2(size)), mask(nextPowerOf2(size) - 1) {
        buffer = new T[nextPowerOf2(size)];
    }
    // destructor
    ~AtomicRingBuffer() {
        delete[] buffer;
    } 

    bool push(const T& input) {
        if (full()) return false;

        size_t current_tail = tail.load();
        buffer[current_tail] = input;
        tail.store((current_tail + 1) & mask);
        return true;
    } 

    bool pop(T& input) {
        if (empty()) return false;

        size_t current_head = head.load();
        input = buffer[current_head];
        head.store((current_head + 1) & mask);
        
        return true;
    }

    bool full() {
        size_t next_tail = (tail.load() + 1) & mask;
        return (next_tail == head.load());
    }

    bool empty() {
        if (head.load() == tail.load()) return true;
        return false;
    }

};

