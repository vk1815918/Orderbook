#pragma once
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <string>
#include <atomic>
#include <vector>
#include <algorithm>
#include <memory>

// Helper function to add commas to large numbers
inline std::string formatNumber(uint64_t num)
{
    if (num < 1000)
        return std::to_string(num);

    std::string str = std::to_string(num);
    int len = str.length();
    for (int i = len - 3; i > 0; i -= 3)
    {
        str.insert(i, ",");
    }
    return str;
}

// Helper to format bytes
inline std::string formatBytes(size_t bytes)
{
    const char *units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double size = bytes;
    while (size >= 1024 && unit < 3)
    {
        size /= 1024;
        unit++;
    }
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f %s", size, units[unit]);
    return std::string(buffer);
}

struct AdvancedStats
{
    // Latency tracking
    std::vector<uint64_t> latencies_ns;
    std::atomic<uint64_t> total_latency_ns{0};

    // Memory stats
    std::atomic<size_t> peak_memory_usage{0};
    std::atomic<size_t> current_memory_usage{0};
    std::atomic<uint64_t> allocations{0};
    std::atomic<uint64_t> deallocations{0};

    // Cache performance (simulated)
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};

    // Per-thread stats
    struct ThreadStats
    {
        std::atomic<uint64_t> processed{0};
        std::atomic<uint64_t> batches{0};
        std::atomic<double> cpu_utilization{0.0};
    };
    std::vector<ThreadStats> thread_stats;

    AdvancedStats(int num_threads = 8) : thread_stats(num_threads) {}

    void addLatency(uint64_t ns)
    {
        latencies_ns.push_back(ns);
        total_latency_ns.fetch_add(ns, std::memory_order_relaxed);
    }

    void updateMemory(size_t current)
    {
        current_memory_usage = current;
        size_t peak = peak_memory_usage.load();
        while (current > peak && !peak_memory_usage.compare_exchange_weak(peak, current))
        {
        }
    }
};

struct Stats
{
    std::atomic<uint64_t> generated{0};
    std::atomic<uint64_t> pushed{0};
    std::atomic<uint64_t> popped{0};
    std::atomic<uint64_t> rejected{0}; // engine rejects (e.g., out-of-range/IOC)
    std::atomic<uint64_t> donefill{0}; // fully filled takers
    std::atomic<uint64_t> resting{0};  // handles currently stored
    std::atomic<uint64_t> cancels{0};

    // timing
    std::chrono::high_resolution_clock::time_point t0, t1;

    // Advanced metrics
    std::unique_ptr<AdvancedStats> advanced;

    Stats() : advanced(std::make_unique<AdvancedStats>()) {}

    void start() { t0 = std::chrono::high_resolution_clock::now(); }
    void stop() { t1 = std::chrono::high_resolution_clock::now(); }

    void print(double extra_orders_per_sec = 0.0, bool show_latency = false, bool show_memory = false,
               bool show_cache = false, bool show_threads = false) const
    {
        const double secs = std::chrono::duration<double>(t1 - t0).count();
        const double gen_per_s = generated.load() / secs;
        const double pop_per_s = popped.load() / secs;

        printf("\n");
        printf("╔══════════════════════════════════════════════════════════════╗\n");
        printf("║                    ORDERBOOK PERFORMANCE REPORT              ║\n");
        printf("╠══════════════════════════════════════════════════════════════╣\n");
        printf("║  📊 ORDER PROCESSING STATISTICS                             ║\n");
        printf("║  ┌────────────────────────────────────────────────────────┐ ║\n");
        printf("║  │ Generated Orders: %15s │ ║\n", formatNumber(generated.load()).c_str());
        printf("║  │ Pushed to Buffer: %15s │ ║\n", formatNumber(pushed.load()).c_str());
        printf("║  │ Processed Orders: %15s │ ║\n", formatNumber(popped.load()).c_str());
        printf("║  │ Rejected Orders:  %15s │ ║\n", formatNumber(rejected.load()).c_str());
        printf("║  │ Immediate Fills:  %15s │ ║\n", formatNumber(donefill.load()).c_str());
        printf("║  │ Cancelled Orders: %15s │ ║\n", formatNumber(cancels.load()).c_str());
        printf("║  └────────────────────────────────────────────────────────┘ ║\n");
        printf("║                                                              ║\n");
        printf("║  ⚡ PERFORMANCE METRICS                                     ║\n");
        printf("║  ┌────────────────────────────────────────────────────────┐ ║\n");
        printf("║  │ Generation Rate: %15.2f orders/sec │ ║\n", gen_per_s);
        printf("║  │ Processing Rate: %15.2f orders/sec │ ║\n", pop_per_s);
        if (extra_orders_per_sec > 0.0)
        {
            printf("║  │ Throughput:     %15.2f orders/sec │ ║\n", extra_orders_per_sec);
        }
        printf("║  │ Total Time:     %15.6f seconds    │ ║\n", secs);
        printf("║  └────────────────────────────────────────────────────────┘ ║\n");

        // Advanced stats sections
        if (show_latency && !advanced->latencies_ns.empty())
        {
            printLatencyStats();
        }

        if (show_memory)
        {
            printMemoryStats();
        }

        if (show_cache)
        {
            printCacheStats();
        }

        if (show_threads)
        {
            printThreadStats();
        }

        printf("╚══════════════════════════════════════════════════════════════╝\n");
        printf("\n");
    }

private:
    void printLatencyStats() const
    {
        auto latencies = advanced->latencies_ns;
        if (latencies.empty())
            return;

        std::sort(latencies.begin(), latencies.end());
        size_t count = latencies.size();

        uint64_t p50 = latencies[count * 50 / 100];
        uint64_t p95 = latencies[count * 95 / 100];
        uint64_t p99 = latencies[count * 99 / 100];
        uint64_t avg = advanced->total_latency_ns.load() / count;

        printf("║                                                              ║\n");
        printf("║  🕐 LATENCY PERCENTILES (nanoseconds)                       ║\n");
        printf("║  ┌────────────────────────────────────────────────────────┐ ║\n");
        printf("║  │ Average Latency: %15lu ns        │ ║\n", avg);
        printf("║  │ P50 Latency:     %15lu ns        │ ║\n", p50);
        printf("║  │ P95 Latency:     %15lu ns        │ ║\n", p95);
        printf("║  │ P99 Latency:     %15lu ns        │ ║\n", p99);
        printf("║  │ Max Latency:     %15lu ns        │ ║\n", latencies.back());
        printf("║  └────────────────────────────────────────────────────────┘ ║\n");
    }

    void printMemoryStats() const
    {
        printf("║                                                              ║\n");
        printf("║  💾 MEMORY USAGE STATISTICS                                 ║\n");
        printf("║  ┌────────────────────────────────────────────────────────┐ ║\n");
        printf("║  │ Peak Memory:     %15s        │ ║\n", formatBytes(advanced->peak_memory_usage.load()).c_str());
        printf("║  │ Current Memory:  %15s        │ ║\n", formatBytes(advanced->current_memory_usage.load()).c_str());
        printf("║  │ Allocations:     %15s        │ ║\n", formatNumber(advanced->allocations.load()).c_str());
        printf("║  │ Deallocations:   %15s        │ ║\n", formatNumber(advanced->deallocations.load()).c_str());
        printf("║  └────────────────────────────────────────────────────────┘ ║\n");
    }

    void printCacheStats() const
    {
        uint64_t hits = advanced->cache_hits.load();
        uint64_t misses = advanced->cache_misses.load();
        double hit_rate = (hits + misses > 0) ? (double)hits / (hits + misses) * 100.0 : 0.0;

        printf("║                                                              ║\n");
        printf("║  🎯 CACHE PERFORMANCE                                       ║\n");
        printf("║  ┌────────────────────────────────────────────────────────┐ ║\n");
        printf("║  │ Cache Hits:      %15s        │ ║\n", formatNumber(hits).c_str());
        printf("║  │ Cache Misses:    %15s        │ ║\n", formatNumber(misses).c_str());
        printf("║  │ Hit Rate:        %15.2f%%       │ ║\n", hit_rate);
        printf("║  └────────────────────────────────────────────────────────┘ ║\n");
    }

    void printThreadStats() const
    {
        printf("║                                                              ║\n");
        printf("║  🧵 PER-THREAD PERFORMANCE                                  ║\n");
        printf("║  ┌────────────────────────────────────────────────────────┐ ║\n");
        for (size_t i = 0; i < advanced->thread_stats.size(); ++i)
        {
            const auto &ts = advanced->thread_stats[i];
            printf("║  │ Thread %zu:       %8s orders (%6s batches) │ ║\n",
                   i, formatNumber(ts.processed.load()).c_str(),
                   formatNumber(ts.batches.load()).c_str());
        }
        printf("║  └────────────────────────────────────────────────────────┘ ║\n");
    }
};
