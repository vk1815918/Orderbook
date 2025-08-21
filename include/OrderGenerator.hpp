#pragma once
#include <random>
#include <thread>
#include <atomic>
#include "Config.hpp"
#include "OrderMsg.hpp"
#include "AtomicRingBuffer.hpp"
#include "OrderManager.hpp"
#include "Stats.hpp"

class OrderGenerator {
public:
    OrderGenerator(AtomicRingBuffer<OrderMsg>& ring,
                   OrderManager& om,
                   const Config& cfg,
                   std::atomic<bool>& done_flag,
                   Stats& stats);

    void operator()(); // thread entry

private:
    AtomicRingBuffer<OrderMsg>& ring_;
    OrderManager& om_;
    const Config cfg_;
    std::atomic<bool>& done_;
    Stats& stats_;
};
