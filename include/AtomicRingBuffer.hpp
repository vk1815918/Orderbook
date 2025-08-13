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

public:

    AtomicRingBuffer(size_t size) : head(0), tail(0), capacity(size) {
        buffer = new T[size];
    }

    bool push(const T& input) {
        if (full()) return false;

        size_t current_tail = tail.load();
        buffer[current_tail] = input;
        tail.store((current_tail + 1) % capacity);
        return true;
    } 

    bool pop(T& input) {
        if (empty()) return false;

        size_t current_head = head.load();
        input = buffer[current_head];
        head.store((current_head + 1) % capacity);
        
        return true;
    }

    bool full() {
        size_t next_tail = (tail.load() + 1) % capacity;
        return (next_tail == head.load());
    }

    bool empty() {
        if (head.load() == tail.load()) return true;
        return false;
    }




};

