#pragma once
#include <cstdint>

struct Config
{
    // Engine bounds - optimized for performance
    static constexpr uint32_t MAX_TICKS = 32768;    // Reduced from 65536 for better cache locality
    static constexpr uint32_t MAX_ORDERS = 500'000; // Reduced from 1M for better memory usage

    // Ring buffer capacity - MUST be large enough to handle order generation rate
    // Increase ring capacity so generator can push 30M orders without heavy backpressure
    static constexpr size_t RING_CAPACITY = 1 << 25; // 33,554,432 slots (>= 30M)

    // Benchmark knobs - optimized for 30M target
    uint64_t num_orders = 40'000'000; // 30M orders for measurement
    uint32_t span_ticks = 50;         // Reduced from 100 for tighter price clustering
    uint32_t max_qty = 10;            // Reduced from 50 for faster processing
    uint64_t cancel_every = 100000;      // Cancel every 500th order (more reasonable for 30M)
    unsigned rng_seed = 12;

    // Advanced stats toggles for HFT demos
    bool show_latency_percentiles = false; // P50, P95, P99 latency breakdown
    bool show_memory_stats = false;        // Memory allocation and usage stats
    bool show_cache_stats = false;         // Cache performance metrics
    bool show_thread_stats = false;        // Per-thread performance breakdown
    bool show_all_advanced = true;        // Enable all advanced stats
};
