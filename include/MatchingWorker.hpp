#pragma once
#include "AtomicRingBuffer.hpp"
#include "OrderManager.hpp"
#include "Stats.hpp"
#include "OrderMsg.hpp"
#include "MatchingEngine.hpp"
#include "Config.hpp"
#include <atomic>

class MatchingWorker {
public:
    MatchingWorker(AtomicRingBuffer<OrderMsg>& ring, 
                   OrderManager& orderManager, 
                   Stats& stats,
                   std::atomic<bool>& done_flag);
    
    void operator()(); // thread entry point
    
    // Access to engine for stats
    const MatchingEngine<Config::MAX_TICKS, Config::MAX_ORDERS>& engine() const { return engine_; }

private:
    AtomicRingBuffer<OrderMsg>& ring_;
    OrderManager& orderManager_;
    Stats& stats_;
    MatchingEngine<Config::MAX_TICKS, Config::MAX_ORDERS> engine_;
    std::atomic<bool>& done_;
    
    // Batch processing for better throughput
    static constexpr size_t BATCH_SIZE = 10000; // Increased batch size
};
