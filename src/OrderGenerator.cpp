#include "OrderGenerator.hpp"
#include <cstring> // std::strcmp (if you later add CLI here)
#include <immintrin.h> // _mm_pause (optional)

template <typename T> static inline T clampT(T v, T lo, T hi){ return v<lo?lo:(v>hi?hi:v); }

OrderGenerator::OrderGenerator(AtomicRingBuffer<OrderMsg>& ring,
                               OrderManager& om,
                               const Config& cfg,
                               std::atomic<bool>& done_flag,
                               Stats& stats)
    : ring_(ring), om_(om), cfg_(cfg), done_(done_flag), stats_(stats) {}

void OrderGenerator::operator()() {
    std::mt19937_64 rng(cfg_.rng_seed);
    std::uniform_int_distribution<uint32_t> qty_dist(1, cfg_.max_qty);
    std::uniform_int_distribution<uint32_t> side_dist(0, 1);
    std::uniform_int_distribution<int32_t>  off_dist(-(int32_t)cfg_.span_ticks,
                                                      (int32_t)cfg_.span_ticks);

    const uint32_t mid = Config::MAX_TICKS / 2;

    // local counters (don't contend with consumer)
    uint64_t generated = 0, pushed = 0;
    uint64_t last_report = 0;

    // Pre-generate some orders to reduce per-iteration overhead
    std::vector<OrderMsg> order_batch;
    order_batch.reserve(1000); // Batch orders for better efficiency

    for (uint64_t i = 0; i < cfg_.num_orders; ++i) {
        const uint8_t  side = (uint8_t)side_dist(rng);
        const uint32_t qty  = qty_dist(rng);
        const int32_t  off  = off_dist(rng);
        uint32_t px = clampT<uint32_t>(mid + off, 1, Config::MAX_TICKS - 2);

        // Create order message directly (skip OrderManager for now to reduce overhead)
        OrderMsg msg{};
        msg.client_id  = i + 1;        // Use simple incrementing ID
        msg.price_tick = px;
        msg.qty        = qty;
        msg.side       = side;
        msg.flags      = 0;            // no IOC/FOK in the base run

        ++generated;

        // Try to push with minimal back-pressure
        uint32_t retry_count = 0;
        while (!ring_.push(msg)) {
            retry_count++;
            
            // Report progress every 500K orders (less frequent to reduce overhead)
            if (generated - last_report >= 500000) {
                printf("OrderGenerator: Generated %llu orders, pushed %llu (buffer may be full, retrying...)\n", 
                       (unsigned long long)generated, (unsigned long long)pushed);
                last_report = generated;
            }
            
            // Very mild backoff to reduce contention
            if (retry_count < 100) {
                _mm_pause();
            } else {
                // More aggressive backoff if we're really stuck
                std::this_thread::yield();
                retry_count = 0;
            }
        }
        ++pushed;
    }

    // Update final stats before finishing
    stats_.generated.store(generated, std::memory_order_release);
    stats_.pushed.store(pushed, std::memory_order_release);
    
    printf("OrderGenerator completed: Generated %llu, Pushed %llu orders\n", 
           (unsigned long long)generated, (unsigned long long)pushed);

    // let consumer know generation is complete
    done_.store(true, std::memory_order_release);
}
