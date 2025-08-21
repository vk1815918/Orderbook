#pragma once
#include <random>
#include <thread>
#include <atomic>
#include "Config.hpp"
#include "OrderMsg.hpp"
#include "AtomicRingBuffer.hpp"
#include "OrderManager.hpp"
#include "Stats.hpp"
#include <vector>

class OrderGenerator
{
public:
    // Accept vector of per-worker ring pointers to route orders directly to each worker (SPSC per ring)
    OrderGenerator(std::vector<AtomicRingBuffer<OrderMsg> *> &rings,
                   OrderManager &om,
                   const Config &cfg,
                   std::atomic<bool> &done_flag,
                   Stats &stats);

    void operator()(); // thread entry

private:
    std::vector<AtomicRingBuffer<OrderMsg> *> &rings_;
    OrderManager &om_;
    const Config cfg_;
    std::atomic<bool> &done_;
    Stats &stats_;
    uint32_t cur_worker_{0};
};
