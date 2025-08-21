#include "OrderGenerator.hpp"
#include <cstring>     // std::strcmp (if you later add CLI here)
#include <immintrin.h> // _mm_pause (optional)

template <typename T>
static inline T clampT(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

OrderGenerator::OrderGenerator(std::vector<AtomicRingBuffer<OrderMsg> *> &rings,
                               OrderManager &om,
                               const Config &cfg,
                               std::atomic<bool> &done_flag,
                               Stats &stats)
    : rings_(rings), om_(om), cfg_(cfg), done_(done_flag), stats_(stats) {}

void OrderGenerator::operator()()
{
    std::mt19937_64 rng(cfg_.rng_seed);
    std::uniform_int_distribution<uint32_t> qty_dist(1, cfg_.max_qty);
    std::uniform_int_distribution<uint32_t> side_dist(0, 1);
    std::uniform_int_distribution<int32_t> off_dist(-(int32_t)cfg_.span_ticks,
                                                    (int32_t)cfg_.span_ticks);

    const uint32_t mid = Config::MAX_TICKS / 2;

    // local counters (don't contend with consumer)
    uint64_t generated = 0, pushed = 0;
    uint64_t last_report = 0;

    // Track active orders for cancellation - store handles per worker
    std::vector<std::vector<uint32_t>> active_orders_per_worker(rings_.size());

    for (uint64_t i = 0; i < cfg_.num_orders; ++i)
    {
        const uint8_t side = (uint8_t)side_dist(rng);
        const uint32_t qty = qty_dist(rng);
        const int32_t off = off_dist(rng);
        uint32_t px = clampT<uint32_t>(mid + off, 1, Config::MAX_TICKS - 2);

        // Determine target worker using round-robin
        uint32_t target_worker = cur_worker_++;
        if (cur_worker_ >= rings_.size())
            cur_worker_ = 0;

        OrderMsg msg{};
        msg.worker_id = target_worker;

        // Decide whether to generate a cancel or new order
        bool should_cancel = (cfg_.cancel_every > 0) &&
                             (i % cfg_.cancel_every == 0) &&
                             (i > 0) &&
                             (!active_orders_per_worker[target_worker].empty());

        if (should_cancel)
        {
            // Generate a cancel message
            msg.msg_type = MessageType::CANCEL_ORDER;

            // Pick a random active order to cancel from this worker
            auto &worker_orders = active_orders_per_worker[target_worker];
            size_t idx = rng() % worker_orders.size();
            msg.handle_to_cancel = worker_orders[idx];

            // Remove from active list (swap with last element for O(1) removal)
            worker_orders[idx] = worker_orders.back();
            worker_orders.pop_back();

            // Set other fields (though they won't be used for cancel)
            msg.client_id = i + 1;
            msg.price_tick = px;
            msg.qty = qty;
            msg.side = side;
            msg.flags = 0;
        }
        else
        {
            // Generate a new order
            msg.msg_type = MessageType::ADD_ORDER;
            msg.client_id = i + 1;
            msg.price_tick = px;
            msg.qty = qty;
            msg.side = side;
            msg.flags = 0;
            msg.handle_to_cancel = 0; // Not used for add orders

            // We'll add the handle to active_orders when we get it back from the engine
            // For now, we'll use a synthetic handle based on order sequence
            uint32_t synthetic_handle = (uint32_t)(i + 1);
            active_orders_per_worker[target_worker].push_back(synthetic_handle);
        }

        ++generated;

        // Try to push with minimal back-pressure
        uint32_t retry_count = 0;
        AtomicRingBuffer<OrderMsg> *target = rings_[target_worker % rings_.size()];
        while (!target->push(msg))
        {
            retry_count++;

            // Report progress every 500K orders (less frequent to reduce overhead)
            if (generated - last_report >= 500000)
            {
                printf("OrderGenerator: Generated %llu orders, pushed %llu (buffer may be full, retrying...)\n",
                       (unsigned long long)generated, (unsigned long long)pushed);
                last_report = generated;
            }

            // Very mild backoff to reduce contention
            if (retry_count < 100)
            {
                _mm_pause();
            }
            else
            {
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
