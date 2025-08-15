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
    static size_t nextPowerOf2(size_t n);

public:

    // design constructor
    AtomicRingBuffer(size_t size);
    // destructor
    ~AtomicRingBuffer();

    bool push(const T& input);
    bool pop(T& input);
    bool full();
    bool empty();

};

