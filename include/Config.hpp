#pragma once
#include <cstdint>

struct Config {
    // Engine bounds - optimized for performance
    static constexpr uint32_t MAX_TICKS  = 32768;    // Reduced from 65536 for better cache locality
    static constexpr uint32_t MAX_ORDERS = 500'000;  // Reduced from 1M for better memory usage

    // Ring buffer capacity - MUST be large enough to handle order generation rate
    static constexpr size_t   RING_CAPACITY = 1 << 24; // 16,777,216 slots (16M - much larger for 10M orders)

    // Benchmark knobs - optimized for 10M target
    uint64_t num_orders = 10'000'000;  // 10M orders for better measurement
    uint32_t span_ticks = 50;          // Reduced from 100 for tighter price clustering
    uint32_t max_qty = 25;             // Reduced from 50 for faster processing
    uint64_t cancel_every = 100;       // Increased from 50 for less overhead
    unsigned rng_seed = 42;
};
