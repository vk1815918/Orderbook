#pragma once
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <string>
#include <atomic>

// Helper function to add commas to large numbers
inline std::string formatNumber(uint64_t num) {
    if (num < 1000) return std::to_string(num);
    
    std::string str = std::to_string(num);
    int len = str.length();
    for (int i = len - 3; i > 0; i -= 3) {
        str.insert(i, ",");
    }
    return str;
}

struct Stats {
    std::atomic<uint64_t> generated{0};
    std::atomic<uint64_t> pushed{0};
    std::atomic<uint64_t> popped{0};
    std::atomic<uint64_t> rejected{0};   // engine rejects (e.g., out-of-range/IOC)
    std::atomic<uint64_t> donefill{0};   // fully filled takers
    std::atomic<uint64_t> resting{0};    // handles currently stored
    std::atomic<uint64_t> cancels{0};

    // timing
    std::chrono::high_resolution_clock::time_point t0, t1;

    void start() { t0 = std::chrono::high_resolution_clock::now(); }
    void stop()  { t1 = std::chrono::high_resolution_clock::now(); }

    void print(double extra_orders_per_sec = 0.0) const {
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
        if (extra_orders_per_sec > 0.0) {
            printf("║  │ Throughput:     %15.2f orders/sec │ ║\n", extra_orders_per_sec);
        }
        printf("║  │ Total Time:     %15.6f seconds    │ ║\n", secs);
        printf("║  └────────────────────────────────────────────────────────┘ ║\n");
        printf("╚══════════════════════════════════════════════════════════════╝\n");
        printf("\n");
    }
};
